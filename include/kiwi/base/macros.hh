//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: macros.h
// -----------------------------------------------------------------------------
//
// This header file defines the set of language macros used within Abseil code.
// For the set of macros used to determine supported compilers and platforms,
// see absl/base/config.h instead.
//
// This code is compiled directly on many platforms, including client
// platforms like Windows, Mac, and embedded systems.  Before making
// any changes here, make sure that you're not breaking any platforms.

#pragma once

#include <cstddef>

/// Returns the number of elements in an array as a compile-time constant, which
/// can be used in defining new arrays. If you use this macro on a pointer by
/// mistake, you will get a compile-time error.
#define KIWI_ARRAYSIZE(array) \
  (sizeof(::kiwi::macros_internal::ArraySizeHelper(array)))

namespace kiwi {

namespace macros_internal {
/// Note: this internal template function declaration is used by ABSL_ARRAYSIZE.
/// The function doesn't need a definition, as we only use its type.
template <typename T, size_t N>
auto ArraySizeHelper(const T (&array)[N]) -> char (&)[N];

}  // namespace macros_internal

}  // namespace kiwi