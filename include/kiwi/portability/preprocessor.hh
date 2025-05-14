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

#include "kiwi/portability/compiler_specific.hh"

// MSVC's preprocessor is a pain, so we have to forcefully expand the VA args in
// some places.
// Reference:
// https://stackoverflow.com/questions/5134523/msvc-doesnt-expand-va-args-correctly
#define KIWI_VA_GLUE(a, b) a b

// KIWI_ONE_OR_NONE(hello, world) expands to hello.
// KIWI_ONE_OR_NONE(hello) expands to nothing.
// This macro is used to insert or eliminate text based on the presence of
// another argument.
#define KIWI_ONE_OR_NONE(a, ...) KIWI_VA_GLUE(KIWI_THIRD, (a, ##__VA_ARGS__, a))
#define KIWI_THIRD(a, b, ...) __VA_ARGS__

// Helper macro that extracts the first argument out of a list of any number of
// arguments.
#define KIWI_ARG_1(a, ...) a

// Helper macro that extracts the second argument out of a list of any number of
// arguments. If only one argument is given, it returns that.
#ifdef _MSC_VER
// GCC refuses to expand this correctly if this macro itself was
// called with KIWI_VA_GLUE :(
#define KIWI_ARG_2_OR_1(...) \
  KIWI_VA_GLUE(KIWI_ARG_2_OR_1_IMPL, (__VA_ARGS__, __VA_ARGS__))
#else
#define KIWI_ARG_2_OR_1(...) KIWI_ARG_2_OR_1_IMPL(__VA_ARGS__, __VA_ARGS__)
#endif

// Support macro for the above
#define KIWI_ARG_2_OR_1_IMPL(a, b, ...) b

// Helper macro that provides a way to pass argument with commas in it to
// some other macro whose syntax doesn't allow using extra parentheses.
// Example:
//
//   #define MACRO(type, name) type name
//   MACRO(KIWI_SINGLE_ARG(std::pair<size_t, size_t>), x);
#define KIWI_SINGLE_ARG(...) __VA_ARGS__

#define KIWI_PP_DETAIL_APPEND_VA_ARG(...) , ##__VA_ARGS__

// Helper macro that just ignores its parameters.
#define KIWI_IGNORE(...)

// Helper macro that just ignores its parameters and inserts a semicolon.
#define KIWI_SEMICOLON(...) ;

// KIWI_ANONYMOUS_VARIABLE(str) introduces an identifier starting with
// str and ending with a number that varies with the line.
#ifndef KIWI_ANONYMOUS_VARIABLE
#define KIWI_CONCATENATE_IMPL(s1, s2) s1##s2
#define KIWI_CONCATENATE(s1, s2) KIWI_CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
// Modular builds build each module with its own preprocessor state, meaning
// `__COUNTER__` no longer provides a unique number across a TU.  Instead of
// calling back to just `__LINE__`, use a mix of `__COUNTER__` and `__LINE__`
// to try provide as much uniqueness as possible.
#if KIWI_HAS_FEATURE(modules)
#define KIWI_ANONYMOUS_VARIABLE(str)                                        \
  KIWI_CONCATENATE(KIWI_CONCATENATE(KIWI_CONCATENATE(str, __COUNTER__), _), \
                   __LINE__)
#else
#define KIWI_ANONYMOUS_VARIABLE(str) KIWI_CONCATENATE(str, __COUNTER__)
#endif
#else
#define KIWI_ANONYMOUS_VARIABLE(str) KIWI_CONCATENATE(str, __LINE__)
#endif
#endif

// Use KIWI_PP_STRINGIZE(x) when you'd want to do what #x does inside
// another macro expansion.
#define KIWI_PP_STRINGIZE(x) #x

#define KIWI_PP_DETAIL_NARGS_1(dummy, _15, _14, _13, _12, _11, _10, _9, _8, \
                               _7, _6, _5, _4, _3, _2, _1, _0, ...)         \
  _0
#define KIWI_PP_DETAIL_NARGS(...)                                            \
  KIWI_PP_DETAIL_NARGS_1(dummy, ##__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, \
                         7, 6, 5, 4, 3, 2, 1, 0)

#define KIWI_PP_DETAIL_FOR_EACH_REC_0(fn, ...)
#define KIWI_PP_DETAIL_FOR_EACH_REC_1(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_0(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_2(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_1(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_3(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_2(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_4(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_3(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_5(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_4(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_6(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_5(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_7(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_6(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_8(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_7(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_9(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_8(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_10(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_9(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_11(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_10(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_12(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_11(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_13(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_12(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_14(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_13(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_REC_15(fn, a, ...) \
  fn(a) KIWI_PP_DETAIL_FOR_EACH_REC_14(fn, __VA_ARGS__)

#define KIWI_PP_DETAIL_FOR_EACH_2(fn, n, ...) \
  KIWI_PP_DETAIL_FOR_EACH_REC_##n(fn, __VA_ARGS__)
#define KIWI_PP_DETAIL_FOR_EACH_1(fn, n, ...) \
  KIWI_PP_DETAIL_FOR_EACH_2(fn, n, __VA_ARGS__)

// KIWI_PP_FOR_EACH
//
// Used to invoke a preprocessor macro, the name of which is passed as the
// first argument, once for each subsequent variadic argument.
//
// At present, supports [0, 16) arguments.
//
// This input:
//
//   #define DOIT(a) go_do_it(a);
//   KIWI_PP_FOR_EACH(DOIT, 3, 5, 7)
//   #undef DOIT
//
// Expands to this output (with whitespace adjusted for clarity):
//
//   go_do_it(3);
//   go_do_it(5);
//   go_do_it(7);
#define KIWI_PP_FOR_EACH(fn, ...) \
  KIWI_PP_DETAIL_FOR_EACH_1(fn, KIWI_PP_DETAIL_NARGS(__VA_ARGS__), __VA_ARGS__)

#if defined(U)
#error defined(U) // literal U is used below
#endif

// KIWI_PP_CONSTINIT_LINE_UNSIGNED
//
// MSVC with /ZI has a special backing variable for __LINE__ which is not a
// literal - but token-pasting __LINE__ suppresses this backing variable. This
// is done in MSVC to support its edit-and-continue feature.
//
// This macro evaluates to:
//   __LINE__ ## U
//
// So this macro may be ill-suited to cases which need exactly __LINE__.
//
// Documentation:
//   https://docs.microsoft.com/en-us/cpp/build/reference/z7-zi-zi-debug-information-format?view=msvc-170#zi-1
// Workaround:
//   https://stackoverflow.com/questions/57137351/line-is-not-constexpr-in-msvc
#define KIWI_PP_CONSTINIT_LINE_UNSIGNED KIWI_CONCATENATE(__LINE__, U)
