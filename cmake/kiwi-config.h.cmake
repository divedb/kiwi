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

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if !defined(KIWI_MOBILE)
#if defined(__ANDROID__) || \
    (defined(__APPLE__) &&  \
     (TARGET_IPHONE_SIMULATOR || TARGET_OS_SIMULATOR || TARGET_OS_IPHONE))
#define KIWI_MOBILE 1
#else
#define KIWI_MOBILE 0
#endif
#endif // KIWI_MOBILE

#cmakedefine KIWI_HAVE_PTHREAD 1
#cmakedefine KIWI_HAVE_PTHREAD_ATFORK 1

#cmakedefine KIWI_HAVE_LIBGFLAGS 1
#cmakedefine KIWI_UNUSUAL_GFLAGS_NAMESPACE 1
#cmakedefine KIWI_GFLAGS_NAMESPACE @KIWI_GFLAGS_NAMESPACE@

#cmakedefine KIWI_HAVE_LIBGLOG 1

#if __has_include(<features.h>)
#include <features.h>
#endif

#cmakedefine KIWI_HAVE_ACCEPT4 1
#cmakedefine01 KIWI_HAVE_GETRANDOM
#cmakedefine KIWI_HAVE_PREADV 1
#cmakedefine KIWI_HAVE_PWRITEV 1
#cmakedefine KIWI_HAVE_CLOCK_GETTIME 1
#cmakedefine KIWI_HAVE_PIPE2 1
#cmakedefine KIWI_HAVE_SENDMMSG 1
#cmakedefine KIWI_HAVE_RECVMMSG 1

#cmakedefine KIWI_HAVE_IFUNC 1
#cmakedefine KIWI_HAVE_UNALIGNED_ACCESS 1
#cmakedefine KIWI_HAVE_VLA 1
#cmakedefine01 KIWI_HAVE_WEAK_SYMBOLS
#cmakedefine KIWI_HAVE_LINUX_VDSO 1
#cmakedefine KIWI_HAVE_MALLOC_USABLE_SIZE 1
#cmakedefine KIWI_HAVE_INT128_T 1
#cmakedefine KIWI_HAVE_WCHAR_SUPPORT 1
#cmakedefine KIWI_HAVE_EXTRANDOM_SFMT19937 1
#cmakedefine HAVE_VSNPRINTF_ERRORS 1

#cmakedefine KIWI_HAVE_LIBUNWIND 1
#cmakedefine KIWI_HAVE_DWARF 1
#cmakedefine KIWI_HAVE_ELF 1
#cmakedefine KIWI_HAVE_SWAPCONTEXT 1
#cmakedefine KIWI_HAVE_BACKTRACE 1
#cmakedefine KIWI_USE_SYMBOLIZER 1
#define KIWI_DEMANGLE_MAX_SYMBOL_SIZE 1024

#cmakedefine KIWI_HAVE_SHADOW_LOCAL_WARNINGS 1

#cmakedefine KIWI_HAVE_LIBLZ4 1
#cmakedefine KIWI_HAVE_LIBLZMA 1
#cmakedefine KIWI_HAVE_LIBSNAPPY 1
#cmakedefine KIWI_HAVE_LIBZ 1
#cmakedefine KIWI_HAVE_LIBZSTD 1
#cmakedefine KIWI_HAVE_LIBBZ2 1

#cmakedefine01 KIWI_LIBRARY_SANITIZE_ADDRESS

#cmakedefine KIWI_SUPPORT_SHARED_LIBRARY 1

#cmakedefine01 KIWI_HAVE_LIBRT
