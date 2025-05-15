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

#include "kiwi/portability/attributes.hh"
#include "kiwi/portability/features.hh"

// A wrapper around `__has_builtin`, similar to `KIWI_HAS_ATTRIBUTE()`.
//
// See also:
//   https://clang.llvm.org/docs/LanguageExtensions.html#has-builtin
#if defined(__has_builtin)
#define KIWI_HAS_BUILTIN(...) __has_builtin(__VA_ARGS__)
#else
#define KIWI_HAS_BUILTIN(...) 0
#endif

// KIWI_BUILTIN_UNPREDICTABLE
//
// mimic: __builtin_unpredictable, clang
#if KIWI_HAS_BUILTIN(__builtin_unpredictable)
#define KIWI_BUILTIN_UNPREDICTABLE(exp) __builtin_unpredictable(exp)
#else
#define KIWI_builtin_unpredictable(exp) \
  ::kiwi::builtin::detail::predict_<long long>(exp)
#endif

// KIWI_BUILTIN_EXPECT
//
// mimic: __builtin_expect, gcc/clang
#if KIWI_HAS_BUILTIN(__builtin_expect)
#define KIWI_BUILTIN_EXPECT(exp, c) __builtin_expect(static_cast<bool>(exp), c)
#else
#define KIWI_BUILTIN_EXPECT(exp, c) \
  ::kiwi::builtin::detail::predict_<long>(exp, c)
#endif

// KIWI_BUILTIN_EXPECT_WITH_PROBABILITY
//
// mimic: __builtin_expect_with_probability, gcc/clang
#if KIWI_HAS_BUILTIN(__builtin_expect_with_probability)
#define KIWI_BUILTIN_EXPECT_WITH_PROBABILITY(exp, c, p) \
  __builtin_expect_with_probability(exp, c, p)
#else
#define KIWI_BUILTIN_EXPECT_WITH_PROBABILITY(exp, c, p) \
  ::kiwi::builtin::detail::predict_<long>(exp, c, p)
#endif

#define KIWI_LIKELY(...) KIWI_BUILTIN_EXPECT((__VA_ARGS__), 1)

namespace kiwi {
namespace builtin {
namespace detail {

template <typename V>
struct predict_constinit_ {
  KIWI_ERASE KIWI_CONSTEVAL /* implicit */ predict_constinit_(
      V /* anonymous */) noexcept {}
};

template <typename E>
KIWI_ERASE constexpr E predict_(
    E exp, predict_constinit_<long> /* anonymous */ = 0,
    predict_constinit_<double> /* anonymous */ = 0.) {
  return exp;
}

}  // namespace detail
}  // namespace builtin
}  // namespace kiwi
