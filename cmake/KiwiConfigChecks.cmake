# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCXXCompilerFlag) 

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # clang only rejects unknown warning flags if -Werror=unknown-warning-option
  # is also specified.
  check_cxx_compiler_flag(
    -Werror=unknown-warning-option
    COMPILER_HAS_UNKNOWN_WARNING_OPTION)

  if (COMPILER_HAS_UNKNOWN_WARNING_OPTION)
    set(CMAKE_REQUIRED_FLAGS
      "${CMAKE_REQUIRED_FLAGS} -Werror=unknown-warning-option")
  endif()

  check_cxx_compiler_flag(-Wshadow-local COMPILER_HAS_W_SHADOW_LOCAL)
  check_cxx_compiler_flag(
    -Wshadow-compatible-local
    COMPILER_HAS_W_SHADOW_COMPATIBLE_LOCAL)

  if (COMPILER_HAS_W_SHADOW_LOCAL AND COMPILER_HAS_W_SHADOW_COMPATIBLE_LOCAL)
    set(KIWI_HAVE_SHADOW_LOCAL_WARNINGS ON)
    list(APPEND KIWI_CXX_FLAGS -Wshadow-compatible-local)
  endif()

  check_cxx_compiler_flag(-Wnoexcept-type COMPILER_HAS_W_NOEXCEPT_TYPE)
  if (COMPILER_HAS_W_NOEXCEPT_TYPE)
    list(APPEND KIWI_CXX_FLAGS -Wno-noexcept-type)
  endif()

  check_cxx_compiler_flag(
      -Wnullability-completeness
      COMPILER_HAS_W_NULLABILITY_COMPLETENESS)
  if (COMPILER_HAS_W_NULLABILITY_COMPLETENESS)
    list(APPEND KIWI_CXX_FLAGS -Wno-nullability-completeness)
  endif()

  check_cxx_compiler_flag(
      -Winconsistent-missing-override
      COMPILER_HAS_W_INCONSISTENT_MISSING_OVERRIDE)
  if (COMPILER_HAS_W_INCONSISTENT_MISSING_OVERRIDE)
    list(APPEND KIWI_CXX_FLAGS -Wno-inconsistent-missing-override)
  endif()

  check_cxx_compiler_flag(-faligned-new COMPILER_HAS_F_ALIGNED_NEW)
  if (COMPILER_HAS_F_ALIGNED_NEW)
    list(APPEND KIWI_CXX_FLAGS -faligned-new)
  endif()

  check_cxx_compiler_flag(-fopenmp COMPILER_HAS_F_OPENMP)
  if (COMPILER_HAS_F_OPENMP)
      list(APPEND KIWI_CXX_FLAGS -fopenmp)
  endif()
endif()

set(KIWI_ORIGINAL_CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
string(REGEX REPLACE
  "-std=(c|gnu)\\+\\+.."
  ""
  CMAKE_REQUIRED_FLAGS
  "${CMAKE_REQUIRED_FLAGS}")

check_symbol_exists(pthread_atfork pthread.h KIWI_HAVE_PTHREAD_ATFORK)

list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(accept4 sys/socket.h KIWI_HAVE_ACCEPT4)
check_symbol_exists(getrandom sys/random.h KIWI_HAVE_GETRANDOM)
check_symbol_exists(preadv sys/uio.h KIWI_HAVE_PREADV)
check_symbol_exists(pwritev sys/uio.h KIWI_HAVE_PWRITEV)
check_symbol_exists(clock_gettime time.h KIWI_HAVE_CLOCK_GETTIME)
check_symbol_exists(pipe2 unistd.h KIWI_HAVE_PIPE2)
check_symbol_exists(sendmmsg sys/socket.h KIWI_HAVE_SENDMMSG)
check_symbol_exists(recvmmsg sys/socket.h KIWI_HAVE_RECVMMSG)

check_function_exists(malloc_usable_size KIWI_HAVE_MALLOC_USABLE_SIZE)

set(CMAKE_REQUIRED_FLAGS "${KIWI_ORIGINAL_CMAKE_REQUIRED_FLAGS}")

check_cxx_source_compiles("
  #pragma GCC diagnostic error \"-Wattributes\"
  extern \"C\" void (*test_ifunc(void))() { return 0; }
  void func() __attribute__((ifunc(\"test_ifunc\")));
  int main() { return 0; }"
  KIWI_HAVE_IFUNC
)
check_cxx_source_runs("
  int main(int, char**) {
    char buf[64] = {0};
    unsigned long *ptr = (unsigned long *)(buf + 1);
    *ptr = 0xdeadbeef;
    return (*ptr & 0xff) == 0xef ? 0 : 1;
  }"
  KIWI_HAVE_UNALIGNED_ACCESS
)
check_cxx_source_compiles("
  int main(int argc, char** argv) {
    unsigned size = argc;
    char data[size];
    return 0;
  }"
  KIWI_HAVE_VLA
)
check_cxx_source_runs("
  extern \"C\" int folly_example_undefined_weak_symbol() __attribute__((weak));
  int main(int argc, char** argv) {
    auto f = folly_example_undefined_weak_symbol; // null pointer
    return f ? f() : 0; // must compile, link, and run with null pointer
  }"
  KIWI_HAVE_WEAK_SYMBOLS
)
check_cxx_source_runs("
  #include <dlfcn.h>
  int main() {
    void *h = dlopen(\"linux-vdso.so.1\", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (h == nullptr) {
      return -1;
    }
    dlclose(h);
    return 0;
  }"
  KIWI_HAVE_LINUX_VDSO
)

check_cxx_source_runs("
  #include <cstddef>
  #include <cwchar>
  int main(int argc, char** argv) {
    return wcstol(L\"01\", nullptr, 10) == 1 ? 0 : 1;
  }"
  KIWI_HAVE_WCHAR_SUPPORT
)

check_cxx_source_compiles("
  #include <ext/random>
  int main(int argc, char** argv) {
    __gnu_cxx::sfmt19937 rng;
    return 0;
  }"
  KIWI_HAVE_EXTRANDOM_SFMT19937
)

check_cxx_source_runs("
  #include <stdarg.h>
  #include <stdio.h>

  int call_vsnprintf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int result = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return result;
  }

  int main(int argc, char** argv) {
    return call_vsnprintf(\"%\", 1) < 0 ? 0 : 1;
  }"
  HAVE_VSNPRINTF_ERRORS
)

if (KIWI_HAVE_LIBGFLAGS)
  # Older releases of gflags used the namespace "gflags"; newer releases
  # use "google" but also make symbols available in the deprecated "gflags"
  # namespace too.  The folly code internally uses "gflags" unless we tell it
  # otherwise.
  list(APPEND CMAKE_REQUIRED_LIBRARIES ${KIWI_LIBGFLAGS_LIBRARY})
  list(APPEND CMAKE_REQUIRED_INCLUDES ${KIWI_LIBGFLAGS_INCLUDE})
  check_cxx_source_compiles("
    #include <gflags/gflags.h>
    int main() {
      gflags::GetArgv();
      return 0;
    }
    "
    GFLAGS_NAMESPACE_IS_GFLAGS
  )
  list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES ${KIWI_LIBGFLAGS_LIBRARY})
  list(REMOVE_ITEM CMAKE_REQUIRED_INCLUDES ${KIWI_LIBGFLAGS_INCLUDE})
  if (GFLAGS_NAMESPACE_IS_GFLAGS)
    set(KIWI_UNUSUAL_GFLAGS_NAMESPACE OFF)
    set(KIWI_GFLAGS_NAMESPACE gflags)
  else()
    set(KIWI_UNUSUAL_GFLAGS_NAMESPACE ON)
    set(KIWI_GFLAGS_NAMESPACE google)
  endif()
endif()
