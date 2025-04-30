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

//
// Docs: https://fburl.com/fbcref_iobuf
//

#pragma once

#include <glog/logging.h>

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <compare>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "kiwi/common/exception.hh"
#include "kiwi/common/function_ref.hh"
#include "kiwi/common/iterators.hh"
#include "kiwi/containers/span.hh"
#include "kiwi/portability/compiler_specific.hh"
#include "kiwi/sys/uio.hh"
#include "kiwi/util/checked_math.hh"

KIWI_PUSH_WARNING
// Ignore shadowing warnings within this file, so includers can use -Wshadow.
KIWI_GNU_DISABLE_WARNING("-Wshadow")
// Some compilers break on -Wdocumentation. Not all compilers recognise that
// option, so we also suppress -Wpragmas
KIWI_GNU_DISABLE_WARNING("-Wpragmas")
// Ignore documentation warnings, to enable overloads to share documentation
// with differing parameters
KIWI_GNU_DISABLE_WARNING("-Wdocumentation")

namespace kiwi {

namespace detail {

/// Trait to check if a type is a std::unique_ptr to a standard layout type.
///
/// \tparam T Type to check.
template <typename T>
struct IsUniquePtrToSL : std::false_type {};

template <typename T, typename D>
struct IsUniquePtrToSL<std::unique_ptr<T, D>> : std::is_standard_layout<T> {};

}  // namespace detail

/// IOBuf manages heap-allocated byte buffers.
///
/// API Details
/// -----------
///
///  - The buffer is not neccesarily full of meaningful bytes - there may be
///    uninitialized bytes before and after the central "valid" range of data.
///  - Buffers are refcounted, and can be shared by multiple IOBuf objects.
///    - If you ever write to an IOBuf, first use Unshare() to get a unique
///      copy.
///  - IOBufs can be "chained" in a circularly linked list.
///    - Use Coalesce() to turn an IOBuf chain into a single IOBuf.
///  - IOBufs are not synchronized. The user is responsible for synchronization.
///    Notes:
///    - Like a shared_ptr, the refcounting is atomic.
///    - const IOBuf methods do not mutate any state, so can safely be called
///      concurrently with each other, as expected.
///  - IOBufs are typically stored on the heap, so that they can be used in
///    chains.
///
///
/// Data Layout
/// -----------
///
/// IOBuf objects contains a pointer to the buffer and information about which
/// segment of the buffer contains valid data.
///
///      +-------+
///      | IOBuf |
///      +-------+
///       /
///      |            |----- Length() -----|
///      v
///      +------------+--------------------+-----------+
///      | headroom   |        data        |  tailroom |
///      +------------+--------------------+-----------+
///      ^            ^                    ^           ^
///      Buffer()   data()               Tail()      BufferEnd()
///
///      |----------------- capacity() ----------------|
///
///
/// Buffer Sharing
/// --------------
///
/// Each buffer is reference counted, and multiple IOBuf objects may point
/// to the same buffer.  Each IOBuf may point to a different section of valid
/// data within the underlying buffer.  For example, if multiple protocol
/// requests are read from the network into a single buffer, a separate IOBuf
/// may be created for each request, all sharing the same underlying buffer.
///
///
/// In other words, when multiple IOBufs share the same underlying buffer, the
/// data() and Tail() methods on each IOBuf may point to a different segment of
/// the data.  However, the Buffer() and BufferEnd() methods will point to the
/// same location for all IOBufs sharing the same underlying buffer, unless the
/// tail was trimmed with TrimWritableTail().
///
///
///           +-----------+     +---------+
///           |  IOBuf 1  |     | IOBuf 2 |
///           +-----------+     +---------+
///            |         | _____/        |
///       data |    tail |/    data      | tail
///            v         v               v
///      +-------------------------------------+
///      |     |         |               |     |
///      +-------------------------------------+
///
/// If you only read data from an IOBuf, you don't need to worry about other
/// IOBuf objects possibly sharing the same underlying buffer.  However, if you
/// ever write to the buffer you need to first ensure that no other IOBufs point
/// to the same buffer.  The Unshare() method may be used to ensure that you
/// have an unshared buffer.
///
/// IOBuf Chains
/// ------------
///
/// IOBuf objects also contain pointers to next and previous IOBuf objects.
/// This can be used to represent a single logical piece of data that is stored
/// in non-contiguous chunks in separate buffers.
///
///     +---------------------------------------------------------------+
///     |                                                               |
///     |    +-----------+        +-----------+        +-----------+    |
///     +--> |  IOBuf 1  | -----> |  IOBuf 2  | -----> |  IOBuf 3  | ---+
///          +-----------+        +-----------+        +-----------+
///            |        | _________/     |           ___/        \__
///            |        |/               |          /               \
///            v        v                v         v                 v
///      +-------------------------------------+   +-----------------+
///      |     |        |                |     |   |                 |
///      +-------------------------------------+   +-----------------+
///
/// A single IOBuf object can only belong to one chain at a time.
///
/// IOBuf chains are always circular.  The "prev" pointer in the head of the
/// chain points to the tail of the chain.  However, it is up to the user to
/// decide which IOBuf is the head.  Internally the IOBuf code does not care
/// which element is the head.
///
/// The lifetime of all IOBufs in the chain are linked: when one element in the
/// chain is deleted, all other chained elements are also deleted.  Conceptually
/// it is simplest to treat this as if the head of the chain owns all other
/// IOBufs in the chain.  When you delete the head of the chain, it will delete
/// the other elements as well.  For this reason, AppendToChain() and
/// InsertAfterThisOne() take ownership of the new elements being added to
/// this chain.
///
/// When the Coalesce() method is used to coalesce an entire IOBuf chain into
/// a single IOBuf, all other IOBufs in the chain are eliminated and
/// automatically deleted.  The Unshare() method may coalesce the chain; if
/// it does it will similarly delete all IOBufs eliminated from the chain.
///
/// As discussed in the following section, it is up to the user to maintain a
/// lock around the entire IOBuf chain if multiple threads need to access the
/// chain.  IOBuf does not provide any internal locking.
///
///
/// Synchronization
/// ---------------
///
/// When used in multithread programs, a single IOBuf object should only be
/// accessed mutably by a single thread at a time.  All const member functions
/// of IOBuf are safe to call concurrently with one another, but when a caller
/// uses a single IOBuf across multiple threads and at least one thread calls a
/// non-const member function, the caller is responsible for using an external
/// lock to synchronize access to the IOBuf.
///
/// Two separate IOBuf objects may be accessed concurrently in separate threads
/// without locking, even if they point to the same underlying buffer.  The
/// buffer reference count is always accessed atomically, and no other
/// operations should affect other IOBufs that point to the same data segment.
/// The caller is responsible for using Unshare() to ensure that the data
/// buffer is not shared by other IOBufs before writing to it, and this ensures
/// that the data itself is not modified in one thread while also being accessed
/// from another thread.
///
/// For IOBuf chains, no two IOBufs in the same chain should be accessed
/// simultaneously in separate threads, except where all simultaneous accesses
/// are to const member functions.  The caller must maintain a lock around the
/// entire chain if the chain, or individual IOBufs in the chain, may be
/// accessed by multiple threads with at least one of the threads needing to
/// mutate.
///
///
/// IOBuf Object Allocation
/// -----------------------
///
/// IOBuf objects themselves exist separately from the data buffer they point
/// to.  Therefore one must also consider how to allocate and manage the IOBuf
/// objects. Typically, IOBufs are allocated on the heap.
///
///      +--------------+
///      |  unique_ptr  |
///      +--------------+
///        |
///        v
///      +---------+
///      |  IOBuf  |
///      +---------+
///        |
///        v
///      +----------+
///      |  buffer  |
///      +----------+
///
///
/// It is more common to allocate IOBuf objects on the heap, using the
/// Create(), TakeOwnership(), or WrapBuffer() factory functions.  The
/// Clone()/CloneOne() functions also return new heap-allocated IOBufs.  The
/// CreateCombined() function allocates the IOBuf object and data storage
/// space together, in a single memory allocation.  This can improve
/// performance, particularly if you know that the data buffer and the IOBuf
/// itself will have similar lifetimes.
///
/// That said, it is also possible to allocate IOBufs on the stack or inline
/// inside another object as well.  This is useful for cases where the IOBuf is
/// short-lived, or when the overhead of allocating the IOBuf on the heap is
/// undesirable.
///
/// However, note that stack-allocated IOBufs may only be used as the head of a
/// chain (or standalone as the only IOBuf in a chain).  All non-head members of
/// an IOBuf chain must be heap allocated.  (All functions to add nodes to a
/// chain require a std::unique_ptr<IOBuf>, which enforces this requirement.)
///
/// Copying IOBufs is only meaningful for the head of a chain. The entire chain
/// is cloned; the IOBufs will become shared, and the old and new IOBufs will
/// refer to the same underlying memory.
///
///
/// IOBuf Sharing
/// -------------
///
/// The IOBuf class manages sharing of the underlying buffer that it points to,
/// maintaining a reference count if multiple IOBufs are pointing at the same
/// buffer.
///
/// However, it is the callers responsibility to manage sharing and ownership of
/// IOBuf objects themselves.  The IOBuf structure does not provide room for an
/// intrusive refcount on the IOBuf object itself, only the underlying data
/// buffer is reference counted.  If users want to share the same IOBuf object
/// between multiple parts of the code, they are responsible for managing this
/// sharing on their own.  (For example, by using a shared_ptr.
/// Alternatively, users always have the option of using clone() to create a
/// second IOBuf that points to the same underlying buffer.)
///
///
/// Inspiration
/// -----------
///
/// IOBuf objects are intended to be used primarily for networking code, and are
/// modelled somewhat after FreeBSD's mbuf data structure, and Linux's sk_buff
/// structure.
///
/// IOBuf objects facilitate zero-copy network programming, by allowing multiple
/// IOBuf objects to point to the same underlying buffer of data, using a
/// reference count to track when the buffer is no longer needed and can be
/// freed.
class IOBuf {
public:
  class Iterator;

  enum CreateOp { kCreate };
  enum WrapBufferOp { kWrapBuffer };
  enum TakeOwnershipOp { kTaskeOwnership };
  enum CopyBufferOp { kCopyBuffer };
  enum SizedFree { kSizedFree };
  enum class CombinedOption { kDefault, kCombined, kSeparate };

  using value_type = uint8_t;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using ByteRange = kiwi::span<uint8_t>;
  using FreeFunction = void (*)(void* buf, void* user_data);

private:
  class DeleterBase {
   public:
    virtual ~DeleterBase() {}
    virtual void Dispose(void* p) noexcept = 0;
  };

  template <typename UniquePtr>
  class UniquePtrDeleter : public DeleterBase {
   public:
    using Pointer = typename UniquePtr::pointer;
    using Deleter = typename UniquePtr::deleter_type;

    explicit UniquePtrDeleter(Deleter deleter) : deleter_(std::move(deleter)) {}

    void Dispose(void* p) noexcept override {
      deleter_(static_cast<Pointer>(p));
      delete this;
    }

   private:
    Deleter deleter_;
  };

  enum FlagsEnum : uintptr_t {
    // Adding any more flags would not work on 32-bit architectures,
    // as these flags are stashed in the least significant 2 bits of a
    // max-align-aligned pointer.
    kFlagFreeSharedInfo = 0x1,
    kFlagMask = (1 << 2 /* least significant bits */) - 1,
  };

  struct SharedInfoObserverEntryBase {
    SharedInfoObserverEntryBase* prev{this};
    SharedInfoObserverEntryBase* next{this};

    virtual ~SharedInfoObserverEntryBase() = default;

    virtual void AfterFreeExtBuffer() const noexcept = 0;
    virtual void AfterReleaseExtBuffer() const noexcept = 0;
  };

  template <typename Observer>
  struct SharedInfoObserverEntry : SharedInfoObserverEntryBase {
    std::decay_t<Observer> observer;

    explicit SharedInfoObserverEntry(Observer&& obs) noexcept(
        noexcept(Observer(std::forward<Observer>(obs))))
        : observer(std::forward<Observer>(obs)) {}

    void AfterFreeExtBuffer() const noexcept final {
      observer.AfterFreeExtBuffer();
    }

    void AfterReleaseExtBuffer() const noexcept final {
      observer.AfterReleaseExtBuffer();
    }
  };

  struct SharedInfo {
    using ObserverCb = function_ref<void(SharedInfoObserverEntryBase&)>;

    static void ReleaseStorage(SharedInfo* info) noexcept;
    static void InvokeAndDeleteEachObserver(
        SharedInfoObserverEntryBase* observer_list_head,
        ObserverCb cb) noexcept;

    SharedInfo();
    SharedInfo(FreeFunction fn, void* arg, bool hfs = false);

    // A pointer to a function to call to free the buffer when the refcount
    // hits 0.  If this is null, free() will be used instead.
    FreeFunction free_fn;
    void* user_data;
    SharedInfoObserverEntryBase* observer_list_head{nullptr};
    std::atomic<uint32_t> refcount;
    bool externally_shared{false};
    bool use_heap_full_storage{false};
    std::mutex observer_list_lock{0};
  };

  /// Helper structs for use by operator new and delete.
  struct HeapPrefix;
  struct HeapStorage;
  struct HeapFullStorage;

  /// Create a new IOBuf pointing to an external buffer.
  ///
  /// The caller is responsible for holding a reference count for this new
  /// IOBuf.  The IOBuf constructor does not automatically increment the
  /// reference count.
  struct InternalConstructor {};  // avoid conflicts

  static void FreeUniquePtrBuffer(void* ptr, void* user_data) noexcept {
    static_cast<DeleterBase*>(user_data)->Dispose(ptr);
  }

  static inline uintptr_t PackFlagsAndSharedInfo(uintptr_t flags,
                                                 SharedInfo* info) noexcept {
    uintptr_t uinfo = reinterpret_cast<uintptr_t>(info);

    DCHECK_EQ(flags & ~kFlagMask, 0u);
    DCHECK_EQ(uinfo & kFlagMask, 0u);

    return flags | uinfo;
  }

  inline SharedInfo* SharedInfo() const noexcept {
    return reinterpret_cast<SharedInfo*>(flags_and_shared_info_ & ~kFlagMask);
  }

  inline void SetSharedInfo(SharedInfo* info) noexcept {
    uintptr_t uinfo = reinterpret_cast<uintptr_t>(info);

    DCHECK_EQ(uinfo & kFlagMask, 0u);

    flags_and_shared_info_ = (flags_and_shared_info_ & kFlagMask) | uinfo;
  }

  inline uintptr_t Flags() const noexcept {
    return flags_and_shared_info_ & kFlagMask;
  }

  inline void SetFlags(uintptr_t flags) noexcept {
    DCHECK_EQ(flags & ~kFlagMask, 0u);

    flags_and_shared_info_ |= flags;
  }

  inline void ClearFlags(uintptr_t flags) noexcept {
    DCHECK_EQ(flags & ~kFlagMask, 0u);

    flags_and_shared_info_ &= ~flags;
  }

  inline void SetFlagsAndSharedInfo(uintptr_t flags,
                                    SharedInfo* info) noexcept {
    flags_and_shared_info_ = PackFlagsAndSharedInfo(flags, info);
  }

  enum class TakeOwnershipOption { kDefault, kStoreSize };

  static std::unique_ptr<IOBuf> TakeOwnership(
      void* buf, std::size_t capacity, std::size_t offset, std::size_t length,
      FreeFunction free_fn, void* user_data, bool free_on_error,
      TakeOwnershipOption option);


  IOBuf(InternalConstructor, uintptr_t flagsAndSharedInfo, uint8_t* buf,
        std::size_t capacity, uint8_t* data, std::size_t length) noexcept;

  void UnshareOneSlow();
  void UnshareChained();
  void MakeManagedChained();
  void CoalesceSlow();
  void CoalesceSlow(size_t max_length);

  // newLength must be the entire length of the buffers between this and
  // end (no truncation)
  void CoalesceAndReallocate(size_t new_head_room, size_t new_length,
                             IOBuf* end, size_t new_tail_room);
  void CoalesceAndReallocate(size_t new_length, IOBuf* end) {
    CoalesceAndReallocate(HeadRoom(), new_length, end, end->prev_->TailRoom());
  }
  void DecrementRefcount() noexcept;
  void ReserveSlow(std::size_t min_head_room, std::size_t min_tail_room);
  void FreeExtBuffer() noexcept;

  static size_t GoodExtBufferSize(std::size_t min_capacity);
  static void InitExtBuffer(uint8_t* buf, size_t malloc_size,
                            SharedInfo** info_return,
                            std::size_t* capacity_return);
  static void AllocExtBuffer(std::size_t min_capacity, uint8_t** buf_return,
                             SharedInfo** info_return,
                             std::size_t* capacity_return);
  static void ReleaseStorage(HeapStorage* storage,
                             uint16_t free_flags) noexcept;
  static void FreeInternalBuf(void* buf, void* user_data) noexcept;

 public:
  /// \copydoc IOBuf(CreateOp, std::size_t).
  /// \returns A unique_ptr to a newly-constructed IOBuf.
  /// \methodset Makers
  static std::unique_ptr<IOBuf> Create(std::size_t capacity);

  /// Create an IOBuf, allocated alongside its buffer.
  ///
  /// This method uses a single memory allocation to allocate space for both the
  /// IOBuf object and the data storage space. This saves one memory allocation.
  ///
  /// This can be wasteful if the IOBuf and the buffer have different lifetimes.
  /// The memory will not be reclaimed until both objects are destroyed. This
  /// can happen, for example, if the buffer is grown using reserve().
  ///
  /// \copydetails IOBuf(CreateOp, std::size_t)
  /// \methodset Makers
  static std::unique_ptr<IOBuf> CreateCombined(std::size_t capacity);

  /// Create an IOBuf, allocated separately from its buffer.
  ///
  /// IOBuf::Create() doesn't necessarily perform separate allocations if the
  /// buffer is small. This function forces the IOBuf and its buffer to be
  /// allocated separately. This can save space if you know that the buffer will
  /// be reallocated.
  ///
  /// \copydetails IOBuf(CreateOp, std::size_t)
  /// \methodset Makers
  static std::unique_ptr<IOBuf> CreateSeparate(std::size_t capacity);

  /// Create a new IOBuf chain.
  ///
  /// \param total_capacity The total buffer size of all IOBufs in the chain
  /// \param max_buf_capacity The maximum buffer size of each IOBuf in the
  ///                         chain
  ///
  /// \post ComputeChainCapacity() >= total_capacity
  ///
  /// Note: Some malloc implementations will internally round up an allocation
  /// size to a convenient amount (e.g. jemalloc(31) will actually give you a
  /// slab of size 32). Your buffer size could actually be rounded up to
  /// `GoodMallocSize(max_buf_capacity)`.
  ///
  /// \methodset Makers
  static std::unique_ptr<IOBuf> CreateChain(size_t total_capacity,
                                            std::size_t max_buf_capacity);

  /// \copydoc IOBuf(WrapBufferOp, ByteRange).
  /// \throws std::bad_alloc on error (the allocation of the IOBuf may
  ///         throw).
  /// \returns A unique_ptr to a newly-constructed IOBuf.
  /// @methodset Makers
  static std::unique_ptr<IOBuf> WrapBuffer(const void* buf,
                                           std::size_t capacity);

  /// \copydoc wrapBuffer(const void*, std::size_t)
  static std::unique_ptr<IOBuf> WrapBuffer(ByteRange br) {
    return WrapBuffer(br.data(), br.size());
  }

  /// \copydoc IOBuf(TakeOwnershipOp, void*, std::size_t, FreeFunction, void*,
  ///          bool)
  /// \returns A unique_ptr to a newly-constructed IOBuf.
  /// \methodset Makers
  static std::unique_ptr<IOBuf> TakeOwnership(void* buf, std::size_t capacity,
                                              FreeFunction free_fn = nullptr,
                                              void* user_data = nullptr,
                                              bool free_on_error = true) {
    return TakeOwnership(buf, capacity, 0, capacity, free_fn, user_data,
                         free_on_error, TakeOwnershipOption::kDefault);
  }

  /// \copydoc IOBuf(TakeOwnershipOp, void*, std::size_t, FreeFunction, void*,
  ///          bool)
  /// \returns A unique_ptr to a newly-constructed IOBuf.
  /// \methodset Makers
  static std::unique_ptr<IOBuf> TakeOwnership(void* buf, std::size_t capacity,
                                              std::size_t length,
                                              FreeFunction free_fn = nullptr,
                                              void* user_data = nullptr,
                                              bool free_on_error = true) {
    return TakeOwnership(buf, capacity, 0, length, free_fn, user_data,
                         free_on_error, TakeOwnershipOption::kDefault);
  }

  /// Create an IOBuf with a reinterpreted buffer.
  ///
  /// Create a new IOBuf pointing to an existing data buffer made up of
  /// count objects of a given standard-layout type.
  ///
  /// This is dangerous -- it is essentially equivalent to doing
  /// reinterpret_cast<unsigned char*> on your data -- but it's often useful
  /// for serialization / deserialization.
  ///
  /// The new IOBuf will assume ownership of the buffer, and free it
  /// appropriately (by calling the UniquePtr's custom deleter, or by calling
  /// delete or delete[] appropriately if there is no custom deleter)
  /// when the buffer is destroyed.  The custom deleter, if any, must never
  /// throw exceptions.
  ///
  /// The IOBuf data pointer will initially point to the start of the buffer,
  /// and the length will be the full capacity of the buffer (count *
  /// sizeof(T)).
  ///
  /// On error, std::bad_alloc will be thrown, and the buffer will be freed
  /// before throwing the error.
  ///
  /// \param buf The unique_ptr to the buffer.
  /// \param count The number of elements in the buffer.
  ///
  /// \returns  A unique_ptr to a newly-constructed IOBuf.
  /// \methodset Makers.
  ///
  /// TODO T154818309
  template <typename UniquePtr>
  static typename std::enable_if<detail::IsUniquePtrToSL<UniquePtr>::value,
                                 std::unique_ptr<IOBuf>>::type
  TakeOwnership(UniquePtr&& buf, size_t count = 1);

  /// Convert an iovec array into an IOBuf.
  ///
  /// A helper that wraps a number of iovecs into an IOBuf chain.  If count ==
  /// 0, then a zero length buf is returned.  This function never returns
  /// nullptr.
  ///
  /// \param vec The iovec array to convert to an IOBuf chain.
  /// \param count The size of the iovec array.
  ///
  /// \methodset IOV
  static std::unique_ptr<IOBuf> WrapIov(const iovec* vec, size_t count);

  /// Take ownership of an iovec, turning it into an IOBuf.
  ///
  /// A helper that takes ownerships a number of iovecs into an IOBuf chain.  If
  /// count == 0, then a zero length buf is returned.  This function never
  /// returns nullptr.
  ///
  /// \param vec The iovec array to convert to an IOBuf chain.
  /// \param count The size of the iovec array.
  /// \param free_fn The function to call when buf is to be freed.
  /// \param user_data An additional arbitrary void* argument to supply to
  ///                  free_fn.
  /// \param free_on_error Whether the buffer should be freed if this function
  ///                      throws an exception.
  ///
  /// \methodset IOV
  static std::unique_ptr<IOBuf> takeOwnershipIov(const iovec* vec, size_t count,
                                                 FreeFunction free_fn = nullptr,
                                                 void* user_data = nullptr,
                                                 bool free_on_error = true);

  /// \copydoc IOBuf(WrapBufferOp, ByteRange)
  ///
  /// This static function behaves exactly like the WrapBufferOp constructor.
  /// It exists for syntactic parity with the unique_ptr-returning variants.
  ///
  /// \returns A stack-allocated IOBuf
  /// \methodset Makers
  static IOBuf WrapBufferAsValue(const void* buf,
                                 std::size_t capacity) noexcept;

  /// \copydoc wrapBufferAsValue(const void*, std::size_t)
  static IOBuf WrapBufferAsValue(ByteRange br) noexcept {
    return WrapBufferAsValue(br.data(), br.size());
  }

  /**
   * @copydoc IOBuf(CopyBufferOp, ByteRange, std::size_t, std::size_t)
   * @returns  A unique_ptr to a newly-constructed IOBuf
   * @methodset Makers
   */
  static std::unique_ptr<IOBuf> CopyBuffer(ByteRange br,
                                           std::size_t head_room = 0,
                                           std::size_t min_tail_room = 0) {
    return CopyBuffer(br.data(), br.size(), head_room, min_tail_room);
  }

  /// \copydoc CopyBuffer(ByteRange, std::size_t, std::size_t)
  static std::unique_ptr<IOBuf> CopyBuffer(const void* buf, std::size_t size,
                                           std::size_t head_room = 0,
                                           std::size_t min_tail_room = 0);

  /// \copydoc IOBuf(CopyBufferOp, ByteRange, std::size_t, std::size_t)
  ///
  /// Beware when attempting to invoke this function with a constant string
  /// literal and a headroom argument: you will likely end up invoking
  /// CopyBuffer(void* buf, size_t size).
  static std::unique_ptr<IOBuf> CopyBuffer(std::string_view buf,
                                           std::size_t head_room = 0,
                                           std::size_t min_tail_room = 0);

  /// \copydoc IOBuf(CopyBufferOp, ByteRange, std::size_t, std::size_t)
  ///
  /// This "maybe" version of copyBuffer returns null if the input is empty.
  ///
  /// \methodset Makers
  static std::unique_ptr<IOBuf> MaybeCopyBuffer(std::string_view buf,
                                                std::size_t head_room = 0,
                                                std::size_t min_tail_room = 0);

  /// Free an IOBuf.
  ///
  /// Note: as with all IOBuf destruction, this will also destroy all other
  /// IOBufs in the same chain.
  ///
  /// \param data The IOBuf to be destroyed.
  /// \post data will be nullptr.
  ///
  /// \methodset Memory
  static void Destroy(std::unique_ptr<IOBuf>&& data) {
    auto destroyer = std::move(data);
  }

  /// Get a good malloc size.
  ///
  /// Some malloc implementations will internally round up an allocation size to
  /// a convenient amount. For example, jemalloc(31) will actually return a
  /// buffer of size 32. Instead of wasting such tailroom, use it.
  ///
  /// \param min_capacity The malloc size to round up.
  /// \param combined Here be dragons. T154812262. The default value of DEFAULT
  ///                 is (a) hard to explain, and (b) probably not what you
  ///                 want. Refer to the code to see why.
  ///
  /// \returns A value at least as large as min_capacity. The overage, if any,
  ///          depends on the allocator.
  ///
  /// Note that IOBufs do this up-sizing for you: they will round up to the full
  /// allocation size and make that capacity available to you without your using
  /// this function. This just lets you introspect into that process, so you can
  /// for example figure out whether a given IOBuf can be usefully compacted.
  ///
  /// \methodset Memory
  static size_t GoodSize(size_t min_capacity,
                         CombinedOption combined = CombinedOption::kDefault);

  /// Create an IOBuf with the requested capacity.
  ///
  /// \param capacity The size of buffer to allocate.
  ///
  /// \post data() points to the start of the buffer.
  /// \post Length() == 0
  /// \post capacity() >= capacity (\see GoodSize for details on why
  ///       IOBuf sometimes allocates a larger buffer than requested)
  ///
  /// \throws std::bad_alloc on malloc failure
  IOBuf(CreateOp, std::size_t capacity);

  /// Create an IOBuf by taking ownership of an existing buffer.
  ///
  /// The IOBuf will assume ownership of the buffer, and free it by calling the
  /// specified FreeFunction when the last IOBuf pointing to this buffer is
  /// destroyed.
  ///    - The FreeFunction will be called like free_fn(buf, user_data)
  ///    - free_fn must not throw an exception
  ///    - If no free_fn is specified, then the buffer will be freed using
  ///      free().
  /// Note that this is UB if the buffer was allocated using `new`.
  ///
  /// \param buf The pointer to the buffer.
  /// \param capacity The size of the buffer.
  /// \param offset The position within the buffer at which the data begins;
  ///               for overloads without this parameter, it defaults to 0.
  /// \param length The amount of data already in buf; for overloads without
  ///               this parameter, it defaults to capacity.
  /// \param free_fn The function to call when buf is to be freed.
  /// \param user_data An additional arbitrary void* argument to supply to
  ///                  free_fn.
  /// \param free_on_error Whether the buffer should be freed if this function
  ///                      throws an exception.
  /// \param SizedFree For overloads specified by this enum type, use
  ///                  io_buf_free_cn(buf, capacity) as the free_fn
  ///
  /// \post data() points to buf+offset (in overloads without offset, offset
  ///       defaults to 0)
  /// \post Length() == length (in overloads without length, length defaults to
  ///       capacity)
  ///
  /// \throws std::bad_alloc on error
  ///
  /// \note If length is unspecified, it defaults to capacity, as opposed to
  ///       empty.
  /// \note free_on_error is not properly handled in all cases.
  IOBuf(TakeOwnershipOp op, void* buf, std::size_t capacity,
        FreeFunction free_fn = nullptr, void* user_data = nullptr,
        bool free_on_error = true)
      : IOBuf(op, buf, capacity, 0, capacity, free_fn, user_data,
              free_on_error) {}

  /// \copydoc IOBuf(TakeOwnershipOp, void*, std::size_t, FreeFunction, void*,
  ///                bool)
  IOBuf(TakeOwnershipOp op, void* buf, std::size_t capacity, std::size_t length,
        FreeFunction free_fn = nullptr, void* user_data = nullptr,
        bool free_on_error = true)
      : IOBuf(op, buf, capacity, 0, length, free_fn, user_data, free_on_error) {
  }

  /// \copydoc IOBuf(TakeOwnershipOp, void*, std::size_t, FreeFunction, void*,
  ///                bool)
  IOBuf(TakeOwnershipOp, void* buf, std::size_t capacity, std::size_t offset,
        std::size_t length, FreeFunction free_fn = nullptr,
        void* user_data = nullptr, bool free_on_error = true);

  /// \copydoc IOBuf(TakeOwnershipOp, void*, std::size_t, FreeFunction, void*,
  ///                bool)
  IOBuf(TakeOwnershipOp, SizedFree, void* buf, std::size_t capacity,
        std::size_t offset, std::size_t length, bool free_on_error = true);

  /// Create a new null buffer.
  ///
  /// This can be used to allocate an empty IOBuf on the stack.  It will have no
  /// space allocated for it.  This is generally useful only to later use move
  /// assignment to fill out the IOBuf.
  IOBuf() noexcept;

  /// Copy constructor.
  ///
  /// \see CloneAsValue()
  IOBuf(const IOBuf& other);

  /// Move constructor.
  ///
  /// In general, you should only ever move the head of an IOBuf chain.
  /// Internal nodes in an IOBuf chain are owned by the head of the chain, and
  /// should not be moved from.  (Technically, nothing prevents you from moving
  /// a non-head node, but the moved-to node will replace the moved-from node in
  /// the chain.  This has implications for ownership, since non-head nodes are
  /// owned by the chain head.  You are then responsible for relinquishing
  /// ownership of the moved-to node, and manually deleting the moved-from
  /// node.)
  IOBuf(IOBuf&& other) noexcept;

  /// Move assignment operator.
  ///
  /// With the assignment operator, the destination should be the head of an
  /// IOBuf chain or a solitary IOBuf not part of a chain.  If the destination
  /// is part of a chain, all other IOBufs in the chain will be deleted.
  IOBuf& operator=(IOBuf&& other) noexcept;

  /// Copy assignment operator.
  ///
  /// @copydetails operator=(IOBuf&&)
  /// \see cloneAsValue()
  IOBuf& operator=(const IOBuf& other);

  /// Create an IOBuf pointing to a buffer, without taking ownership.
  ///
  /// This should only be used when the caller knows the lifetime of the IOBuf
  /// object ahead of time and can ensure that all IOBuf objects that will point
  /// to this buffer will be destroyed before the buffer itself is destroyed.
  /// The buffer will not be freed automatically when the last IOBuf
  /// referencing it is destroyed.  It is the caller's responsibility to free
  /// the buffer after the last IOBuf has been destroyed.
  ///
  /// An IOBuf created using WrapBuffer() will always be reported as shared.
  /// Unshare() may be used to create a writable copy of the buffer.
  ///
  /// \param buf The pointer to the buffer
  /// \param capacity The size of the buffer
  /// \param br Can pass a ByteRange instead of {buf, capacity}
  ///
  /// \post data() points to buf.
  /// \post Length() == capacity.
  IOBuf(WrapBufferOp op, ByteRange br) noexcept;

  /// \copydoc IOBuf(WrapBufferOp, ByteRange)
  IOBuf(WrapBufferOp op, const void* buf, std::size_t capacity) noexcept;

  /// Create an IOBuf and copy data into the buffer.
  ///
  /// The IOBuf will have a newly-allocated buffer. That buffer shall be
  /// populated with data from the argument buffer.
  ///
  /// \param buf The buffer from which to copy data.
  /// \param size The size of the buffer from which to copy data.
  /// \param br Can pass a ByteRange in lieu of {buf, size}.
  /// \param head_room The amount of headroom to add to the destination buffer.
  /// \param min_tail_room The amount of tailroom to add to the destination
  ///                      buffer.
  ///
  /// \post data() points to a new buffer whose content is the same as buf
  /// \post Length() == size
  /// \post HeadRoom() == head_room
  /// \post TailRoom() >= min_tail_room
  ///
  /// \throws std::bad_alloc on error
  IOBuf(CopyBufferOp op, ByteRange br, std::size_t head_room = 0,
        std::size_t min_tail_room = 0);

  /// \copydoc IOBuf(CopyBufferOp, ByteRange, std::size_t, std::size_t)
  IOBuf(CopyBufferOp op, const void* buf, std::size_t size,
        std::size_t head_room = 0, std::size_t min_tail_room = 0);

  IOBuf(CopyBufferOp op, std::string_view buf, std::size_t headroom = 0,
        std::size_t minTailroom = 0)
      : IOBuf(op, buf.data(), buf.size(), headroom, minTailroom) {}

  /// Destroy this IOBuf.
  ///
  /// Deleting an IOBuf will automatically destroy all IOBufs in the chain.
  /// (All subsequent IOBufs in the chain are considered to be owned by the head
  /// of the chain.  Users should only explicitly delete the head of a chain.)
  ///
  /// When each individual IOBuf is destroyed, it will release its reference
  /// count on the underlying buffer.  If it was the last user of the buffer,
  /// the buffer will be freed.
  ~IOBuf();

  /// Check whether the chain is empty.
  ///
  /// This method is semantically equivalent to
  ///   i->ComputeChainDataLength() == 0
  /// but may run faster because it can short-circuit as soon as it
  /// encounters a buffer with Length() != 0
  ///
  /// \methodset Chaining
  bool empty() const;

  /// Get the pointer to the start of the data.
  ///
  /// \methodset Access
  const uint8_t* data() const { return data_; }

  /// Get a writable pointer to the start of the data.
  ///
  /// The caller is responsible for calling unshare() first to ensure that it is
  /// actually safe to write to the buffer.
  ///
  /// \methodset Access
  uint8_t* WritableData() { return data_; }

  /// Get the pointer to the end of the data.
  ///
  /// \methodset Access
  const uint8_t* Tail() const { return data_ + length_; }

  /// Get a writable pointer to the end of the data.
  ///
  /// The caller is responsible for calling Unshare() first to ensure that it is
  /// actually safe to write to the buffer.
  ///
  /// \methodset Access
  uint8_t* WritableTail() { return data_ + length_; }

  /// Get the size of the data for this individual IOBuf in the chain.
  ///
  /// Use ComputeChainDataLength() for the sum of data length for the full
  /// chain.
  ///
  /// @methodset Buffer Capacity
  std::size_t Length() const { return length_; }

  /// Get the amount of head room.
  ///
  /// \returns The number of bytes in the buffer before the start of the data.
  ///
  /// \methodset Buffer Capacity
  std::size_t HeadRoom() const { return std::size_t(data_ - Buffer()); }

  /// Get the amount of tail room.
  ///
  /// \returns The number of bytes in the buffer after the end of the data.
  ///
  /// \methodset Buffer Capacity
  std::size_t TailRoom() const { return std::size_t(BufferEnd() - Tail()); }

  /// Get the pointer to the start of the buffer.
  ///
  /// Note that this is the pointer to the very beginning of the usable buffer,
  /// not the start of valid data within the buffer.  Use the data() method to
  /// get a pointer to the start of the data within the buffer.
  ///
  /// \methodset Access
  const uint8_t* Buffer() const { return buf_; }

  /// Get a writable pointer to the start of the buffer.
  ///
  /// The caller is responsible for calling unshare() first to ensure that it is
  /// actually safe to write to the buffer.
  ///
  /// \methodset Access
  uint8_t* WritableBuffer() { return buf_; }

  /// Get the pointer to the end of the buffer.
  ///
  /// Note that this is the pointer to the very end of the usable buffer,
  /// not the end of valid data within the buffer.  Use the tail() method to
  /// get a pointer to the end of the data within the buffer.
  ///
  /// \methodset Access
  const uint8_t* BufferEnd() const { return buf_ + capacity_; }

  /// Get the total size of the buffer.
  ///
  /// This returns the total usable length of the buffer.  Use the length()
  /// method to get the length of the actual valid data in this IOBuf.
  ///
  /// \methodset Buffer Capacity
  std::size_t capacity() const { return capacity_; }

  /// Get a pointer to the next IOBuf in this chain.
  ///
  /// \methodset Chaining
  IOBuf* Next() { return next_; }

  /// \copydoc next()
  const IOBuf* Next() const { return next_; }

  /// Get a pointer to the previous IOBuf in this chain.
  ///
  /// \methodset Chaining
  IOBuf* Prev() { return prev_; }

  /// \copydoc prev
  const IOBuf* Prev() const { return prev_; }

  /// Shift the data forwards in the buffer.
  ///
  /// This shifts the data pointer forwards in the buffer to increase the
  /// headroom.  This is commonly used to increase the headroom in a newly
  /// allocated buffer.
  ///
  /// The caller is responsible for ensuring that there is sufficient
  /// tailroom in the buffer before calling advance().
  ///
  /// If there is a non-zero data length, advance() will use memmove() to shift
  /// the data forwards in the buffer.  In this case, the caller is responsible
  /// for making sure the buffer is unshared, so it will not affect other IOBufs
  /// that may be sharing the same underlying buffer.
  ///
  /// \param amount The amount by which to shift all data forward.
  /// \post Length() is unchanged.
  ///
  /// \methodset Shifting
  void Advance(std::size_t amount) {
    // In debug builds, assert if there is a problem.
    assert(amount <= TailRoom());

    if (length_ > 0) {
      memmove(data_ + amount, data_, length_);
    }

    data_ += amount;
  }

  /// Shift the data backwards in the buffer.
  ///
  /// This shifts the data pointer backwards in the buffer, decreasing the
  /// headroom.
  ///
  /// The caller is responsible for ensuring that there is sufficient headroom
  /// in the buffer before calling retreat().
  ///
  /// If there is a non-zero data length, retreat() will use memmove() to shift
  /// the data backwards in the buffer.  In this case, the caller is responsible
  /// for making sure the buffer is unshared, so it will not affect other IOBufs
  /// that may be sharing the same underlying buffer.
  ///
  /// \param amount The amount by which to shift all data backward
  /// \post Length() is unchanged.
  ///
  /// \methodset Shifting
  void Retreat(std::size_t amount) {
    // In debug builds, assert if there is a problem.
    assert(amount <= HeadRoom());

    if (length_ > 0) {
      memmove(data_ - amount, data_, length_);
    }

    data_ -= amount;
  }

  /// Adjust the data pointer to include more valid data at the beginning.
  ///
  /// This moves the data pointer backwards to include more of the available
  /// buffer.  The caller is responsible for ensuring that there is sufficient
  /// headroom for the new data.  The caller is also responsible for populating
  /// this section with valid data.
  ///
  /// This does not modify any actual data in the buffer.
  ///
  /// \param amount The amount by which to shift the data() pointer backward.
  /// \post Length() is increased by amount.
  ///
  /// @methodset Shifting
  void prepend(std::size_t amount) {
    DCHECK_LE(amount, HeadRoom());

    data_ -= amount;
    length_ += amount;
  }

  /// Adjust the tail pointer to include more valid data at the end.
  ///
  /// This moves the tail pointer forwards to include more of the available
  /// buffer.  The caller is responsible for ensuring that there is sufficient
  /// tailroom for the new data.  The caller is also responsible for populating
  /// this section with valid data.
  ///
  /// This does not modify any actual data in the buffer.
  ///
  /// \param amount The amount by which to shift the tail() pointer forward.
  /// \post Length() is increased by amount.
  ///
  /// \methodset Shifting
  void Append(std::size_t amount) {
    DCHECK_LE(amount, TailRoom());

    length_ += amount;
  }

  /// Adjust the data pointer to include less valid data.
  ///
  /// This moves the data pointer forwards so that the first amount bytes are no
  /// longer considered valid data.  The caller is responsible for ensuring that
  /// amount is less than or equal to the actual data length.
  ///
  /// This does not modify any actual data in the buffer.
  ///
  /// \param amount The amount by which to shift the data() pointer forward.
  /// \post Length() is decreased by amount.
  ///
  /// \methodset Shifting
  void TrimStart(std::size_t amount) {
    DCHECK_LE(amount, length_);

    data_ += amount;
    length_ -= amount;
  }

  /// Adjust the tail pointer backwards to include less valid data.
  ///
  /// This moves the tail pointer backwards so that the last amount bytes are no
  /// longer considered valid data.  The caller is responsible for ensuring that
  /// amount is less than or equal to the actual data length.
  ///
  /// This does not modify any actual data in the buffer.
  ///
  /// \param amount The amount by which to shift the tail() pointer backward.
  /// \post Length() is decreased by amount.
  ///
  /// \methodset Shifting
  void TrimEnd(std::size_t amount) {
    DCHECK_LE(amount, length_);

    length_ -= amount;
  }

  /// Adjust the buffer end pointer to reduce the buffer capacity.
  ///
  /// This can be used to pass the ownership of the writable tail to another
  /// IOBuf.
  ///
  /// \param amount The amount by which to shift the bufferEnd() pointer
  ///               backward.
  /// \post capacity() is decreased by amount.
  ///
  /// \methodset Shifting
  void TrimWritableTail(std::size_t amount) {
    DCHECK_LE(amount, TailRoom());

    capacity_ -= amount;
  }

  /// Clear the buffer.
  ///
  /// \post data() == Buffer().
  /// \post Length() == 0.
  void clear() {
    data_ = WritableBuffer();
    length_ = 0;
  }

  /// Ensure that the buffer has enough free space.
  ///
  /// Ensure that this buffer has at least minHeadroom headroom bytes and at
  /// least minTailroom tailroom bytes.  The buffer must be writable
  /// (you must call unshare() before this, if necessary).
  ///
  /// This might involve a reallocation of the underlying buffer.
  ///
  /// \param min_head_room The requested amount of headroom.
  /// \param min_tail_room The requested amount of tailroom.
  ///
  /// \post HeadRoom() >= min_head_room.
  /// \post TailRoom() >= min_tail_room.
  /// \post The contents between data() and tail() are preseved.
  ///
  /// \methodset Buffer Capacity
  void reserve(std::size_t min_head_room, std::size_t min_tail_room) {
    // Maybe we don't need to do anything.
    if (HeadRoom() >= min_head_room && TailRoom() >= min_tail_room) {
      return;
    }

    // If the buffer is empty but we have enough total room (head + tail),
    // move the data_ pointer around.
    if (Length() == 0 &&
        HeadRoom() + TailRoom() >= min_head_room + min_tail_room) {
      data_ = WritableBuffer() + min_head_room;

      return;
    }

    // Bah, we have to do actual work.
    ReserveSlow(min_head_room, min_tail_room);
  }

  /// Is this IOBuf part of a chain.
  ///
  /// Technically, all IOBufs are part of a chain, possibly of length 1. This
  /// functin checks if the chain is non-trivial, i.e. the chain has more than
  /// just one IOBuf in it.
  ///
  /// \returns  true iff the the IOBuf's chain has more than 1 IOBuf in it
  ///
  /// \methodset Chaining
  bool IsChained() const {
    assert((next_ == this) == (prev_ == this));

    return next_ != this;
  }

  /// Get the number of IOBufs in this chain.
  ///
  /// Beware that this method has to walk the entire chain.
  /// Use isChained() if you just want to check if this IOBuf is part of a chain
  /// or not.
  ///
  /// \methodset Chaining
  size_t CountChainElements() const;

  /// Get the length of all the data in this IOBuf chain.
  ///
  /// Beware that this method has to walk the entire chain.
  ///
  /// \methodset Chaining
  std::size_t ComputeChainDataLength() const;

  /// Get the capacity all IOBufs in the chain.
  ///
  /// Beware that this method has to walk the entire chain.
  ///
  /// \methodset Chaining
  std::size_t ComputeChainCapacity() const;

  /// Append another IOBuf chain to the end of this chain.
  ///
  /// For example, if there are two IOBuf chains (A, B, C) and (D, E, F),
  /// and A->appendToChain(D) is called, the (D, E, F) chain will be subsumed
  /// and become part of the chain starting at A, which will now look like
  /// (A, B, C, D, E, F).
  ///
  /// \methodset Chaining
  void AppendToChain(std::unique_ptr<IOBuf>&& iobuf);

  /// Insert an IOBuf chain immediately after this chain element.
  ///
  /// For example, if there are two IOBuf chains (A, B, C) and (D, E, F),
  /// and B->insertAfterThisOne(D) is called, the (D, E, F) chain will be
  /// subsumed and become part of the chain starting at A, which will now look
  /// like (A, B, D, E, F, C)
  ///
  /// Note if X is an IOBuf chain with just a single element, X->appendToChain()
  /// and X->insertAfterThisOne() behave identically.
  ///
  /// \methodset Chaining
  void InsertAfterThisOne(std::unique_ptr<IOBuf>&& iobuf) {
    // Just use appendToChain() on the next element in our chain
    next_->AppendToChain(std::move(iobuf));
  }

  /// Deprecated name for appendToChain()
  ///
  /// IOBuf chains are circular, so appending to the end of the chain is
  /// logically equivalent to prepending to the current head (but keeping the
  /// chain head pointing to the same element).  That was the reason this method
  /// was originally called prependChain().  However, almost every time this
  /// method is called the intent is to append to the end of a chain, so the
  /// `PrependChain()` name is very confusing to most callers.
  ///
  /// \methodset Chaining
  void PrependChain(std::unique_ptr<IOBuf>&& iobuf) {
    AppendToChain(std::move(iobuf));
  }

  /// Deprecated name for insertAfterThisOne()
  ///
  /// Beware: appendToChain() and appendChain() are two different methods,
  /// and you probably want appendToChain() instead of this one.
  ///
  /// \methodset Chaining
  void AppendChain(std::unique_ptr<IOBuf>&& iobuf) {
    InsertAfterThisOne(std::move(iobuf));
  }

  /// Remove this IOBuf from its current chain.
  ///
  /// Ownership of all elements an IOBuf chain is normally maintained by
  /// the head of the chain. Unlink() transfers ownership of this IOBuf from the
  /// chain and gives it to the caller.  A new unique_ptr to the IOBuf is
  /// returned to the caller.  The caller must store the returned unique_ptr (or
  /// call release() on it) to take ownership, otherwise the IOBuf will be
  /// immediately destroyed.
  ///
  /// Since Unlink() transfers ownership of the IOBuf to the caller, be careful
  /// not to call Unlink() on the head of a chain if you already maintain
  /// ownership on the head of the chain via other means.  The pop() method
  /// is a better choice for that situation.
  ///
  /// \methodset Chaining
  std::unique_ptr<IOBuf> Unlink() {
    next_->prev_ = prev_;
    prev_->next_ = next_;
    prev_ = this;
    next_ = this;

    return std::unique_ptr<IOBuf>(this);
  }

  /// Remove the rest of the chain from this IOBuf.
  ///
  /// Ownership of all elements an IOBuf chain is normally maintained by
  /// the head of the chain. pop() transfers ownership of the rest of the chain
  /// to the caller.
  ///
  /// Since pop() transfers ownership of the rest to the caller, be careful
  /// not to call pop() except on the head of a chain.
  ///
  /// @returns A new unique_ptr pointing to the rest of the chain; nullptr if
  ///          this IOBuf was the only chain element.
  std::unique_ptr<IOBuf> pop() {
    IOBuf* next = next_;
    next_->prev_ = prev_;
    prev_->next_ = next_;
    prev_ = this;
    next_ = this;

    return std::unique_ptr<IOBuf>((next == this) ? nullptr : next);
  }

  /// Remove a subchain from this chain.
  ///
  /// Remove the subchain starting at head and ending at tail from this chain.
  /// This is inclusive of tail.
  ///
  /// If you have a chain (A, B, C, D, E, F), and you call A->separateChain(B,
  /// D), then you will be returned the chain (B, C, D) and the current IOBuf
  /// chain will change to (A, E, F).
  ///
  /// Returns a unique_ptr pointing to the new head.  (In other words, ownership
  /// of the head of the subchain is transferred to the caller.)  If the caller
  /// ignores the return value and lets the unique_ptr be destroyed, the
  /// subchain will be immediately destroyed.
  ///
  /// head may equal tail. In this case, the subchain of length 1 is removed.
  ///
  /// \pre head and tail are part of the current IOBuf chain.
  /// \pre head and tail are not equal to the current IOBuf.
  ///
  /// \param head The first IOBuf chain element to remove.
  /// \param tail The last IOBuf chain element to remove (inclusive).
  ///
  /// \methodset Chaining
  std::unique_ptr<IOBuf> SeparateChain(IOBuf* head, IOBuf* tail) {
    assert(head != this);
    assert(tail != this);

    head->prev_->next_ = tail->next_;
    tail->next_->prev_ = head->prev_;

    head->prev_ = tail;
    tail->next_ = head;

    return std::unique_ptr<IOBuf>(head);
  }

  /// Check if any chain buffers are shared.
  ///
  /// Return true if at least one of the IOBufs in this chain are shared,
  /// or false if all of the IOBufs point to unique buffers.
  ///
  /// Use isSharedOne() to only check this IOBuf rather than the entire chain.
  ///
  /// If isShared() returns false, then you are probably the sole owner of the
  /// IOBuf chain and can write to it without needing to call unshare(). This is
  /// not a guarantee, since it is possible for another thread to concurrently
  /// acquire shared ownership.
  ///
  /// \methodset Buffer Management
  bool IsShared() const {
    const IOBuf* current = this;

    while (true) {
      if (current->IsSharedOne()) {
        return true;
      }

      current = current->next_;

      if (current == this) {
        return false;
      }
    }
  }

  /// Get userData.
  ///
  /// userData is the optional constructor argument which will be passed to the
  /// FreeFunction.
  ///
  /// \returns A non-owning pointer to userData if set, else nullptr
  ///
  /// \methodset Buffer Management
  void* GetUserData() const noexcept {
    auto info = SharedInfo();

    return info ? info->user_data : nullptr;
  }

  /// Get the FreeFunction.
  ///
  /// freeFn is the optional constructor argument which shall be called when the
  /// buffer is to be destroyed.
  ///
  /// \returns A non-owning pointer to freeFn if set, else nullptr.
  ///
  /// \methodset Buffer Management
  FreeFunction GetFreeFn() const noexcept {
    auto info = SharedInfo();

    return info ? info->free_fn : nullptr;
  }

  /// Add an Observer to the refcount block (SharedInfo).
  ///
  /// \param observer The observer to add to SharedInfo
  /// \returns true iff the observer was added (if there is no SharedInfo,
  ///          there's nothing to observe)
  ///
  /// \methodset Misc
  template <typename Observer>
  bool AppendSharedInfoObserver(Observer&& observer) {
    auto info = SharedInfo();

    if (!info) {
      return false;
    }

    auto* entry =
        new SharedInfoObserverEntry<Observer>(std::forward<Observer>(observer));
    std::lock_guard<std::mutex> guard(info->observer_list_lock);

    if (!info->observer_list_head) {
      info->observer_list_head = entry;
    } else {
      // prepend
      entry->next = info->observer_list_head;
      entry->prev = info->observer_list_head->prev;
      info->observer_list_head->prev->next = entry;
      info->observer_list_head->prev = entry;
    }

    return true;
  }

  /// Check if all IOBufs in this chain use the standard refcounting mechanism.
  ///
  /// If so, then the lifetime of the underlying memory can be extended by
  /// Clone().
  ///
  /// \returns true iff all IOBufs in this chain are IsManagedOne().
  ///
  /// \methodset Buffer Management
  bool IsManaged() const {
    const IOBuf* current = this;

    while (true) {
      if (!current->IsManagedOne()) {
        return false;
      }

      current = current->next_;

      if (current == this) {
        return true;
      }
    }
  }

  /// Check if this IOBuf uses the standard refcounting mechanism.
  ///
  /// If so, then the lifetime of the underlying memory can be extended by
  /// CloneOne().
  ///
  /// \returns true iff the current IOBuf was allocated normally (without the
  ///          user specifying special memory semantics, such as with a
  ///          user-owned buffer)
  ///
  /// \methodset Buffer Management
  bool IsManagedOne() const noexcept { return SharedInfo(); }

  /// Inconsistently get the reference count.
  ///
  /// For most of the use-cases where it seems like a good idea to call this
  /// function, what you really want is isSharedOne().
  ///
  /// If this IOBuf is managed by the usual refcounting mechanism (ie
  /// isManagedOne() returns true) then this returns the reference count as it
  /// was when recently observed by this thread.
  ///
  /// If this IOBuf is *not* managed by the usual refcounting mechanism then the
  /// result of this function is not defined.
  ///
  /// This only checks the current IOBuf, and not other IOBufs in the chain.
  ///
  /// \methodset Buffer Management
  uint32_t ApproximateShareCountOne() const;

  /// Check if the buffer is shared.
  ///
  /// IOBuf buffers can be shared (using refcounting). Check if any other IOBufs
  /// are pointing to this same buffer.
  ///
  /// If this IOBuf points at a buffer owned by another (non-IOBuf) part of the
  /// code (i.e., if the IOBuf was created using wrapBuffer(), or was cloned
  /// from such an IOBuf), it is always considered shared.
  ///
  /// This only checks the current IOBuf, and not other IOBufs in the chain.
  ///
  /// \methodset Buffer Management
  bool IsSharedOne() const noexcept {
    auto info = SharedInfo();

    // If this is a user-owned buffer, it is always considered shared.
    if (!info) [[unlikely]] {
      return true;
    }

    if (info->externally_shared) [[unlikely]] {
      return true;
    }

    return info->refcount.load(std::memory_order_acquire) > 1;
  }

  /// Ensure that this IOBuf chain has unique, unshared buffers.
  ///
  /// Multiple IOBufs can point to the same buffer. This means that an IOBuf's
  /// buffer is not necessarily writeable, since another IOBuf might be using
  /// the same underlying data. If you want to write to an IOBuf's buffer, it is
  /// your responsibility to make sure that you aren't trampling the data used
  /// by another IOBuf. This can be accomplished by calling Unshare().
  ///
  /// Unshare() ensures that the underlying buffer of each IOBuf in the chain is
  /// not shared with another IOBuf.
  ///
  /// \note If the current chain has any shared buffers, then Unshare() might
  ///       coalesce the chain during unsharing.
  /// \note Buffers owned by other (non-IOBuf) users are automatically
  ///       considered to be shared.
  ///
  /// \post  The buffers in this IOBuf chain are all writeable, since they are
  ///        uniquely owned by the current IOBuf.
  ///
  /// \throws std::bad_alloc on error.  On error the IOBuf chain will be
  ///         unmodified.
  ///
  /// Currently unshare may also throw std::overflow_error if it tries to
  /// coalesce.  (TODO: In the future it would be nice if Unshare() were smart
  /// enough not to coalesce the entire buffer if the data is too large.
  /// However, in practice this seems unlikely to become an issue.)
  ///
  /// \methodset Buffer Management
  void Unshare() {
    if (IsChained()) {
      UnshareChained();
    } else {
      UnshareOne();
    }
  }

  /// Ensure that this IOBuf has a unique, unshared buffer.
  ///
  /// UnshareOne() operates on a single IOBuf object.  This IOBuf will have a
  /// unique buffer after UnshareOne() returns, but other IOBufs in the chain
  /// may still be shared after UnshareOne() returns.
  ///
  /// \throws std::bad_alloc on error.  On error the IOBuf will be unmodified.
  ///
  /// \methodset Buffer Management
  void UnshareOne() {
    if (IsSharedOne()) {
      UnshareOneSlow();
    }
  }

  /// Mark the underlying buffers in this chain as shared.
  ///
  /// Assume that the underlying buffers are also owned by an external memory
  /// management mechanism. This will make IsShared() always returns true.
  ///
  /// This function is not thread-safe, and only safe to call immediately after
  /// creating an IOBuf, before it has been shared with other threads.
  ///
  /// \methodset Buffer Management
  void MarkExternallyShared();

  /// Mark the underlying buffer as shared.
  ///
  /// Assume that the underlying buffer is also owned by an external memory
  /// management mechanism. This will make isSharedOne() always returns true.
  ///
  /// This function is not thread-safe, and only safe to call immediately after
  /// creating an IOBuf, before it has been shared with other threads.
  ///
  /// \methodset Buffer Management
  void MarkExternallySharedOne() {
    auto info = SharedInfo();

    if (info) {
      info->externally_shared = true;
    }
  }

  /// Ensure that the buffers are owned by the IOBuf chain.
  ///
  /// It is possible for an IOBuf to be constructed with a user-owned buffer. In
  /// such circumstances, the user is responsible for ensuring that the buffer
  /// outlives the IOBuf. MakeManaged() lets the user subsequently reallocate
  /// the buffer to be owned by the IOBuf directly.
  ///
  /// If the buffers are already owned by IOBuf, then this function doesn't need
  /// to do anything.
  ///
  /// \methodset Buffer Management
  void MakeManaged() {
    if (IsChained()) {
      MakeManagedChained();
    } else {
      MakeManagedOne();
    }
  }

  /// Ensure that the buffer is owned by the IOBuf.
  ///
  /// It is possible for an IOBuf to be constructed with a user-owned buffer. In
  /// such circumstances, the user is responsible for ensuring that the buffer
  /// outlives the IOBuf. MakeManaged() lets the user subsequently reallocate
  /// the buffer to be owned by the IOBuf directly.
  ///
  /// If the buffer is already owned by IOBuf, then this function doesn't need
  /// to do anything.
  ///
  /// \methodset Buffer Management
  void MakeManagedOne() {
    if (!IsManagedOne()) {
      // We can call the internal function directly; unmanaged implies shared.
      UnshareOneSlow();
    }
  }

  /// Coalesce this IOBuf chain into a single buffer.
  ///
  /// This method moves all of the data in this IOBuf chain into a single
  /// contiguous buffer, if it is not already in one buffer.  After Coalesce()
  /// returns, this IOBuf will be a chain of length one.  Other IOBufs in the
  /// chain will be automatically deleted.
  ///
  /// After coalescing, the IOBuf will have at least as much headroom as the
  /// first IOBuf in the chain, and at least as much tailroom as the last IOBuf
  /// in the chain.
  ///
  /// \post IsChained() == false
  ///
  /// \throws std::bad_alloc on error.  On error the IOBuf chain will be
  ///         unmodified.
  ///
  /// \returns A ByteRange that points to the now-contiguous buffer data()
  ///
  /// \methodset Chaining
  ByteRange Coalesce() {
    if (IsChained()) {
      const std::size_t new_head_room = HeadRoom();
      const std::size_t new_tail_room = Prev()->TailRoom();

      CoalesceAndReallocate(new_head_room, ComputeChainDataLength(), this,
                            new_tail_room);
    }

    return ByteRange(data_, length_);
  }

  /// \copydoc Coalesce()
  ///
  /// \param new_head_room How much headroom the new coalesced chain should
  ///                      have, instead of mimicking the original headroom.
  /// \param new_tail_room How much tailroom the new coalesced chain should
  ///                      have, instead of mimicking the original tailroom.
  ByteRange CoalesceWithHeadroomTailroom(std::size_t new_head_room,
                                         std::size_t new_tail_room) {
    if (IsChained()) {
      CoalesceAndReallocate(new_head_room, ComputeChainDataLength(), this,
                            new_tail_room);
    }

    return ByteRange(data_, length_);
  }

  /// Ensure that this chain has at least contiguousLength bytes available as a
  /// contiguous memory range.
  ///
  /// This method coalesces whole buffers in the chain into this buffer as
  /// necessary until this buffer's length() is at least contiguousLength.
  ///
  /// After coalescing, the IOBuf will have at least as much headroom as the
  /// first IOBuf in the chain, and at least as much tailroom as the last IOBuf
  /// that was coalesced.
  ///
  /// \throws std::bad_alloc or std::overflow_error on error.  On error the
  ///         IOBuf chain will be unmodified.
  /// \throws std::overflow_error if contiguousLength is longer than the total
  ///         chain length.
  ///
  /// \post Length() >= contiguousLength
  ///
  /// \methodset Chaining
  void Gather(std::size_t contiguous_length) {
    if (!IsChained() || length_ >= contiguous_length) {
      return;
    }

    CoalesceSlow(contiguous_length);
  }

  /// Copy an IOBuf chain.
  ///
  /// This is a shallow buffer copy; the source buffers will be shared.
  ///
  /// The new IOBuf chain will normally point to the same underlying data
  /// buffers as the original chain.  (The one exception to this is if some of
  /// the IOBufs in this chain contain small internal data buffers which cannot
  /// be shared.)
  ///
  /// \methodset Makers
  std::unique_ptr<IOBuf> Clone() const;

  /// \copydoc Clone()
  ///
  /// Similar to Clone(), but returns by value rather than heap-allocating.
  IOBuf CloneAsValue() const;

  /// Copy an individual IOBuf.
  ///
  /// Only clone the buffer of the current IOBuf; ignore chained IOBufs.
  ///
  /// \methodset Makers
  std::unique_ptr<IOBuf> CloneOne() const;

  /// \copydoc CloneOne()
  ///
  /// Similar to CloneOne(), but returns by value rather than heap-allocating.
  IOBuf CloneOneAsValue() const;

  /// Copy an IOBuf chain into a single buffer.
  ///
  /// Semantically similar to .Clone().Coalesce(), but without the intermediate
  /// allocations.
  ///
  /// The new IOBuf will have at least as much headroom as the first IOBuf in
  /// the chain, and at least as much tailroom as the last IOBuf in the chain.
  ///
  /// \return  An IOBuf for which IsChained() == false, and whose data is the
  ///          same as Coalesce().
  ///
  /// \throws std::bad_alloc on error.
  ///
  /// \methodset Makers
  std::unique_ptr<IOBuf> CloneCoalesced() const;

  /// \copydoc CloneCoalesced()
  ///
  /// \param new_head_room How much headroom the new coalesced chain should
  ///                      have, instead of mimicking the original headroom.
  /// \param new_tail_room How much tailroom the new coalesced chain should
  ///                      have, instead of mimicking the original tailroom.
  std::unique_ptr<IOBuf> CloneCoalescedWithHeadroomTailroom(
      std::size_t new_head_room, std::size_t new_tail_room) const;

  /// \copydoc CloneCoalesced()
  ///
  /// Similar to cloneCoalesced(), but returns by value rather than
  /// heap-allocating.
  IOBuf CloneCoalescedAsValue() const;

  /// \copydoc
  /// cloneCoalescedWithHeadroomTailroom(std::size_t, std::size_t) const
  ///
  /// Similar to CloneCoalescedWithHeadroomTailroom(), but returns by value
  /// rather than heap-allocating.
  IOBuf CloneCoalescedAsValueWithHeadroomTailroom(
      std::size_t new_head_room, std::size_t new_tail_room) const;

  /// \copydoc Clone()
  ///
  /// Similar to Clone(), but returns by argument. The argument will become the
  /// clone's head. Other nodes in the chain (if any) will be allocated on the
  /// heap as usual.
  ///
  /// \param[out] other An IOBuf to assign the clone to.
  void CloneInto(IOBuf& other) const { other = CloneAsValue(); }

  /// \copydoc CloneOne()
  ///
  /// Similar to CloneOne(), but returns by argument. The argument will become
  /// the clone.
  ///
  /// @param[out] other An IOBuf to assign the clone to.
  void CloneOneInto(IOBuf& other) const { other = CloneOneAsValue(); }

  /// Append the chain data into the provided container.
  ///
  /// This is meant to be used with containers such as std::string or
  /// std::vector<char>, but any container which supports reserve(), insert(),
  /// and has char or unsigned char value type is supported.
  ///
  /// \methodset Conversions
  template <typename Container>
  void AppendTo(Container& container) const;

  /// Dump the chain data into a container.
  ///
  /// \copydetails AppendTo(Container&) const
  ///
  /// \tparam Container The type of container to return.
  ///
  /// \returns A Container whose data equals the coalseced data of this chain.
  template <typename Container>
  Container To() const;

  /// Get an iovector suitable for e.g. writev().
  ///
  /// \code
  ///   auto iov = buf->GetIov();
  ///   auto xfer = writev(fd, iov.data(), iov.size());
  /// \endcode
  ///
  /// Naturally, the returned iovector is invalid if you modify the buffer
  /// chain.
  ///
  /// \methodset IOV
  std::vector<struct iovec> GetIov() const;

  /// Update an existing iovec array with the IOBuf data.
  ///
  /// New iovecs will be appended to the existing vector; anything already
  /// present in the vector will be left unchanged.
  ///
  /// Naturally, the returned iovec data will be invalid if you modify the
  /// buffer chain.
  ///
  /// \param[out] iov The iovector to append to.
  ///
  /// \methodset IOV
  void AppendToIov(std::vector<struct iovec>* iov) const;

  struct FillIovResult {
    /// How many iovecs were filled (or 0 on error).
    size_t num_iovecs;
    /// The total length of filled iovecs (or 0 on error).
    size_t total_length;
  };

  /// Fill an iovec array with the IOBuf data.
  ///
  /// Returns a struct with two fields: the number of iovec filled, and total
  /// size of the iovecs filled. If there are more buffer than iovec, returns 0
  /// in both fields.
  /// This version is suitable to use with stack iovec arrays.
  ///
  /// Naturally, the filled iovec data will be invalid if you modify the
  /// buffer chain.
  ///
  /// \param[out] iov The iovector to append to
  /// \param len The size of the iov array
  ///
  /// \methodset IOV
  FillIovResult FillIov(struct iovec* iov, size_t len) const;

  /// Overridden operator new and delete.
  ///
  /// These perform specialized memory management to help support
  /// CreateCombined(), which allocates IOBuf objects together with the buffer
  /// data.
  ///
  /// \methodset Memory
  void* operator new(size_t size);

  /// Overridden operator new.
  /// \methodset Memory
  void* operator new(size_t size, void* ptr);

  /// Overridden operator delete.
  /// \methodset Memory
  void operator delete(void* ptr);

  /// Overridden operator delete.
  /// \methodset Memory
  void operator delete(void* ptr, void* placement);

  /// Destructively convert to an string.
  ///
  /// Destructively convert this IOBuf to a string efficiently.
  /// We rely on fbstring's AcquireMallocatedString constructor to
  /// transfer memory.
  ///
  /// \methodset Conversions
  std::string MoveToFbString();

  /// Iterate over the IOBufs in this chain.
  ///
  /// The iterators dereference to a ByteRange.
  ///
  /// \methodset Iterators
  Iterator cbegin() const;

  /// \copydoc cbegin()
  Iterator cend() const;

  /// \copydoc cbegin()
  Iterator begin() const;

  /// \copydoc cbegin()
  Iterator end() const;

 private:
  /// A pointer to the start of the data referenced by this IOBuf and the length
  /// of the data.
  /// This may refer to any subsection of the actual buffer capacity.
  std::size_t length_{0};
  uint8_t* data_{nullptr};

  std::size_t capacity_{0};
  uint8_t* buf_{nullptr};

  /// Links to the next and the previous IOBuf in this chain.
  ///
  /// The chain is circularly linked (the last element in the chain points back
  /// at the head), and next_ and prev_ can never be null.  If this IOBuf is the
  /// only element in the chain, next_ and prev_ will both point to this.
  IOBuf* next_{this};
  IOBuf* prev_{this};

  /// Pack flags in least significant 2 bits, sharedInfo in the rest.
  uintptr_t flags_and_shared_info_{0};
};

/// Hasher for IOBuf objects. Hashes the entire chain using SpookyHashV2.
struct IOBufHash {
  size_t operator()(const IOBuf& buf) const noexcept;
  size_t operator()(const std::unique_ptr<IOBuf>& buf) const noexcept {
    return operator()(buf.get());
  }
  size_t operator()(const IOBuf* buf) const noexcept {
    return buf ? (*this)(*buf) : 0;
  }
};

/// Ordering for IOBuf objects. Compares data in the entire chain.
struct IOBufCompare {
  std::strong_ordering operator()(const IOBuf& a, const IOBuf& b) const {
    return &a == &b ? std::strong_ordering::equal : impl(a, b);
  }

  std::strong_ordering operator()(const std::unique_ptr<IOBuf>& a,
                                  const std::unique_ptr<IOBuf>& b) const {
    return operator()(a.get(), b.get());
  }

  std::strong_ordering operator()(const IOBuf* a, const IOBuf* b) const {
    // clang-format off
    return
        !a && !b ? std::strong_ordering::equal :
        !a && b ?  std::strong_ordering::less :
        a && !b ?  std::strong_ordering::greater :
        operator()(*a, *b);
    // clang-format on
  }

 private:
  std::strong_ordering impl(IOBuf const& a, IOBuf const& b) const noexcept;
};

template <typename UniquePtr>
typename std::enable_if<detail::IsUniquePtrToSL<UniquePtr>::value,
                        std::unique_ptr<IOBuf>>::type
IOBuf::TakeOwnership(UniquePtr&& buf, size_t count) {
  size_t size = count * sizeof(typename UniquePtr::element_type);
  auto deleter = new UniquePtrDeleter<UniquePtr>(buf.get_deleter());

  return TakeOwnership(buf.release(), size, &IOBuf::FreeUniquePtrBuffer,
                       deleter);
}

inline std::unique_ptr<IOBuf> IOBuf::CopyBuffer(const void* data,
                                                std::size_t size,
                                                std::size_t head_room,
                                                std::size_t min_tail_room) {
  std::size_t capacity;

  if (!kiwi::checked_add(&capacity, size, head_room, min_tail_room)) {
    throw_exception(std::length_error(""));
  }

  std::unique_ptr<IOBuf> buf = Create(capacity);
  buf->Advance(head_room);

  if (size != 0) {
    memcpy(buf->WritableData(), data, size);
  }

  buf->Append(size);

  return buf;
}

inline std::unique_ptr<IOBuf> IOBuf::CopyBuffer(Sstd::string_view buf,
                                                std::size_t head_room,
                                                std::size_t min_tail_room) {
  return CopyBuffer(buf.data(), buf.size(), head_room, min_tail_room);
}

inline std::unique_ptr<IOBuf> IOBuf::MaybeCopyBuffer(
    std::string_view buf, std::size_t head_room, std::size_t min_tail_room) {
  if (buf.empty()) {
    return nullptr;
  }

  return CopyBuffer(buf.data(), buf.size(), head_room, min_tail_room);
}

template <typename Container>
void IOBuf::AppendTo(Container& container) const {
  static_assert(
    std::is_same_v<typename Container::value_type, uint8_t>, "Unsupported value type"
  );

  container.reserve(container.size() + ComputeChainDataLength());

  for (auto data : *this) {
    container.insert(container.end(), data.begin(), data.end());
  }
}

template <typename Container>
Container IOBuf::To() const {
  Container result;
  AppendTo(result);

  return result;
}

class IOBuf::Iterator
    : public detail::IteratorFacade<IOBuf::Iterator, ByteRange const,
                                    std::forward_iterator_tag> {
  void SetVal() { val_ = ByteRange(pos_->data(), pos_->Tail()); }

  void AdjustForEnd() {
    if (pos_ == end_) {
      pos_ = end_ = nullptr;
      val_ = ByteRange();
    } else {
      SetVal();
    }
  }

 public:
  // Note that IOBufs are stored as a circular list without a guard node,
  // so pos == end is ambiguous (it may mean "begin" or "end").  To solve
  // the ambiguity (at the cost of one extra comparison in the "increment"
  // code path), we define end iterators as having pos_ == end_ == nullptr
  // and we only allow forward iteration.
  explicit Iterator(const IOBuf* pos, const IOBuf* end) : pos_(pos), end_(end) {
    // Sadly, we must return by const reference, not by value.
    if (pos_) {
      SetVal();
    }
  }

  Iterator() = default;
  Iterator(Iterator const& rhs) : Iterator(rhs.pos_, rhs.end_) {}
  Iterator& operator=(Iterator const& rhs) {
    pos_ = rhs.pos_;
    end_ = rhs.end_;

    if (pos_) {
      SetVal();
    }

    return *this;
  }

  const ByteRange& Dereference() const { return val_; }
  bool IsEqual(const Iterator& other) const {
    // We must compare end_ in addition to pos_, because forward traversal
    // requires that if two iterators are equal (a == b) and dereferenceable,
    // then ++a == ++b.
    return pos_ == other.pos_ && end_ == other.end_;
  }

  void Increment() {
    pos_ = pos_->Next();
    AdjustForEnd();
  }

 private:
  const IOBuf* pos_{nullptr};
  const IOBuf* end_{nullptr};
  ByteRange val_;
};

inline IOBuf::Iterator IOBuf::begin() const { return cbegin(); }
inline IOBuf::Iterator IOBuf::end() const { return cend(); }

}  // namespace kiwi

KIWI_POP_WARNING
