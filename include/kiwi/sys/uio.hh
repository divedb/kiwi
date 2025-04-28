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

#pragma once

#include "kiwi/portability/build_config.hh"

#if BUILDFLAG(IS_POSIX)
#include <limits.h>
#include <sys/uio.h>
#else
#include <stdlib.h>

#define UIO_MAXIOV 16
#define IOV_MAX UIO_MAXIOV

struct iovec {
  void* iov_base;
  size_t iov_len;
};

extern "C" ssize_t readv(int fd, const iovec* iov, int count);
extern "C" ssize_t writev(int fd, const iovec* iov, int count);

#endif

namespace kiwi {

#if !KIWI_HAVE_PREADV
ssize_t preadv(int fd, const iovec* iov, int count, off_t offset);
#else
using ::preadv;
#endif

#if !KIWI_HAVE_PWRITEV
ssize_t pwritev(int fd, const iovec* iov, int count, off_t offset);
#else
using ::pwritev;
#endif

#ifdef IOV_MAX  // Not defined on Android
inline constexpr size_t kIovMax = IOV_MAX;
#else
inline constexpr size_t kIovMax = UIO_MAXIOV;
#endif

}  // namespace kiwi
