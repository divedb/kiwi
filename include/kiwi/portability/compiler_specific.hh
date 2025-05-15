// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "kiwi/portability/build_config.hh"
#include "kiwi/portability/builtin.hh"

#if defined(COMPILER_MSVC) && !defined(__clang__)
#error "Only clang-cl is supported on Windows, see https://crbug.com/988071"
#endif

// A wrapper around `__has_feature`, similar to `KIWI_HAS_ATTRIBUTE()`.
//
// See also:
//   https://clang.llvm.org/docs/LanguageExtensions.html#has-feature-and-has-extension
#if defined(__has_feature)
#define KIWI_HAS_FEATURE(FEATURE) __has_feature(FEATURE)
#else
#define KIWI_HAS_FEATURE(FEATURE) 0
#endif

// Attribute hidden.
#if defined(_MSC_VER)
#define KIWI_ATTR_VISIBILITY_HIDDEN
#elif defined(__GNUC__)
#define KIWI_ATTR_VISIBILITY_HIDDEN __attribute__((__visibility__("hidden")))
#else
#define KIWI_ATTR_VISIBILITY_HIDDEN
#endif

// Annotates a pointer and size directing MSAN to treat that memory region as
// fully initialized. Useful for e.g. code that deliberately reads uninitialized
// data, such as a GC scavenging root set pointers from the stack.
//
// See also:
//   https://github.com/google/sanitizers/wiki/MemorySanitizer
//
// Usage:
// ```
//   T* ptr = ...;
//   // After the next statement, MSAN will assume `ptr` points to an
//   // initialized `T`.
//   MSAN_UNPOISON(ptr, sizeof(T));
// ```
#if defined(MEMORY_SANITIZER) && !BUILDFLAG(IS_NACL)
#include <sanitizer/msan_interface.h>
#define MSAN_UNPOISON(p, size) __msan_unpoison(p, size)
#else
#define MSAN_UNPOISON(p, size)
#endif

// Annotates a pointer and size directing MSAN to check whether that memory
// region is initialized, as if it was being read from. If any bits are
// uninitialized, crashes with an MSAN report. Useful for e.g. sanitizing data
// MSAN won't be able to track, such as data that is about to be passed to
// another process via shared memory.
//
// See also:
//   https://www.chromium.org/developers/testing/memorysanitizer/#debugging-msan-reports
//
// Usage:
// ```
//   T* ptr = ...;
//   // The following line will crash at runtime in MSAN builds if `ptr` does
//   // not point to an initialized `T`.
//   MSAN_CHECK_MEM_IS_INITIALIZED(ptr, sizeof(T));
// ```
#if defined(MEMORY_SANITIZER) && !BUILDFLAG(IS_NACL)
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size) \
  __msan_check_mem_is_initialized(p, size)
#else
#define MSAN_CHECK_MEM_IS_INITIALIZED(p, size)
#endif

// Annotates a function disabling Control Flow Integrity checks due to perf
// impact.
//
// See also:
//   https://clang.llvm.org/docs/ControlFlowIntegrity.html#performance
//   https://www.chromium.org/developers/testing/control-flow-integrity/#overhead-only-tested-on-x64
//
// Usage:
// ```
//   DISABLE_CFI_PERF void Func() {
//     // CFI checks will not be performed in this body, due to perf reasons.
//   }
// ```
#if !defined(DISABLE_CFI_PERF)
#if defined(__clang__) && defined(OFFICIAL_BUILD)
#define DISABLE_CFI_PERF NO_SANITIZE("cfi")
#else
#define DISABLE_CFI_PERF
#endif
#endif

// Annotates a function disabling Control Flow Integrity indirect call checks.
// NOTE: Prefer `DISABLE_CFI_DLSYM()` if you just need to allow calling of dlsym
// functions.
//
// See also:
//   https://clang.llvm.org/docs/ControlFlowIntegrity.html#available-schemes
//   https://www.chromium.org/developers/testing/control-flow-integrity/#indirect-call-failures
//
// Usage:
// ```
//   DISABLE_CFI_ICALL void Func() {
//     // CFI indirect call checks will not be performed in this body.
//   }
// ```
#if !defined(DISABLE_CFI_ICALL)
#if BUILDFLAG(IS_WIN)
#define DISABLE_CFI_ICALL NO_SANITIZE("cfi-icall") __declspec(guard(nocf))
#else
#define DISABLE_CFI_ICALL NO_SANITIZE("cfi-icall")
#endif
#endif

// Annotates a function disabling Control Flow Integrity indirect call checks if
// doing so is necessary to call dlsym functions. The checks are retained on
// platforms where loaded modules participate in CFI (viz. Windows).
//
// See also:
//   https://www.chromium.org/developers/testing/control-flow-integrity/#indirect-call-failures
//
// Usage:
// ```
//   DISABLE_CFI_DLSYM void Func() {
//     // On non-Windows platforms, CFI indirect call checks will not be
//     // performed in this body.
//   }
// ```
#if !defined(DISABLE_CFI_DLSYM)
#if BUILDFLAG(IS_WIN)
#define DISABLE_CFI_DLSYM
#else
#define DISABLE_CFI_DLSYM DISABLE_CFI_ICALL
#endif
#endif

// Evaluates to a string constant containing the function signature.
//
// See also:
//   https://clang.llvm.org/docs/LanguageExtensions.html#source-location-builtins
//   https://en.cppreference.com/w/c/language/function_definition#func
//
// Usage:
// ```
//   void Func(int arg) {
//     std::cout << PRETTY_FUNCTION;  // Prints `void Func(int)` or similar.
//   }
// ```
#if defined(COMPILER_GCC)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(COMPILER_MSVC)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __func__
#endif

// Annotates a codepath suppressing static analysis along that path. Useful when
// code is safe in practice for reasons the analyzer can't detect, e.g. because
// the condition leading to that path guarantees a param is non-null.
//
// Usage:
// ```
//   if (cond) {
//     ANALYZER_SKIP_THIS_PATH();
//     // Static analysis will be disabled for the remainder of this block.
//     delete ptr;
//   }
// ```
#if defined(__clang_analyzer__)
inline constexpr bool AnalyzerNoReturn()
#if KIWI_HAS_ATTRIBUTE(analyzer_noreturn)
    __attribute__((analyzer_noreturn))
#endif
{
  return false;
}
#define ANALYZER_SKIP_THIS_PATH() static_cast<void>(::AnalyzerNoReturn())
#else
// The above definition would be safe even outside the analyzer, but defining
// the macro away entirely avoids the need for the optimizer to eliminate it.
#define ANALYZER_SKIP_THIS_PATH()
#endif

// Annotates a condition directing static analysis to assume it is always true.
// Evaluates to the provided `arg` as a `bool`.
//
// Usage:
// ```
//   // Static analysis will assume the following condition always holds.
//   if (ANALYZER_ASSUME_TRUE(cond)) ...
// ```
#if defined(__clang_analyzer__)
inline constexpr bool AnalyzerAssumeTrue(bool arg) {
  return arg || AnalyzerNoReturn();
}
#define ANALYZER_ASSUME_TRUE(arg) ::AnalyzerAssumeTrue(!!(arg))
#else
// Again, the above definition is safe, this is just simpler for the optimizer.
#define ANALYZER_ASSUME_TRUE(arg) (arg)
#endif

// Determines whether a type is trivially relocatable, i.e. a move-and-destroy
// sequence can safely be replaced with `memcpy()`. This is true of types with
// trivial copy or move construction plus trivial destruction, as well as types
// marked `TRIVIAL_ABI`. Useful to optimize container implementations.
//
// See also:
//   https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p1144r8.html
//   https://clang.llvm.org/docs/LanguageExtensions.html#:~:text=__is_trivially_relocatable
//
// Usage:
// ```
//   if constexpr (IS_TRIVIALLY_RELOCATABLE(T)) {
//     // This block will only be executed if type `T` is trivially relocatable.
//   }
// ```
#if KIWI_HAS_BUILTIN(__is_trivially_relocatable)
#define IS_TRIVIALLY_RELOCATABLE(t) __is_trivially_relocatable(t)
#else
#define IS_TRIVIALLY_RELOCATABLE(t) false
#endif

// Annotates code indicating that it should be permanently exempted from
// `-Wunsafe-buffer-usage`. For temporary cases such as migrating callers to
// safer patterns, use `UNSAFE_TODO()` instead; see documentation there.
//
// All calls to functions annotated with `UNSAFE_BUFFER_USAGE` must be marked
// with one of these two macros; they can also be used around pointer
// arithmetic, pointer subscripting, and the like.
//
// ** USE OF THIS MACRO SHOULD BE VERY RARE.** Using this macro indicates that
// the compiler cannot verify that the code avoids OOB, and manual review is
// required. Even with manual review, it's easy for assumptions to change and
// security bugs to creep in over time. Prefer safer patterns instead.
//
// Usage should wrap the minimum necessary code, and *must* include a
// `// SAFETY: ...` comment that explains how the code guarantees safety or
// meets the requirements of called `UNSAFE_BUFFER_USAGE` functions. Guarantees
// must be manually verifiable by the Chrome security team using only local
// invariants; contact security@chromium.org to schedule such a review. Valid
// invariants include:
// - Runtime conditions or `CHECK()`s nearby
// - Invariants guaranteed by types in the surrounding code
// - Invariants guaranteed by function calls in the surrounding code
// - Caller requirements, if the containing function is itself annotated with
//   `UNSAFE_BUFFER_USAGE`; this is less safe and should be a last resort
//
// See also:
//   https://chromium.googlesource.com/chromium/src/+/main/docs/unsafe_buffers.md
//   https://clang.llvm.org/docs/SafeBuffers.html
//   https://clang.llvm.org/docs/DiagnosticsReference.html#wunsafe-buffer-usage
//
// Usage:
// ```
//   // The following call will not trigger a compiler warning even if `Func()`
//   // is annotated `UNSAFE_BUFFER_USAGE`.
//   return UNSAFE_BUFFERS(Func(input, end));
// ```
//
// Test for `__clang__` directly, as there's no `__has_pragma` or similar (see
// https://github.com/llvm/llvm-project/issues/51887).
#if defined(__clang__)
// Disabling `clang-format` allows each `_Pragma` to be on its own line, as
// recommended by https://gcc.gnu.org/onlinedocs/cpp/Pragmas.html.
// clang-format off
#define UNSAFE_BUFFERS(...)                    \
    _Pragma("clang unsafe_buffer_usage begin") \
    __VA_ARGS__                                \
    _Pragma("clang unsafe_buffer_usage end")
// clang-format on
#else
#define UNSAFE_BUFFERS(...) __VA_ARGS__
#endif

// Annotates code indicating that it should be temporarily exempted from
// `-Wunsafe-buffer-usage`. While this is functionally the same as
// `UNSAFE_BUFFERS()`, semantically it indicates that this is for migration
// purposes, and should be cleaned up as soon as possible.
//
// Usage:
// ```
//   // The following call will not trigger a compiler warning even if `Func()`
//   // is annotated `UNSAFE_BUFFER_USAGE`.
//   return UNSAFE_TODO(Func(input, end));
// ```
#define UNSAFE_TODO(...) UNSAFE_BUFFERS(__VA_ARGS__)

#if defined(_MSC_VER)
#define UNREACHABLE() __assume(0)
#elif defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE() \
  do {                \
  } while (0)
#endif

// Generalize warning push/pop.
#if defined(__GNUC__) || defined(__clang__)
// Clang & GCC
#define KIWI_PUSH_WARNING _Pragma("GCC diagnostic push")
#define KIWI_POP_WARNING _Pragma("GCC diagnostic pop")
#define KIWI_GNU_DISABLE_WARNING_INTERNAL2(warning_name) #warning_name
#define KIWI_GNU_DISABLE_WARNING(warning_name) \
  _Pragma(                                     \
      KIWI_GNU_DISABLE_WARNING_INTERNAL2(GCC diagnostic ignored warning_name))
#ifdef __clang__
#define KIWI_CLANG_DISABLE_WARNING(warning_name) \
  KIWI_GNU_DISABLE_WARNING(warning_name)
#define KIWI_GCC_DISABLE_WARNING(warning_name)
#else
#define KIWI_CLANG_DISABLE_WARNING(warning_name)
#define KIWI_GCC_DISABLE_WARNING(warning_name) \
  KIWI_GNU_DISABLE_WARNING(warning_name)
#endif
#define KIWI_MSVC_DISABLE_WARNING(warning_number)
#elif defined(_MSC_VER)
#define KIWI_PUSH_WARNING __pragma(warning(push))
#define KIWI_POP_WARNING __pragma(warning(pop))
// Disable the GCC warnings.
#define KIWI_GNU_DISABLE_WARNING(warning_name)
#define KIWI_GCC_DISABLE_WARNING(warning_name)
#define KIWI_CLANG_DISABLE_WARNING(warning_name)
#define KIWI_MSVC_DISABLE_WARNING(warning_number) \
  __pragma(warning(disable : warning_number))
#else
#define KIWI_PUSH_WARNING
#define KIWI_POP_WARNING
#define KIWI_GNU_DISABLE_WARNING(warning_name)
#define KIWI_GCC_DISABLE_WARNING(warning_name)
#define KIWI_CLANG_DISABLE_WARNING(warning_name)
#define KIWI_MSVC_DISABLE_WARNING(warning_number)
#endif

// Define KIWI_HAS_EXCEPTIONS
#if (defined(__cpp_exceptions) && __cpp_exceptions >= 199711) || \
    KIWI_HAS_FEATURE(cxx_exceptions)
#define KIWI_HAS_EXCEPTIONS 1
#elif __GNUC__
#if defined(__EXCEPTIONS) && __EXCEPTIONS
#define KIWI_HAS_EXCEPTIONS 1
#else  // __EXCEPTIONS
#define KIWI_HAS_EXCEPTIONS 0
#endif  // __EXCEPTIONS
#elif KIWI_MICROSOFT_ABI_VER
#if _CPPUNWIND
#define KIWI_HAS_EXCEPTIONS 1
#else  // _CPPUNWIND
#define KIWI_HAS_EXCEPTIONS 0
#endif  // _CPPUNWIND
#else
#define KIWI_HAS_EXCEPTIONS 1  // default assumption for unknown platforms
#endif
