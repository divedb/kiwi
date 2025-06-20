/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kiwi/concurrency/cache_locality.hh"

#ifndef _MSC_VER
#define _GNU_SOURCE 1  // for RTLD_NOLOAD
#include <dlfcn.h>
#endif

#include <fmt/core.h>
#include <folly/Indestructible.h>
#include <folly/Memory.h>
#include <folly/Optional.h>
#include <folly/ScopeGuard.h>
#include <folly/detail/StaticSingletonManager.h>
#include <folly/hash/Hash.h>
#include <folly/portability/Unistd.h>

#include <fstream>
#include <mutex>

#include "kiwi/common/exception.hh"
#include "kiwi/portability/build_config.hh"

namespace kiwi {

/// Returns the CacheLocality information best for this machine
static CacheLocality GetSystemLocalityInfo() {
  if (BUILDFLAG(IS_LINUX)) {
    try {
      return CacheLocality::ReadFromProcCPUInfo();
    } catch (...) {
      // keep trying
    }
  }

  long numCpus = sysconf(_SC_NPROCESSORS_CONF);

  if (numCpus <= 0) {
    // This shouldn't happen, but if it does we should try to keep
    // going.  We are probably not going to be able to parse /sys on
    // this box either (although we will try), which means we are going
    // to fall back to the SequentialThreadId splitter.  On my 16 core
    // (x hyperthreading) dev box 16 stripes is enough to get pretty good
    // contention avoidance with SequentialThreadId, and there is little
    // improvement from going from 32 to 64.  This default gives us some
    // wiggle room
    numCpus = 32;
  }

  return CacheLocality::Uniform(size_t(numCpus));
}

template <>
const CacheLocality& CacheLocality::system<std::atomic>() {
  static std::atomic<const CacheLocality*> cache;
  auto value = cache.load(std::memory_order_acquire);
  if (value != nullptr) {
    return *value;
  }
  auto next = new CacheLocality(getSystemLocalityInfo());
  if (cache.compare_exchange_strong(value, next, std::memory_order_acq_rel)) {
    return *next;
  }
  delete next;
  return *value;
}

/// Each level of cache has sharing sets, which are the set of cpus
/// that share a common cache at that level.  These are available in a
/// hex bitset form (/sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_map,
/// for example).  They are also available in a human-readable list form,
/// as in /sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list.  The list
/// is a comma-separated list of numbers and ranges, where the ranges are
/// a pair of decimal numbers separated by a '-'.
///
/// To sort the cpus for optimum locality we don't really need to parse
/// the sharing sets, we just need a unique representative from the
/// equivalence class.  The smallest value works fine, and happens to be
/// the first decimal number in the file.  We load all of the equivalence
/// class information from all of the cpu*/index* directories, order the
/// cpus first by increasing last-level cache equivalence class, then by
/// the smaller caches.  Finally, we break ties with the cpu number itself.
///
/// Returns the first decimal number in the string, or throws an exception
/// if the string does not start with a number terminated by ',', '-',
/// '\n', or eos.
static size_t ParseLeadingNumber(const std::string& line) {
  char* end;
  auto raw = line.c_str();
  unsigned long val = strtoul(raw, &end, 10);

  if (end == raw || (*end != ',' && *end != '-' && *end != '\n' && *end != 0)) {
    throw std::runtime_error(fmt::format("error parsing list '{}'", line));
  }

  return val;
}

CacheLocality CacheLocality::ReadFromSysfsTree(
    const std::function<std::string(std::string)>& mapping) {
  // The number of equivalence classes per level.
  std::vector<size_t> num_caches_by_level;

  // The list of cache equivalence classes, where equivalance classes
  // are named by the smallest cpu in the class
  std::vector<std::vector<size_t>> equiv_classes_by_cpu;

  std::vector<size_t> cpus;

  while (true) {
    auto cpu = cpus.size();
    std::vector<size_t> levels;

    for (size_t index = 0;; ++index) {
      auto dir = fmt::format("/sys/devices/system/cpu/cpu{}/cache/index{}/",
                             cpu, index);
      auto cache_type = mapping(dir + "type");
      auto equiv_str = mapping(dir + "shared_cpu_list");

      // No more caches.
      if (cache_type.empty() || equiv_str.empty()) {
        break;
      }

      // The cache_type in { "Data", "Instruction", "Unified" }. Skip icache.
      if (cache_type[0] == 'I') {
        continue;
      }

      auto equiv = ParseLeadingNumber(equiv_str);
      auto level = levels.size();

      levels.push_back(equiv);

      if (equiv == cpu) {
        // we only want to count the equiv classes once, so we do it when
        // we first encounter them
        while (num_caches_by_level.size() <= level) {
          num_caches_by_level.push_back(0);
        }

        num_caches_by_level[level]++;
      }
    }

    if (levels.empty()) {
      // No levels at all for this cpu, we must be done.
      break;
    }

    equiv_classes_by_cpu.emplace_back(std::move(levels));
    cpus.push_back(cpu);
  }

  if (cpus.empty()) {
    throw std::runtime_error("Unable to load cache sharing info");
  }

  std::sort(cpus.begin(), cpus.end(), [&](size_t lhs, size_t rhs) -> bool {
    // sort first by equiv class of cache with highest index,
    // direction doesn't matter.  If different cpus have
    // different numbers of caches then this code might produce
    // a sub-optimal ordering, but it won't crash
    auto& lhsEquiv = equivClassesByCpu[lhs];
    auto& rhsEquiv = equivClassesByCpu[rhs];
    for (ssize_t i = ssize_t(std::min(lhsEquiv.size(), rhsEquiv.size())) - 1;
         i >= 0; --i) {
      auto idx = size_t(i);
      if (lhsEquiv[idx] != rhsEquiv[idx]) {
        return lhsEquiv[idx] < rhsEquiv[idx];
      }
    }

    // break ties deterministically by cpu
    return lhs < rhs;
  });

  // the cpus are now sorted by locality, with neighboring entries closer
  // to each other than entries that are far away.  For striping we want
  // the inverse map, since we are starting with the cpu
  std::vector<size_t> indexes(cpus.size());

  for (size_t i = 0; i < cpus.size(); ++i) {
    indexes[cpus[i]] = i;
  }

  return CacheLocality{cpus.size(), std::move(numCachesByLevel),
                       std::move(indexes)};
}

CacheLocality CacheLocality::readFromSysfs() {
  return readFromSysfsTree([](std::string name) {
    std::ifstream xi(name.c_str());
    std::string rv;
    std::getline(xi, rv);
    return rv;
  });
}

static bool procCpuinfoLineRelevant(std::string const& line) {
  return line.size() > 4 && (line[0] == 'p' || line[0] == 'c');
}

CacheLocality CacheLocality::readFromProcCpuinfoLines(
    std::vector<std::string> const& lines) {
  size_t physicalId = 0;
  size_t coreId = 0;
  std::vector<std::tuple<size_t, size_t, size_t>> cpus;
  size_t maxCpu = 0;
  for (auto iter = lines.rbegin(); iter != lines.rend(); ++iter) {
    auto& line = *iter;
    if (!procCpuinfoLineRelevant(line)) {
      continue;
    }

    auto sepIndex = line.find(':');
    if (sepIndex == std::string::npos || sepIndex + 2 > line.size()) {
      continue;
    }
    auto arg = line.substr(sepIndex + 2);

    // "physical id" is socket, which is the most important locality
    // context.  "core id" is a real core, so two "processor" entries with
    // the same physical id and core id are hyperthreads of each other.
    // "processor" is the top line of each record, so when we hit it in
    // the reverse order then we can emit a record.
    if (line.find("physical id") == 0) {
      physicalId = parseLeadingNumber(arg);
    } else if (line.find("core id") == 0) {
      coreId = parseLeadingNumber(arg);
    } else if (line.find("processor") == 0) {
      auto cpu = parseLeadingNumber(arg);
      maxCpu = std::max(cpu, maxCpu);
      cpus.emplace_back(physicalId, coreId, cpu);
    }
  }

  if (cpus.empty()) {
    throw std::runtime_error("no CPUs parsed from /proc/cpuinfo");
  }
  if (maxCpu != cpus.size() - 1) {
    throw std::runtime_error(
        "offline CPUs not supported for /proc/cpuinfo cache locality source");
  }

  std::sort(cpus.begin(), cpus.end());
  size_t cpusPerCore = 1;
  while (cpusPerCore < cpus.size() &&
         std::get<0>(cpus[cpusPerCore]) == std::get<0>(cpus[0]) &&
         std::get<1>(cpus[cpusPerCore]) == std::get<1>(cpus[0])) {
    ++cpusPerCore;
  }

  // we can't tell the real cache hierarchy from /proc/cpuinfo, but it
  // works well enough to assume there are 3 levels, L1 and L2 per-core
  // and L3 per socket
  std::vector<size_t> numCachesByLevel;
  numCachesByLevel.push_back(cpus.size() / cpusPerCore);
  numCachesByLevel.push_back(cpus.size() / cpusPerCore);
  numCachesByLevel.push_back(std::get<0>(cpus.back()) + 1);

  std::vector<size_t> indexes(cpus.size());
  for (size_t i = 0; i < cpus.size(); ++i) {
    indexes[std::get<2>(cpus[i])] = i;
  }

  return CacheLocality{cpus.size(), std::move(numCachesByLevel),
                       std::move(indexes)};
}

CacheLocality CacheLocality::readFromProcCpuinfo() {
  std::vector<std::string> lines;
  {
    std::ifstream xi("/proc/cpuinfo");
    if (xi.fail()) {
      throw std::runtime_error("unable to open /proc/cpuinfo");
    }
    char buf[8192];
    while (xi.good() && lines.size() < 20000) {
      xi.getline(buf, sizeof(buf));
      std::string str(buf);
      if (procCpuinfoLineRelevant(str)) {
        lines.emplace_back(std::move(str));
      }
    }
  }
  return readFromProcCpuinfoLines(lines);
}

CacheLocality CacheLocality::uniform(size_t numCpus) {
  CacheLocality rv;

  rv.numCpus = numCpus;

  // one cache shared by all cpus
  rv.numCachesByLevel.push_back(numCpus);

  // no permutations in locality index mapping
  for (size_t cpu = 0; cpu < numCpus; ++cpu) {
    rv.localityIndexByCpu.push_back(cpu);
  }

  return rv;
}

////////////// Getcpu

Getcpu::Func Getcpu::resolveVdsoFunc() {
#if !defined(FOLLY_HAVE_LINUX_VDSO) || defined(FOLLY_SANITIZE_MEMORY)
  return nullptr;
#else
  void* h = dlopen("linux-vdso.so.1", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
  if (h == nullptr) {
    return nullptr;
  }

  auto func = Getcpu::Func(dlsym(h, "__vdso_getcpu"));
  if (func == nullptr) {
    // technically a null result could either be a failure or a successful
    // lookup of a symbol with the null value, but the second can't actually
    // happen for this symbol.  No point holding the handle forever if
    // we don't need the code
    dlclose(h);
  }

  return func;
#endif
}

/////////////// SequentialThreadId
unsigned SequentialThreadId::get() {
  static std::atomic<unsigned> global{0};
  static thread_local unsigned local{0};
  return FOLLY_LIKELY(local) ? local : (local = ++global);
}

/////////////// HashingThreadId
unsigned HashingThreadId::get() {
  return hash::twang_32from64(getCurrentThreadID());
}

namespace detail {

int AccessSpreaderBase::degenerateGetcpu(unsigned* cpu, unsigned* node, void*) {
  if (cpu != nullptr) {
    *cpu = 0;
  }
  if (node != nullptr) {
    *node = 0;
  }
  return 0;
}

struct AccessSpreaderStaticInit {
  static AccessSpreaderStaticInit instance;
  AccessSpreaderStaticInit() { (void)AccessSpreader<>::current(~size_t(0)); }
};
AccessSpreaderStaticInit AccessSpreaderStaticInit::instance;

bool AccessSpreaderBase::initialize(GlobalState& state,
                                    Getcpu::Func (&pickGetcpuFunc)(),
                                    const CacheLocality& (&system)()) {
  (void)AccessSpreaderStaticInit::instance;  // ODR-use it so it is not dropped
  constexpr auto relaxed = std::memory_order_relaxed;
  auto& cacheLocality = system();
  auto n = cacheLocality.numCpus;
  for (size_t width = 0; width <= kMaxCpus; ++width) {
    auto& row = state.table[width];
    auto numStripes = std::max(size_t{1}, width);
    for (size_t cpu = 0; cpu < kMaxCpus && cpu < n; ++cpu) {
      auto index = cacheLocality.localityIndexByCpu[cpu];
      assert(index < n);
      // as index goes from 0..n, post-transform value goes from
      // 0..numStripes
      make_atomic_ref(row[cpu]).store(
          static_cast<CompactStripe>((index * numStripes) / n), relaxed);
      assert(make_atomic_ref(row[cpu]).load(relaxed) < numStripes);
    }
    size_t filled = n;
    while (filled < kMaxCpus) {
      size_t len = std::min(filled, kMaxCpus - filled);
      for (size_t i = 0; i < len; ++i) {
        make_atomic_ref(row[filled + i])
            .store(make_atomic_ref(row[i]).load(relaxed), relaxed);
      }
      filled += len;
    }
    for (size_t cpu = n; cpu < kMaxCpus; ++cpu) {
      assert(make_atomic_ref(row[cpu]).load(relaxed) ==
             make_atomic_ref(row[cpu - n]).load(relaxed));
    }
  }
  state.getcpu.exchange(pickGetcpuFunc(), std::memory_order_acq_rel);

  return true;
}

}  // namespace detail

namespace {

/// A simple freelist allocator.  Allocates things of size sz, from slabs of
/// size kAllocSize.  Takes a lock on each allocation/deallocation.
class SimpleAllocator {
 public:
  // To support array aggregate initialization without an implicit constructor.
  struct Ctor {};

  SimpleAllocator(Ctor, size_t sz) : sz_(sz) {}
  ~SimpleAllocator() {
    std::lock_guard<std::mutex> g(m_);
    for (auto& block : blocks_) {
      folly::aligned_free(block);
    }
  }

  void* allocate() {
    std::lock_guard<std::mutex> g(m_);
    // Freelist allocation.
    if (freelist_) {
      auto mem = freelist_;
      freelist_ = *static_cast<void**>(freelist_);
      return mem;
    }

    if (mem_) {
      // Bump-ptr allocation.
      if (intptr_t(mem_) % kMallocAlign == 0) {
        // Avoid allocating pointers that may look like malloc
        // pointers.
        mem_ += std::min(sz_, max_align_v);
      }
      if (mem_ + sz_ <= end_) {
        auto mem = mem_;
        mem_ += sz_;

        assert(intptr_t(mem) % kMallocAlign != 0);
        return mem;
      }
    }

    return allocateHard();
  }

  static void deallocate(void* ptr) {
    assert(intptr_t(ptr) % kMallocAlign != 0);
    // Find the allocator instance.
    auto addr =
        reinterpret_cast<void*>(intptr_t(ptr) & ~intptr_t(kAllocSize - 1));
    auto allocator = *static_cast<SimpleAllocator**>(addr);

    std::lock_guard<std::mutex> g(allocator->m_);
    *static_cast<void**>(ptr) = allocator->freelist_;
    if constexpr (kIsSanitizeAddress) {
      // If running under ASAN, scrub the memory on deallocation, so we don't
      // leave pointers that could hide leaks at shutdown, since the backing
      // slabs may not be deallocated if the instance is a leaky singleton.
      auto* base = static_cast<char*>(ptr);
      std::fill(base + sizeof(void*), base + allocator->sz_,
                static_cast<char>(0));
    }
    allocator->freelist_ = ptr;
  }

  constexpr static size_t kMallocAlign = 128;
  static_assert(kMallocAlign % hardware_destructive_interference_size == 0,
                "Large allocations should be cacheline-aligned");

 private:
  constexpr static size_t kAllocSize = 4096;

  void* allocateHard() {
    // Allocate a new slab.
    mem_ = static_cast<uint8_t*>(folly::aligned_malloc(kAllocSize, kAllocSize));
    if (!mem_) {
      throw_exception<std::bad_alloc>();
    }
    end_ = mem_ + kAllocSize;
    blocks_.push_back(mem_);

    // Install a pointer to ourselves as the allocator.
    *reinterpret_cast<SimpleAllocator**>(mem_) = this;
    static_assert(max_align_v >= sizeof(SimpleAllocator*),
                  "alignment too small");
    mem_ += std::min(sz_, max_align_v);

    // New allocation.
    auto mem = mem_;
    mem_ += sz_;
    assert(intptr_t(mem) % kMallocAlign != 0);
    return mem;
  }

  std::mutex m_;
  uint8_t* mem_{nullptr};
  uint8_t* end_{nullptr};
  void* freelist_{nullptr};
  size_t sz_;
  std::vector<void*> blocks_;
};

class Allocator {
 public:
  void* allocate(size_t size) {
    if (auto cl = sizeClass(size)) {
      return allocators_[*cl].allocate();
    }

    // Fall back to malloc, returning a kMallocAlign-aligned allocation so it
    // can be distinguished from SimpleAllocator allocations.
    size = size + (SimpleAllocator::kMallocAlign - 1);
    size &= ~size_t(SimpleAllocator::kMallocAlign - 1);
    void* mem = aligned_malloc(size, SimpleAllocator::kMallocAlign);
    if (!mem) {
      throw_exception<std::bad_alloc>();
    }
    return mem;
  }

  static void deallocate(void* ptr) {
    if (!ptr) {
      return;
    }

    // See if it came from SimpleAllocator or malloc.
    if (intptr_t(ptr) % SimpleAllocator::kMallocAlign != 0) {
      SimpleAllocator::deallocate(ptr);
    } else {
      aligned_free(ptr);
    }
  }

 private:
  folly::Optional<uint8_t> sizeClass(size_t size) {
    if (size <= 8) {
      return 0;
    } else if (size <= 16) {
      return 1;
    } else if (size <= 32) {
      return 2;
    } else if (size <= 64) {
      return 3;
    } else {
      return folly::none;
    }
  }

  std::array<SimpleAllocator, 4> allocators_{{{SimpleAllocator::Ctor{}, 8},
                                              {SimpleAllocator::Ctor{}, 16},
                                              {SimpleAllocator::Ctor{}, 32},
                                              {SimpleAllocator::Ctor{}, 64}}};
};

}  // namespace

void* coreMalloc(size_t size, size_t numStripes, size_t stripe) {
  static folly::Indestructible<Allocator>
      allocators[AccessSpreader<>::maxLocalityIndexValue()];

  auto index = AccessSpreader<>::localityIndexForStripe(numStripes, stripe);
  return allocators[index]->allocate(size);
}

void coreFree(void* ptr) { Allocator::deallocate(ptr); }

namespace {
thread_local CoreAllocatorGuard* gCoreAllocatorGuard = nullptr;
}

CoreAllocatorGuard::CoreAllocatorGuard(size_t numStripes, size_t stripe)
    : numStripes_(numStripes), stripe_(stripe) {
  CHECK(gCoreAllocatorGuard == nullptr)
      << "CoreAllocator::Guard cannot be used recursively";
  gCoreAllocatorGuard = this;
}

CoreAllocatorGuard::~CoreAllocatorGuard() { gCoreAllocatorGuard = nullptr; }

namespace detail {

void* coreMallocFromGuard(size_t size) {
  CHECK(gCoreAllocatorGuard != nullptr)
      << "CoreAllocator::allocator called without an active Guard";
  return coreMalloc(size, gCoreAllocatorGuard->numStripes_,
                    gCoreAllocatorGuard->stripe_);
}

}  // namespace detail

}  // namespace kiwi
