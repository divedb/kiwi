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

#include "kiwi/io/uio.hh"

#include "kiwi/sys/unistd.hh"

// #include <folly/ScopeGuard.h>
// #include <folly/portability/Sockets.h>
// #include <folly/portability/SysFile.h>
// #include <folly/portability/SysUio.h>

#include <cerrno>
#include <cstdio>

#include "kiwi/portability/compiler_specific.hh"

/// Temporarily seeks to a given offset on a file descriptor, invokes a file
/// operation, then restores the original position.
///
/// \param func File operation function to be called with signature:
///             `int(int, args...)`
/// \param fd The file descriptor.
/// \param offset The offset in the file at which to perform the operation.
/// \param ...args Additional arguments to pass to the function `func`.
/// \return The result of calling `func`. If seeking fails, returns -1 and sets
///         `errno`.
template <typename Func, typename... Args>
static int WrapPositional(Func func, int fd, off_t offset, Args... args) {
  off_t orig_offset = lseek(fd, 0, SEEK_CUR);

  if (orig_offset == off_t(-1)) {
    return -1;
  }

  if (lseek(fd, offset, SEEK_SET) == off_t(-1)) {
    return -1;
  }

  int res = (int)func(fd, args...);
  int saved_errno = errno;

  if (lseek(fd, orig_offset, SEEK_SET) == off_t(-1)) {
    if (res == -1) {
      errno = saved_errno;
    }

    return -1;
  }

  errno = saved_errno;

  return res;
}

namespace {
#if !KIWI_HAVE_PREADV
ssize_t PReadvFallback(int fd, const iovec* iov, int count, off_t offset) {
  return static_cast<ssize_t>(WrapPositional(readv, fd, offset, iov, count));
}
#endif

#if !KIWI_HAVE_PWRITEV
ssize_t PWritevFallback(int fd, const iovec* iov, int count, off_t offset) {
  return static_cast<ssize_t>(WrapPositional(writev, fd, offset, iov, count));
}
#endif
}  // namespace

namespace kiwi {
#if !KIWI_HAVE_PREADV
ssize_t preadv(int fd, const iovec* iov, int count, off_t offset) {
  using sig = ssize_t(int, const iovec*, int, off_t);

  static auto the_preadv = []() -> sig* {
#if defined(__APPLE__) && KIWI_HAS_BUILTIN(__builtin_available) && \
    !TARGET_OS_SIMULATOR &&                                        \
    (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101600 ||                   \
     __IPHONE_OS_VERSION_MAX_ALLOWED >= 140000)
    if (__builtin_available(iOS 14.0, macOS 11.0, watchOS 7.0, tvOS 14.0, *)) {
      return &::preadv;
    }
#endif

    return &PReadvFallback;
  }();

  return the_preadv(fd, iov, count, offset);
}
#endif

#if !KIWI_HAVE_PWRITEV
ssize_t pwritev(int fd, const iovec* iov, int count, off_t offset) {
  using sig = ssize_t(int, const iovec*, int, off_t);
  static auto the_pwritev = []() -> sig* {
#if defined(__APPLE__) && KIWI_HAS_BUILTIN(__builtin_available) && \
    !TARGET_OS_SIMULATOR &&                                        \
    (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101600 ||                   \
     __IPHONE_OS_VERSION_MAX_ALLOWED >= 140000)
    if (__builtin_available(iOS 14.0, macOS 11.0, watchOS 7.0, tvOS 14.0, *)) {
      return &::pwritev;
    }
#endif
    return &PWritevFallback;
  }();

  return the_pwritev(fd, iov, count, offset);
}
#endif
}  // namespace kiwi

#ifdef _WIN32
template <bool isRead>
static ssize_t doVecOperation(int fd, const iovec* iov, int count) {
  if (!count) {
    return 0;
  }
  if (count < 0 || count > folly::kIovMax) {
    errno = EINVAL;
    return -1;
  }

  // We only need to worry about locking if the file descriptor is
  // not a socket. We have no way of locking sockets :(
  // The correct way to do this for sockets is via sendmsg/recvmsg,
  // but this is good enough for now.
  bool shouldLock = !folly::portability::sockets::is_fh_socket(fd);
  if (shouldLock && lockf(fd, F_LOCK, 0) == -1) {
    return -1;
  }
  SCOPE_EXIT {
    if (shouldLock) {
      lockf(fd, F_ULOCK, 0);
    }
  };

  ssize_t bytesProcessed = 0;
  int curIov = 0;
  void* curBase = iov[0].iov_base;
  size_t curLen = iov[0].iov_len;
  while (curIov < count) {
    ssize_t res = 0;
    if (isRead) {
      res = read(fd, curBase, (unsigned int)curLen);
      if (res == 0 && curLen != 0) {
        break;  // End of File
      }
    } else {
      res = write(fd, curBase, (unsigned int)curLen);
      // Write of zero bytes is fine.
    }

    if (res == -1) {
      return -1;
    }

    if (size_t(res) == curLen) {
      curIov++;
      if (curIov < count) {
        curBase = iov[curIov].iov_base;
        curLen = iov[curIov].iov_len;
      }
    } else {
      curBase = (void*)((char*)curBase + res);
      curLen -= res;
    }

    if (bytesProcessed + res < 0) {
      // Overflow
      errno = EINVAL;
      return -1;
    }
    bytesProcessed += res;
  }

  return bytesProcessed;
}

extern "C" ssize_t readv(int fd, const iovec* iov, int count) {
  return doVecOperation<true>(fd, iov, count);
}

extern "C" ssize_t writev(int fd, const iovec* iov, int count) {
  return doVecOperation<false>(fd, iov, count);
}
#endif
