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

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "kiwi/io/iobuf.hh"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include "kiwi/common/malloc.hh"
#include "kiwi/common/scope_exit.hh"

// Callbacks that will be invoked when IOBuf allocates or frees memory.
// Note that io_buf_alloc_cb() will also be invoked when IOBuf takes ownership
// of a malloc-allocated buffer, even if it was allocated earlier by another
// part of the code.
//
// By default these are unimplemented, but programs can define these functions
// to perform their own custom logic on memory allocation.  This is intended
// primarily to help programs track memory usage and possibly take action
// when thresholds are hit.  Callers should generally avoid performing any
// expensive work in these callbacks, since they may be called from arbitrary
// locations in the code that use IOBuf, possibly while holding locks.
#if KIWI_HAVE_WEAK_SYMBOLS
KIWI_ATTR_WEAK void io_buf_alloc_cb(void* /*ptr*/, size_t /*size*/) noexcept;
KIWI_ATTR_WEAK void io_buf_free_cb(void* /*ptr*/, size_t /*size*/) noexcept;
#else
static void (*io_buf_alloc_cb)(void* /*ptr*/,
                               size_t /*size*/) noexcept = nullptr;
static void (*io_buf_free_cb)(void* /*ptr*/,
                              size_t /*size*/) noexcept = nullptr;
#endif

using std::unique_ptr;

namespace {

enum : uint16_t {
  kHeapMagic = 0xa5a5,
  // This memory segment contains an IOBuf that is still in use.
  kIOBufInUse = 0x01,
  // This memory segment contains buffer data that is still in use.
  kDataInUse = 0x02,
  // This memory segment contains a SharedInfo that is still in use.
  kSharedInfoInUse = 0x04,
};

enum : std::size_t {
  // When Create() is called for buffers less than kDefaultCombinedBufSize,
  // we allocate a single combined memory segment for the IOBuf and the data
  // together.  See the comments for CreateCombined()/CreateSeparate() for more
  // details.
  //
  // (The size of 1k is largely just a guess here.  We could could probably do
  // benchmarks of real applications to see if adjusting this number makes a
  // difference.  Callers that know their exact use case can also explicitly
  // call CreateCombined() or CreateSeparate().)
  kDefaultCombinedBufSize = 1024,
  kMaxIOBufSize = std::numeric_limits<size_t>::max() >> 1,
};

// Helper function for IOBuf::TakeOwnership().
// The user's free function is not allowed to throw.
// (We are already in the middle of throwing an exception, so
// we cannot let this exception go unhandled.)
void take_ownership_error(bool free_on_error, void* buf,
                          kiwi::IOBuf::FreeFunction free_fn,
                          void* user_data) noexcept {
  if (!free_on_error) {
    return;
  }

  if (!free_fn) {
    free(buf);
    return;
  }

  free_fn(buf, user_data);
}

}  // namespace

namespace kiwi {

/// Use free for size >= 4GB since we can store only 32 bits in the size var.
struct IOBuf::HeapPrefix {
  HeapPrefix(uint16_t flg, size_t sz)
      : magic(kHeapMagic),
        flags(flg),
        size((sz == ((size_t)(uint32_t)sz)) ? static_cast<uint32_t>(sz) : 0) {}
  ~HeapPrefix() {
    // Reset magic to 0 on destruction.  This is solely for debugging purposes
    // to help catch bugs where someone tries to use HeapStorage after it has
    // been deleted.
    magic = 0;
  }

  uint16_t magic;
  std::atomic<uint16_t> flags;
  uint32_t size;
};

struct IOBuf::HeapStorage {
  HeapPrefix prefix;
  // The IOBuf is last in the HeapStorage object.
  // This way operator new will work even if allocating a subclass of IOBuf
  // that requires more space.
  kiwi::IOBuf buf;
};

struct alignas(std::max_align_t) IOBuf::HeapFullStorage {
  // Make sure jemalloc allocates from the 64-byte class.  Putting this here
  // because HeapStorage is private so it can't be at namespace level.
  static_assert(sizeof(HeapStorage) <= 64, "IOBuf may not grow over 56 bytes!");

  HeapStorage hs;
  SharedInfo shared;
};

IOBuf::SharedInfo::SharedInfo()
    : free_fn(nullptr), user_data(nullptr), use_heap_full_storage(false) {
  // Use relaxed memory ordering here.  Since we are creating a new SharedInfo,
  // no other threads should be referring to it yet.
  refcount.store(1, std::memory_order_relaxed);
}

IOBuf::SharedInfo::SharedInfo(FreeFunction fn, void* arg, bool hfs)
    : free_fn(fn), user_data(arg), use_heap_full_storage(hfs) {
  // Use relaxed memory ordering here.  Since we are creating a new SharedInfo,
  // no other threads should be referring to it yet.
  refcount.store(1, std::memory_order_relaxed);
}

void IOBuf::SharedInfo::InvokeAndDeleteEachObserver(
    SharedInfoObserverEntryBase* observer_list_head, ObserverCb cb) noexcept {
  if (observer_list_head && cb) {
    // Break the chain.
    observer_list_head->prev->next = nullptr;
    auto entry = observer_list_head;

    while (entry) {
      auto tmp = entry->next;
      cb(*entry);
      delete entry;
      entry = tmp;
    }
  }
}

void IOBuf::SharedInfo::ReleaseStorage(SharedInfo* info) noexcept {
  if (info->use_heap_full_storage) {
    auto storage_addr =
        reinterpret_cast<uint8_t*>(info) - offsetof(HeapFullStorage, shared);
    auto storage = reinterpret_cast<HeapFullStorage*>(storage_addr);
    info->~SharedInfo();
    IOBuf::ReleaseStorage(&storage->hs, kSharedInfoInUse);
  }
}

void* IOBuf::operator new(size_t size) {
  if (size > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  size_t full_size = offsetof(HeapStorage, buf) + size;
  auto storage = static_cast<HeapStorage*>(checked_malloc(full_size));
  new (&storage->prefix) HeapPrefix(kIOBufInUse, full_size);

  if (io_buf_alloc_cb) {
    io_buf_alloc_cb(storage, full_size);
  }

  return &(storage->buf);
}

void* IOBuf::operator new(size_t /* size */, void* ptr) { return ptr; }

void IOBuf::operator delete(void* ptr) {
  auto storage_addr = static_cast<uint8_t*>(ptr) - offsetof(HeapStorage, buf);
  auto storage = reinterpret_cast<HeapStorage*>(storage_addr);

  ReleaseStorage(storage, kIOBufInUse);
}

void IOBuf::operator delete(void* /* ptr */, void* /* placement */) {
  // Provide matching operator for `IOBuf::new` to avoid MSVC compilation
  // warning (C4291) about memory leak when exception is thrown in the
  // constructor.
}

void IOBuf::ReleaseStorage(HeapStorage* storage, uint16_t free_flags) noexcept {
  CHECK_EQ(storage->prefix.magic, static_cast<uint16_t>(kHeapMagic));

  // This function is effectively used as a memory barrier.  Logically, we can
  // use relaxed memory order here.  If we are unlucky and happen to get
  // out-of-date data the compare_exchange_weak() call below will catch it and
  // load new data with memory_order_acq_rel.  However, changing this to be
  // std::memory_order_relaxed will cause tsan to find problems with some
  // caller code.
  auto flags = storage->prefix.flags.load(std::memory_order_acquire);
  DCHECK_EQ((flags & freeFlags), freeFlags);

  while (true) {
    auto new_flags = uint16_t(flags & ~free_flags);

    if (new_flags == 0) {
      // Save the size.
      size_t size = storage->prefix.size;
      // The storage space is now unused.  Free it.
      storage->prefix.HeapPrefix::~HeapPrefix();

      if (size) [[likely]] {
        if (io_buf_free_cb) {
          io_buf_free_cb(storage, size);
        }
      }

      free(storage);

      return;
    }

    // This storage segment still contains portions that are in use.
    // Just clear the flags specified in freeFlags for now.
    auto ret = storage->prefix.flags.compare_exchange_weak(
        flags, newFlags, std::memory_order_acq_rel);

    if (ret) {
      // We successfully updated the flags.
      return;
    }

    // We failed to update the flags.  Some other thread probably updated them
    // and cleared some of the other bits.  Continue around the loop to see if
    // we are the last user now, or if we need to try updating the flags again.
  }
}

void IOBuf::FreeInternalBuf(void* /* buf */, void* user_data) noexcept {
  auto storage = static_cast<HeapStorage*>(user_data);

  ReleaseStorage(storage, kDataInUse);
}

IOBuf::IOBuf(CreateOp, std::size_t capacity)
    : length_(0),
      data_(nullptr),
      next_(this),
      prev_(this),
      flags_and_shared_info_(0) {
  SharedInfo* info;
  AllocExtBuffer(capacity, &buf_, &info, &capacity_);
  SetSharedInfo(info);
  data_ = buf_;
}

IOBuf::IOBuf(CopyBufferOp /* op */, const void* buf, std::size_t size,
             std::size_t head_room, std::size_t min_tail_room)
    : length_(0),
      data_(nullptr),
      next_(this),
      prev_(this),
      flags_and_shared_info_(0) {
  std::size_t capacity = 0;

  if (!checked_add(&capacity, size, head_room, min_tail_room) ||
      capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  SharedInfo* info;
  AllocExtBuffer(capacity, &buf_, &info, &capacity_);
  SetSharedInfo(info);
  data_ = buf_;

  Advance(head_room);

  if (size > 0) {
    assert(buf != nullptr);

    memcpy(WritableData(), buf, size);
    Append(size);
  }
}

IOBuf::IOBuf(CopyBufferOp op, ByteRange br, std::size_t head_room,
             std::size_t min_tail_room)
    : IOBuf(op, br.data(), br.size(), head_room, min_tail_room) {}

unique_ptr<IOBuf> IOBuf::Create(std::size_t capacity) {
  if (capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  // For smaller-sized buffers, allocate the IOBuf, SharedInfo, and the buffer
  // all with a single allocation.
  //
  // We don't do this for larger buffers since it can be wasteful if the user
  // needs to reallocate the buffer but keeps using the same IOBuf object.
  // In this case we can't free the data space until the IOBuf is also
  // destroyed.  Callers can explicitly call CreateCombined() or
  // CreateSeparate() if they know their use case better, and know if they are
  // likely to reallocate the buffer later.
  if (capacity <= kDefaultCombinedBufSize) {
    return CreateCombined(capacity);
  }

  return CreateSeparate(capacity);
}

unique_ptr<IOBuf> IOBuf::CreateCombined(std::size_t capacity) {
  if (capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  // To save a memory allocation, allocate space for the IOBuf object, the
  // SharedInfo struct, and the data itself all with a single call to malloc().
  size_t required_storage = sizeof(HeapFullStorage) + capacity;
  size_t malloc_size = good_malloc_size(required_storage);
  auto storage = static_cast<HeapFullStorage*>(checked_malloc(malloc_size));

  new (&storage->hs.prefix) HeapPrefix(kIOBufInUse | kDataInUse, malloc_size);
  new (&storage->shared) SharedInfo(FreeInternalBuf, storage);

  if (io_buf_alloc_cb) {
    io_buf_alloc_cb(storage, malloc_size);
  }

  auto buf_addr = reinterpret_cast<uint8_t*>(storage) + sizeof(HeapFullStorage);
  uint8_t* storage_end = reinterpret_cast<uint8_t*>(storage) + malloc_size;
  auto actual_capacity = size_t(storage_end - buf_addr);
  unique_ptr<IOBuf> ret(new (&storage->hs.buf) IOBuf(
      InternalConstructor(), PackFlagsAndSharedInfo(0, &storage->shared),
      buf_addr, actual_capacity, buf_addr, 0));

  return ret;
}

unique_ptr<IOBuf> IOBuf::CreateSeparate(std::size_t capacity) {
  return std::make_unique<IOBuf>(kCreate, capacity);
}

unique_ptr<IOBuf> IOBuf::CreateChain(size_t total_capacity,
                                     std::size_t max_buf_capacity) {
  unique_ptr<IOBuf> out =
      Create(std::min(total_capacity, size_t(max_buf_capacity)));
  size_t allocated_capacity = out->capacity();

  while (allocated_capacity < totalCapacity) {
    unique_ptr<IOBuf> new_buf = Create(std::min(
        total_capacity - allocated_capacity, size_t(max_buf_capacity)));
    allocated_capacity += new_buf->capacity();
    out->AppendToChain(std::move(newBuf));
  }

  return out;
}

size_t IOBuf::GoodSize(size_t min_capacity, CombinedOption combined) {
  if (combined == CombinedOption::kDefault) {
    combined = min_capacity <= kDefaultCombinedBufSize
                   ? CombinedOption::kCombined
                   : CombinedOption::kSeparate;
  }

  size_t overhead;

  if (combined == CombinedOption::kCombined) {
    overhead = sizeof(HeapFullStorage);
  } else {
    // Pad minCapacity to a multiple of 8
    min_capacity = (min_capacity + 7) & ~7;
    overhead = sizeof(SharedInfo);
  }

  size_t good_size = kiwi::good_malloc_size(min_capacity + overhead);

  return good_size - overhead;
}

IOBuf::IOBuf(TakeOwnershipOp, void* buf, std::size_t capacity,
             std::size_t offset, std::size_t length, FreeFunction free_fn,
             void* user_data, bool free_on_error)
    : length_(length),
      data_(static_cast<uint8_t*>(buf) + offset),
      capacity_(capacity),
      buf_(static_cast<uint8_t*>(buf)),
      next_(this),
      prev_(this),
      flags_and_shared_info_(
          PackFlagsAndSharedInfo(kFlagFreeSharedInfo, nullptr)) {
  // do not allow only user data without a freeFn
  // since we use that for folly::sizedFree
  DCHECK(!user_data || (user_data && free_fn));

  auto rollback = make_scope_exit(
      [&] { TakeOwnershipError(free_on_error, buf, free_fn, user_data); });
  setSharedInfo(new SharedInfo(free_fn, user_data));
  rollback.Dismiss();
}

IOBuf::IOBuf(TakeOwnershipOp, SizedFree, void* buf, std::size_t capacity,
             std::size_t offset, std::size_t length, bool free_on_error)
    : length_(length),
      data_(static_cast<uint8_t*>(buf) + offset),
      capacity_(capacity),
      buf_(static_cast<uint8_t*>(buf)),
      next_(this),
      prev_(this),
      flags_and_shared_info_(
          PackFlagsAndSharedInfo(kFlagFreeSharedInfo, nullptr)) {
  auto rollback = make_scope_exit(
      [&] { TakeOwnershipError(free_on_error, buf, nullptr, nullptr); });
  SetSharedInfo(new SharedInfo(nullptr, reinterpret_cast<void*>(capacity)));
  rollback.Dismiss();

  if (io_buf_alloc_cb && capacity) {
    io_buf_alloc_cb(buf, capacity);
  }
}

unique_ptr<IOBuf> IOBuf::TakeOwnership(void* buf, std::size_t capacity,
                                       std::size_t offset, std::size_t length,
                                       FreeFunction free_fn, void* user_data,
                                       bool free_on_error,
                                       TakeOwnershipOption option) {
  // do not allow only user data without a freeFn
  // since we use that for folly::sizedFree
  DCHECK(
      !user_data || (user_data && free_fn) ||
      (user_data && !free_fn && (option == TakeOwnershipOption::kStoreSize)));

  HeapFullStorage* storage = nullptr;

  auto rollback = make_scope_exit([&] {
    if (storage) {
      free(storage);
    }

    TakeOwnershipError(free_on_error, buf, free_fn, user_data);
  });

  if (capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  size_t required_storage = sizeof(HeapFullStorage);
  size_t malloc_size = good_malloc_size(required_storage);
  storage = static_cast<HeapFullStorage*>(checked_malloc(malloc_size));

  new (&storage->hs.prefix)
      HeapPrefix(kIOBufInUse | kSharedInfoInUse, malloc_size);
  new (&storage->shared)
      SharedInfo(free_fn, user_data, true /*useHeapFullStorage*/);

  auto result = unique_ptr<IOBuf>(new (&storage->hs.buf) IOBuf(
      InternalConstructor(), PackFlagsAndSharedInfo(0, &storage->shared),
      static_cast<uint8_t*>(buf), capacity, static_cast<uint8_t*>(buf) + offset,
      length));

  rollback.Dismiss();

  if (io_buf_alloc_cb) {
    io_buf_alloc_cb(storage, malloc_size);

    if (user_data && !free_fn && (option == TakeOwnershipOption::kStoreSize)) {
      // Even though we did not allocate the buffer, call io_buf_alloc_cb()
      // since we will call io_buf_free_cb() on destruction, and we want these
      // calls to be 1:1.
      io_buf_alloc_cb(buf, capacity);
    }
  }

  return result;
}

IOBuf::IOBuf(WrapBufferOp, const void* buf, std::size_t capacity) noexcept
    : IOBuf(InternalConstructor(), 0,
            // We cast away the const-ness of the buffer here.
            // This is okay since IOBuf users must use unshare() to create a
            // copy of this buffer before writing to the buffer.
            static_cast<uint8_t*>(const_cast<void*>(buf)), capacity,
            static_cast<uint8_t*>(const_cast<void*>(buf)), capacity) {}

IOBuf::IOBuf(WrapBufferOp op, ByteRange br) noexcept
    : IOBuf(op, br.data(), br.size()) {}

unique_ptr<IOBuf> IOBuf::WrapBuffer(const void* buf, std::size_t capacity) {
  return std::make_unique<IOBuf>(kWrapBuffer, buf, capacity);
}

IOBuf IOBuf::WrapBufferAsValue(const void* buf, std::size_t capacity) noexcept {
  return IOBuf(WrapBufferOp::kWrapBuffer, buf, capacity);
}

IOBuf::IOBuf() noexcept = default;

IOBuf::IOBuf(IOBuf&& other) noexcept
    : length_(other.length_),
      data_(other.data_),
      capacity_(other.capacity_),
      buf_(other.buf_),
      flags_and_shared_info_(other.flags_and_shared_info_) {
  // Reset other so it is a clean state to be destroyed.
  other.data_ = nullptr;
  other.buf_ = nullptr;
  other.length_ = 0;
  other.capacity_ = 0;
  other.flags_and_shared_info_ = 0;

  // If other was part of the chain, assume ownership of the rest of its chain.
  // (It's only valid to perform move assignment on the head of a chain.)
  if (other.next_ != &other) {
    next_ = other.next_;
    next_->prev_ = this;
    other.next_ = &other;

    prev_ = other.prev_;
    prev_->next_ = this;
    other.prev_ = &other;
  }

  // Sanity check to make sure that other is in a valid state to be destroyed.
  DCHECK_EQ(other.prev_, &other);
  DCHECK_EQ(other.next_, &other);
}

IOBuf::IOBuf(const IOBuf& other) { *this = other.CloneAsValue(); }

IOBuf::IOBuf(InternalConstructor, uintptr_t flags_and_shared_info, uint8_t* buf,
             std::size_t capacity, uint8_t* data, std::size_t length) noexcept
    : length_(length),
      data_(data),
      capacity_(capacity),
      buf_(buf),
      next_(this),
      prev_(this),
      flags_and_shared_info_(flags_and_shared_info) {
  assert(data >= buf);
  assert(intptr_t(data) + length <= intptr_t(buf) + capacity);

  //   if (folly::asan_region_is_poisoned(buf, capacity)) {
  //     // If we know it's a poisoned region, access it to trigger ASAN
  //     reporting. memset(buf, 0, capacity);
  //   }
}

IOBuf::~IOBuf() {
  // Destroying an IOBuf destroys the entire chain.
  // Users of IOBuf should only explicitly delete the head of any chain.
  // The other elements in the chain will be automatically destroyed.
  while (next_ != this) {
    // Since unlink() returns unique_ptr() and we don't store it,
    // it will automatically delete the unlinked element.
    (void)next_->Unlink();
  }

  DecrementRefcount();
}

IOBuf& IOBuf::operator=(IOBuf&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  // If we are part of a chain, delete the rest of the chain.
  while (next_ != this) {
    // Since unlink() returns unique_ptr() and we don't store it,
    // it will automatically delete the unlinked element.
    (void)next_->Unlink();
  }

  // Decrement our refcount on the current buffer
  DecrementRefcount();

  // Take ownership of the other buffer's data
  data_ = other.data_;
  buf_ = other.buf_;
  length_ = other.length_;
  capacity_ = other.capacity_;
  flags_and_shared_info_ = other.flags_and_shared_info_;
  // Reset other so it is a clean state to be destroyed.
  other.data_ = nullptr;
  other.buf_ = nullptr;
  other.length_ = 0;
  other.capacity_ = 0;
  other.flags_and_shared_info_ = 0;

  // If other was part of the chain, assume ownership of the rest of its chain.
  // (It's only valid to perform move assignment on the head of a chain.)
  if (other.next_ != &other) {
    next_ = other.next_;
    next_->prev_ = this;
    other.next_ = &other;

    prev_ = other.prev_;
    prev_->next_ = this;
    other.prev_ = &other;
  }

  // Sanity check to make sure that other is in a valid state to be destroyed.
  DCHECK_EQ(other.prev_, &other);
  DCHECK_EQ(other.next_, &other);

  return *this;
}

IOBuf& IOBuf::operator=(const IOBuf& other) {
  if (this != &other) {
    *this = IOBuf(other);
  }

  return *this;
}

bool IOBuf::empty() const {
  const IOBuf* current = this;

  do {
    if (current->Length() != 0) {
      return false;
    }

    current = current->next_;
  } while (current != this);

  return true;
}

size_t IOBuf::CountChainElements() const {
  size_t count = 1;

  for (IOBuf* current = next_; current != this; current = current->next_) {
    ++count;
  }

  return count;
}

std::size_t IOBuf::ComputeChainDataLength() const {
  std::size_t len = length_;

  for (IOBuf* current = next_; current != this; current = current->next_) {
    len += current->length_;
  }

  return len;
}

std::size_t IOBuf::ComputeChainCapacity() const {
  std::size_t capacity = capacity_;

  for (IOBuf* current = next_; current != this; current = current->next_) {
    capacity += current->capacity_;
  }

  return capacity;
}

void IOBuf::AppendToChain(unique_ptr<IOBuf>&& iobuf) {
  // Take ownership of the specified IOBuf
  IOBuf* other = iobuf.release();

  // Remember the pointer to the tail of the other chain
  IOBuf* other_tail = other->prev_;

  // Hook up prev_->next_ to point at the start of the other chain,
  // and other->prev_ to point at prev_
  prev_->next_ = other;
  other->prev_ = prev_;

  // Hook up other_tail->next_ to point at us,
  // and prev_ to point back at other_tail,
  other_tail->next_ = this;
  prev_ = other_tail;
}

unique_ptr<IOBuf> IOBuf::Clone() const {
  auto tmp = CloneOne();

  for (IOBuf* current = next_; current != this; current = current->next_) {
    tmp->AppendToChain(current->CloneOne());
  }

  return tmp;
}

unique_ptr<IOBuf> IOBuf::CloneOne() const {
  if (SharedInfo* info = SharedInfo()) {
    info->refcount.fetch_add(1, std::memory_order_acq_rel);
  }

  return std::unique_ptr<IOBuf>(new IOBuf(InternalConstructor(),
                                          flags_and_shared_info_, buf_,
                                          capacity_, data_, length_));
}

unique_ptr<IOBuf> IOBuf::CloneCoalesced() const {
  return std::make_unique<IOBuf>(CloneCoalescedAsValue());
}

unique_ptr<IOBuf> IOBuf::CloneCoalescedWithHeadroomTailroom(
    std::size_t new_head_room, std::size_t new_tail_room) const {
  return std::make_unique<IOBuf>(
      CloneCoalescedAsValueWithHeadroomTailroom(new_head_room, new_tail_room));
}

IOBuf IOBuf::CloneAsValue() const {
  auto tmp = CloneOneAsValue();

  for (IOBuf* current = next_; current != this; current = current->next_) {
    tmp.AppendToChain(current->CloneOne());
  }

  return tmp;
}

IOBuf IOBuf::CloneOneAsValue() const {
  if (SharedInfo* info = SharedInfo()) {
    info->refcount.fetch_add(1, std::memory_order_acq_rel);
  }

  return IOBuf(InternalConstructor(), flags_and_shared_info_, buf_, capacity_,
               data_, length_);
}

IOBuf IOBuf::CloneCoalescedAsValue() const {
  const std::size_t new_head_room = HeadRoom();
  const std::size_t new_tail_room = prev()->TailRoom();

  return CloneCoalescedAsValueWithHeadroomTailroom(new_head_room,
                                                   new_tail_room);
}

IOBuf IOBuf::CloneCoalescedAsValueWithHeadroomTailroom(
    std::size_t new_head_room, std::size_t new_tail_room) const {
  if (IsChained() || new_head_room != HeadRoom()) {
    // Fall through to slow code path.
  } else if (new_tail_room == TailRoom()) {
    return CloneOneAsValue();
  } else if (new_tail_room < TailRoom()) {
    const std::size_t new_capacity =
        GoodExtBufferSize(length_ + new_head_room + new_tail_room) -
        sizeof(SharedInfo);

    if (TailRoom() <= new_capacity - new_head_room - length_) {
      return CloneOneAsValue();
    }
  }

  // Coalesce into new buffer.
  const std::size_t new_length = ComputeChainDataLength();
  const std::size_t new_capacity = new_length + new_head_room + new_tail_room;
  IOBuf new_buf{kCreate, new_capacity};
  new_buf.Advance(new_head_room);

  auto current = this;

  do {
    if (current->Length() > 0) {
      DCHECK_NOTNULL(current->data());
      DCHECK_LE(current->Length(), new_buf.TailRoom());

      memcpy(new_buf.WritableTail(), current->data(), current->Length());
      new_buf.Append(current->Length());
    }

    current = current->Next();
  } while (current != this);

  DCHECK_EQ(new_length, new_buf.Length());
  DCHECK_EQ(new_head_room, new_buf.HeadRoom());
  DCHECK_LE(new_tail_room, new_buf.TailRoom());

  return new_buf;
}

void IOBuf::UnshareOneSlow() {
  // Allocate a new buffer for the data
  uint8_t* buf;
  SharedInfo* shared_info;
  std::size_t actual_capacity;

  AllocExtBuffer(capacity_, &buf, &shared_info, &actual_capacity);

  // Copy the data
  // Maintain the same amount of headroom.  Since we maintained the same
  // minimum capacity we also maintain at least the same amount of tailroom.
  std::size_t head_len = HeadRoom();

  if (length_ > 0) {
    assert(data_ != nullptr);

    memcpy(buf + head_len, data_, length_);
  }

  // Release our reference on the old buffer
  DecrementRefcount();
  // Make sure flags are all cleared.
  SetFlagsAndSharedInfo(0, shared_info);

  // Update the buffer pointers to point to the new buffer
  data_ = buf + head_len;
  buf_ = buf;
}

void IOBuf::UnshareChained() {
  // unshareChained() should only be called if we are part of a chain of
  // multiple IOBufs.  The caller should have already verified this.
  assert(IsChained());

  IOBuf* current = this;

  while (true) {
    if (current->IsSharedOne()) {
      // we have to unshare
      break;
    }

    current = current->next_;

    if (current == this) {
      // None of the IOBufs in the chain are shared,
      // so return without doing anything
      return;
    }
  }

  // We have to unshare.  Let coalesceSlow() do the work.
  CoalesceSlow();
}

void IOBuf::MarkExternallyShared() {
  IOBuf* current = this;

  do {
    current->MarkExternallySharedOne();
    current = current->next_;
  } while (current != this);
}

void IOBuf::MakeManagedChained() {
  assert(IsChained());

  IOBuf* current = this;

  while (true) {
    current->MakeManagedOne();
    current = current->next_;

    if (current == this) {
      break;
    }
  }
}

void IOBuf::CoalesceSlow() {
  // CoalesceSlow() should only be called if we are part of a chain of multiple
  // IOBufs.  The caller should have already verified this.
  DCHECK(IsChained());

  // Compute the length of the entire chain
  std::size_t new_length = 0;
  IOBuf* end = this;

  do {
    new_length += end->length_;
    end = end->next_;
  } while (end != this);

  CoalesceAndReallocate(new_length, end);

  // We should be only element left in the chain now
  DCHECK(!IsChained());
}

void IOBuf::CoalesceSlow(size_t max_length) {
  // coalesceSlow() should only be called if we are part of a chain of multiple
  // IOBufs.  The caller should have already verified this.
  DCHECK(IsChained());
  DCHECK_LT(length_, max_length);

  // Compute the length of the entire chain
  std::size_t new_length = 0;
  IOBuf* end = this;

  while (true) {
    new_length += end->length_;
    end = end->next_;

    if (new_length >= maxLength) {
      break;
    }

    if (end == this) {
      throw_exception<std::overflow_error>(
          "attempted to coalesce more data than "
          "available");
    }
  }

  CoalesceAndReallocate(new_length, end);

  // We should have the requested length now
  DCHECK_GE(length_, maxLength);
}

void IOBuf::CoalesceAndReallocate(size_t new_head_room, size_t new_length,
                                  IOBuf* end, size_t new_tail_room) {
  std::size_t new_capacity = new_length + new_head_room + new_tail_room;

  // Allocate space for the coalesced buffer.
  // We always convert to an external buffer, even if we happened to be an
  // internal buffer before.
  uint8_t* new_buf;
  SharedInfo* new_info;
  std::size_t actual_capacity;
  allocExtBuffer(new_capacity, &new_buf, &new_info, &actual_capacity);

  // Copy the data into the new buffer
  uint8_t* new_data = new_buf + new_head_room;
  uint8_t* p = new_data;
  IOBuf* current = this;
  size_t remaining = new_length;

  do {
    if (current->length_ > 0) {
      assert(current->length_ <= remaining);
      assert(current->data_ != nullptr);

      remaining -= current->length_;
      memcpy(p, current->data_, current->length_);
      p += current->length_;
    }

    current = current->next_;
  } while (current != end);

  assert(remaining == 0);

  (void)remaining;

  // Point at the new buffer
  DecrementRefcount();

  // Make sure flags are all cleared.
  SetFlagsAndSharedInfo(0, new_info);

  capacity_ = actual_capacity;
  buf_ = new_buf;
  data_ = new_data;
  length_ = new_length;

  // Separate from the rest of our chain.
  // Since we don't store the unique_ptr returned by separateChain(),
  // this will immediately delete the returned subchain.
  if (IsChained()) {
    (void)SeparateChain(next_, current->prev_);
  }
}

void IOBuf::DecrementRefcount() noexcept {
  // Externally owned buffers don't have a SharedInfo object and aren't managed
  // by the reference count
  SharedInfo* info = SharedInfo();

  if (!info) {
    return;
  }

  // Avoid doing atomic decrement if the refcount is 1.
  // This is safe, because it means that we're the last reference and destroying
  // the object. Anything trying to copy it is already undefined behavior.
  if (info->refcount.load(std::memory_order_acquire) > 1) {
    // Decrement the refcount
    uint32_t new_cnt = info->refcount.fetch_sub(1, std::memory_order_acq_rel);
    // Note that fetch_sub() returns the value before we decremented.
    // If it is 1, we were the only remaining user; if it is greater there are
    // still other users.
    if (new_cnt > 1) {
      return;
    }
  }

  // save the useHeapFullStorage flag here since
  // freeExtBuffer can delete the sharedInfo()
  bool use_heap_full_storage = info->use_heap_full_storage;

  // We were the last user.  Free the buffer
  FreeExtBuffer();

  // Free the SharedInfo if it was allocated separately.
  //
  // This is only used by takeOwnership().
  //
  // To avoid this special case handling in decrementRefcount(), we could have
  // takeOwnership() set a custom freeFn() that calls the user's free function
  // then frees the SharedInfo object.  (This would require that
  // takeOwnership() store the user's free function with its allocated
  // SharedInfo object.)  However, handling this specially with a flag seems
  // like it shouldn't be problematic.
  if (Flags() & kFlagFreeSharedInfo) {
    delete info;
  } else {
    if (use_heap_full_storage) {
      SharedInfo::ReleaseStorage(info);
    }
  }
}

void IOBuf::ReserveSlow(std::size_t min_head_room, std::size_t min_tail_room) {
  size_t new_capacity = length_;

  if (!checked_add(&new_capacity, new_capacity, min_head_room) ||
      !checked_add(&new_capacity, new_capacity, min_tail_room) ||
      new_capacity > kMaxIOBufSize) {
    // overflow
    throw_exception<std::bad_alloc>();
  }

  // reserveSlow() is dangerous if anyone else is sharing the buffer, as we may
  // reallocate and free the original buffer.  It should only ever be called if
  // we are the only user of the buffer.
  DCHECK(!IsSharedOne());

  // We'll need to reallocate the buffer.
  // There are a few options.
  // - If we have enough total room, move the data around in the buffer
  //   and adjust the data_ pointer.
  // - If we're using an internal buffer, we'll switch to an external
  //   buffer with enough headroom and tailroom.
  // - If we have enough headroom (headroom() >= minHeadroom) but not too much
  //   (so we don't waste memory), we can try one of two things, depending on
  //   whether we use jemalloc or not:
  //   - If using jemalloc, we can try to expand in place, avoiding a memcpy()
  //   - If not using jemalloc and we don't have too much to copy,
  //     we'll use realloc() (note that realloc might have to copy
  //     headroom + data + tailroom, see smartRealloc in folly/memory/Malloc.h)
  // - Otherwise, bite the bullet and reallocate.
  if (HeadRoom() + TailRoom() >= min_head_room + min_tail_room) {
    uint8_t* new_data = WritableBuffer() + min_head_room;

    memmove(new_data, data_, length_);
    data_ = new_data;

    return;
  }

  size_t new_allocated_capacity = 0;
  uint8_t* new_buffer = nullptr;
  std::size_t new_head_room = 0;
  std::size_t old_head_room = HeadRoom();

  // If we have a buffer allocated with malloc and we just need more tailroom,
  // try to use realloc()/xallocx() to grow the buffer in place.
  SharedInfo* info = SharedInfo();
  bool use_heap_full_storage = info && info->use_heap_full_storage;

  if (info && (info->free_fn == nullptr) && length_ != 0 &&
      old_head_room >= min_head_room) {
    size_t head_slack = old_head_room - min_head_room;
    new_allocated_capacity = GoodExtBufferSize(new_capacity + head_slack);
    size_t copy_slack = capacity() - length_;

    if (copy_slack * 2 <= length_) {
      void* p = realloc(buf_, new_allocated_capacity);

      if (p == nullptr) [[unlikely]] {
        throw_exception<std::bad_alloc>();
      }

      new_buffer = static_cast<uint8_t*>(p);
      new_head_room = old_head_room;
    }
  }

  // None of the previous reallocation strategies worked (or we're using
  // an internal buffer).  malloc/copy/free.
  if (new_buffer == nullptr) {
    new_allocated_capacity = GoodExtBufferSize(new_capacity);
    new_buffer = static_cast<uint8_t*>(checked_malloc(new_allocated_capacity));

    if (length_ > 0) {
      assert(data_ != nullptr);

      memcpy(new_buffer + min_head_room, data_, length_);
    }

    if (SharedInfo()) {
      FreeExtBuffer();
    }

    new_head_room = min_head_room;
  }

  std::size_t cap;
  InitExtBuffer(new_buffer, new_allocated_capacity, &info, &cap);

  if (Flags() & kFlagFreeSharedInfo) {
    delete SharedInfo();
  } else {
    if (use_heap_full_storage) {
      SharedInfo::ReleaseStorage(SharedInfo());
    }
  }

  SetFlagsAndSharedInfo(0, info);
  capacity_ = cap;
  buf_ = new_buffer;
  data_ = new_buffer + new_head_room;
  // length_ is unchanged
}

// The user's free function should never throw. Otherwise we might throw from
// the IOBuf destructor. Other code paths like coalesce() also assume that
// decrementRefcount() cannot throw.
void IOBuf::FreeExtBuffer() noexcept {
  SharedInfo* info = SharedInfo();

  DCHECK(info);

  // save the observer_list_head
  // since the SharedInfo can be freed
  auto observer_list_head = info->observer_list_head;
  info->observer_list_head = nullptr;

  if (info->free_fn) {
    info->free_fn(buf_, info->user_data);
  } else {
    // this will invoke free if info->userData is 0
    size_t size = reinterpret_cast<size_t>(info->user_data);

    if (size) {
      if (io_buf_free_cb) {
        io_buf_free_cb(buf_, size);
      }
    }

    free(buf_);
  }

  SharedInfo::InvokeAndDeleteEachObserver(
      observer_list_head, [](auto& entry) { entry.AfterFreeExtBuffer(); });

  if (kIsMobile) {
    buf_ = nullptr;
  }
}

void IOBuf::AllocExtBuffer(std::size_t min_capacity, uint8_t** out_buf,
                           SharedInfo** out_info, std::size_t* out_capacity) {
  if (min_capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  size_t malloc_size = GoodExtBufferSize(min_capacity);
  auto buf = static_cast<uint8_t*>(checkedMalloc(malloc_size));
  InitExtBuffer(buf, malloc_size, out_info, out_capacity);

  // the userData and the freeFn are nullptr here
  // just store the malloc_size in userData
  (*out_info)->user_data = reinterpret_cast<void*>(malloc_size);

  if (io_buf_alloc_cb) {
    io_buf_alloc_cb(buf, malloc_size);
  }

  *out_buf = buf;
}

size_t IOBuf::GoodExtBufferSize(std::size_t min_capacity) {
  if (min_capacity > kMaxIOBufSize) {
    throw_exception<std::bad_alloc>();
  }

  // Determine how much space we should allocate.  We'll store the SharedInfo
  // for the external buffer just after the buffer itself.  (We store it just
  // after the buffer rather than just before so that the code can still just
  // use free(buf_) to free the buffer.)
  size_t min_size = static_cast<size_t>(min_capacity) + sizeof(SharedInfo);
  // Add room for padding so that the SharedInfo will be aligned on an 8-byte
  // boundary.
  min_size = (min_size + 7) & ~7;

  // Use GoodMallocSize() to bump up the capacity to a decent size to request
  // from malloc, so we can use all of the space that malloc will probably give
  // us anyway.
  return min_size;
}

void IOBuf::InitExtBuffer(uint8_t* buf, size_t malloc_size,
                          SharedInfo** out_info, std::size_t* out_capacity) {
  // Find the SharedInfo storage at the end of the buffer
  // and construct the SharedInfo.
  uint8_t* info_start = (buf + malloc_size) - sizeof(SharedInfo);
  auto shared_info = new (info_start) SharedInfo;

  *out_capacity = std::size_t(info_start - buf);
  *out_info = shared_info;
}

std::string IOBuf::MoveToFbString() {
  //   // we need to save useHeapFullStorage and the observerListHead since
  //   // sharedInfo() may not be valid after fbstring str
  //   bool use_heap_full_storage = false;
  //   SharedInfoObserverEntryBase* observer_list_head = nullptr;
  //   auto info = SharedInfo();

  //   // malloc-allocated buffers are just fine, everything else needs
  //   // to be turned into one.
  //   if (!info ||            // user owned, not ours to give up
  //       info->free_fn ||    // not malloc()-ed
  //       HeadRoom() != 0 ||  // malloc()-ed block doesn't start at beginning
  //       TailRoom() == 0 ||  // no room for NUL terminator
  //       IsShared() ||       // shared
  //       IsChained()) {      // chained
  //     // We might as well get rid of all head and tailroom if we're going
  //     // to reallocate; we need 1 byte for NUL terminator.
  //     CoalesceAndReallocate(0, ComputeChainDataLength(), this, 1);
  //   } else {
  //     if (info) {
  //       // if we do not call coalesceAndReallocate
  //       // we might need to call SharedInfo::releaseStorage()
  //       // and/or SharedInfo::invokeAndDeleteEachObserver()
  //       use_heap_full_storage = info->use_heap_full_storage;
  //       // save the observer_list_head
  //       // the coalesceAndReallocate path will call
  //       // decrementRefcount and freeExtBuffer if needed
  //       // so the observer lis notification is needed here
  //       observer_list_head = info->observer_list_head;
  //       info->observer_list_head = nullptr;
  //     }
  //   }

  //   // Ensure NUL terminated
  //   *WritableTail() = 0;
  //   fbstring str(reinterpret_cast<char*>(writableData()), length(),
  //   capacity(),
  //                AcquireMallocatedString());

  //   if (io_buf_free_cb && sharedInfo() && sharedInfo()->userData) {
  //     io_buf_free_cb(writableData(),
  //                    reinterpret_cast<size_t>(sharedInfo()->userData));
  //   }

  //   SharedInfo::invokeAndDeleteEachObserver(
  //       observerListHead, [](auto& entry) { entry.afterReleaseExtBuffer();
  //       });

  //   if (flags() & kFlagFreeSharedInfo) {
  //     delete sharedInfo();
  //   } else {
  //     if (useHeapFullStorage) {
  //       SharedInfo::releaseStorage(sharedInfo());
  //     }
  //   }

  //   // Reset to a state where we can be deleted cleanly
  //   flagsAndSharedInfo_ = 0;
  //   buf_ = nullptr;
  //   clear();

  //   return str;

  return {};
}

IOBuf::Iterator IOBuf::cbegin() const { return Iterator(this, this); }

IOBuf::Iterator IOBuf::cend() const { return Iterator(nullptr, nullptr); }

std::vector<struct iovec> IOBuf::GetIov() const {
  folly::fbvector<struct iovec> iov;
  iov.reserve(CountChainElements());
  AppendToIov(&iov);

  return iov;
}

void IOBuf::AppendToIov(std::vector<struct iovec>* iov) const {
  IOBuf const* p = this;

  do {
    // some code can get confused by empty iovs, so skip them
    if (p->Length() > 0) {
      iov->push_back({(void*)p->data(), p->Length()});
    }
    p = p->Next();
  } while (p != this);
}

unique_ptr<IOBuf> IOBuf::WrapIov(const iovec* vec, size_t count) {
  unique_ptr<IOBuf> result = nullptr;

  for (size_t i = 0; i < count; ++i) {
    size_t len = vec[i].iov_len;
    void* data = vec[i].iov_base;

    if (len > 0) {
      auto buf = WrapBuffer(data, len);

      if (!result) {
        result = std::move(buf);
      } else {
        result->AppendToChain(std::move(buf));
      }
    }
  }

  if (result == nullptr) [[unlikely]] {
    return Create(0);
  }

  return result;
}

std::unique_ptr<IOBuf> IOBuf::TakeOwnershipIov(const iovec* vec, size_t count,
                                               FreeFunction free_fn,
                                               void* user_data,
                                               bool free_on_error) {
  unique_ptr<IOBuf> result = nullptr;

  for (size_t i = 0; i < count; ++i) {
    size_t len = vec[i].iov_len;
    void* data = vec[i].iov_base;

    if (len > 0) {
      auto buf = TakeOwnership(data, len, free_fn, user_data, free_on_error);

      if (!result) {
        result = std::move(buf);
      } else {
        result->AppendToChain(std::move(buf));
      }
    }
  }

  if (result == nullptr) [[unlikely]] {
    return Create(0);
  }

  return result;
}

IOBuf::FillIovResult IOBuf::FillIov(struct iovec* iov, size_t len) const {
  IOBuf const* p = this;
  size_t i = 0;
  size_t total_bytes = 0;

  while (i < len) {
    // some code can get confused by empty iovs, so skip them
    if (p->Length() > 0) {
      iov[i].iov_base = const_cast<uint8_t*>(p->data());
      iov[i].iov_len = p->Length();
      total_bytes += p->Length();
      i++;
    }

    p = p->Next();

    if (p == this) {
      return {i, total_bytes};
    }
  }

  return {0, 0};
}

uint32_t IOBuf::ApproximateShareCountOne() const {
  auto info = SharedInfo();

  if (!info) [[unlikely]] {
    return 1U;
  }

  return info->refcount.load(std::memory_order_acquire);
}

// size_t IOBufHash::operator()(const IOBuf& buf) const noexcept {
//   folly::hash::SpookyHashV2 hasher;
//   hasher.Init(0, 0);
//   io::Cursor cursor(&buf);
//   for (;;) {
//     auto b = cursor.peekBytes();
//     if (b.empty()) {
//       break;
//     }
//     hasher.Update(b.data(), b.size());
//     cursor.skip(b.size());
//   }
//   uint64_t h1;
//   uint64_t h2;
//   hasher.Final(&h1, &h2);
//   return static_cast<std::size_t>(h1);
// }

// ordering IOBufCompare::impl(const IOBuf& a, const IOBuf& b) const noexcept {
//   io::Cursor ca(&a);
//   io::Cursor cb(&b);
//   for (;;) {
//     auto ba = ca.peekBytes();
//     auto bb = cb.peekBytes();
//     if (ba.empty() || bb.empty()) {
//       return to_ordering(int(bb.empty()) - int(ba.empty()));
//     }
//     const size_t n = std::min(ba.size(), bb.size());
//     DCHECK_GT(n, 0u);
//     const ordering r = to_ordering(std::memcmp(ba.data(), bb.data(), n));
//     if (r != ordering::eq) {
//       return r;
//     }
//     // Cursor::skip() may throw if n is too large, but n is not too large
//     here ca.skip(n); cb.skip(n);
//   }
// }

}  // namespace kiwi
