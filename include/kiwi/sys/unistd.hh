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

#ifndef _WIN32

#include <unistd.h>

#if defined(__APPLE__) || defined(__EMSCRIPTEN__)

using off64_t = off_t;

/// The lseek64() library function uses a 64-bit type even when off_t is a
/// 32-bit type. Its  prototype (and the type off64_t) is available only
/// when one compiles with
///
/// \code
/// #define _LARGEFILE64_SOURCE
/// \endcode
///
/// NOTE: lseek64() was introduced to let 32-bit systems access files larger
/// than what a 32-bit off_t can handle. It’s available when
/// `_LARGEFILE64_SOURCE` is defined, or when `_FILE_OFFSET_BITS` is set to 64,
/// in which case lseek maps to lseek64.
off64_t lseek64(int fh, off64_t off, int orig);
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset);

#endif

#else

#include <folly/Portability.h>
#include <folly/portability/SysTypes.h>
#include <process.h>

// Locks or unlocks bytes of a file.
// int _locking(
//   int fd,
//   int mode,
//   long nbytes
// );
#include <sys/locking.h>

#include <cstdint>

// This is different from the normal headers because there are a few cases,
// such as close(), where we need to override the definition of an existing
// function. To avoid conflicts at link time, everything here is in a namespace
// which is then used globally.

#define _SC_PAGESIZE 1
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 2
#define _SC_NPROCESSORS_CONF 2

// Windows doesn't define these, but these are the correct values
// for Windows.
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// Windows is weird and doesn't actually defined these
// for the parameters to access, so we have to do it ourselves -_-...
#define F_OK 0
#define X_OK F_OK
#define W_OK 2
#define R_OK 4
#define RW_OK 6

#define F_LOCK _LK_LOCK
#define F_ULOCK _LK_UNLCK

namespace folly {
namespace portability {
namespace unistd {
using off64_t = int64_t;
int access(char const* fn, int am);
int chdir(const char* path);
int close(int fh);
int dup(int fh);
int dup2(int fhs, int fhd);
int fsync(int fd);
int ftruncate(int fd, off_t len);
char* getcwd(char* buf, int sz);
int getdtablesize();
int getgid();
pid_t getppid();
int getuid();
int isatty(int fh);
int lockf(int fd, int cmd, off_t len);
off_t lseek(int fh, off_t off, int orig);
off64_t lseek64(int fh, off64_t off, int orig);
ssize_t read(int fh, void* buf, size_t mcc);
int rmdir(const char* path);
int pipe(int pth[2]);
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
ssize_t pread64(int fd, void* buf, size_t count, off64_t offset);
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);
ssize_t readlink(const char* path, char* buf, size_t buflen);
void* sbrk(intptr_t i);
unsigned int sleep(unsigned int seconds);
long sysconf(int tp);
int truncate(const char* path, off_t len);
int usleep(unsigned int ms);
ssize_t write(int fh, void const* buf, size_t count);
}  // namespace unistd
}  // namespace portability
}  // namespace folly

FOLLY_PUSH_WARNING
FOLLY_CLANG_DISABLE_WARNING("-Wheader-hygiene")
/* using override */ using namespace folly::portability::unistd;
FOLLY_POP_WARNING
#endif
