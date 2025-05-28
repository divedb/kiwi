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

/// KIWI_CREATE_MEMBER_INVOKER
///
/// Used to create an invoker type bound to a specific member-invocable name.
///
/// Example:
///
///   KIWI_CREATE_MEMBER_INVOKER(foo_invoker, foo);
///
/// The type `foo_invoker` is generated in the current namespace and may be
/// used as follows:
///
/// \code
///   struct CanFoo {
///     int foo(Bar&) { return 1; }
///     int foo(Car&&) noexcept { return 2; }
///   };
///
///   using traits = kiwi::invoke_traits<foo_invoker>;
///
///   traits::invoke(CanFoo{}, Car{}) // 2
///
///   traits::invoke_result<CanFoo, Bar&> // has member
///   traits::invoke_result_t<CanFoo, Bar&> // int
///   traits::invoke_result<CanFoo, Bar&&> // empty
///   traits::invoke_result_t<CanFoo, Bar&&> // error
///
///   traits::is_invocable_v<CanFoo, Bar&> // true
///   traits::is_invocable_v<CanFoo, Bar&&> // false
///
///   traits::is_invocable_r_v<int, CanFoo, Bar&> // true
///   traits::is_invocable_r_v<char*, CanFoo, Bar&> // false
///
///   traits::is_nothrow_invocable_v<CanFoo, Bar&> // false
///   traits::is_nothrow_invocable_v<CanFoo, Car&&> // true
///
///   traits::is_nothrow_invocable_v<int, CanFoo, Bar&> // false
///   traits::is_nothrow_invocable_v<char*, CanFoo, Bar&> // false
///   traits::is_nothrow_invocable_v<int, CanFoo, Car&&> // true
///   traits::is_nothrow_invocable_v<char*, CanFoo, Car&&> // false
/// \endcode
#define KIWI_CREATE_MEMBER_INVOKER(classname, membername)                  \
  struct classname {                                                       \
    template <typename O, typename... Args>                                \
    [[maybe_unused]] KIWI_ERASE_HACK_GCC constexpr auto operator()(        \
        O&& o, Args&&... args) const                                       \
        noexcept(noexcept(                                                 \
            static_cast<O&&>(o).membername(static_cast<Args&&>(args)...))) \
            -> decltype(static_cast<O&&>(o).membername(                    \
                static_cast<Args&&>(args)...)) {                           \
      return static_cast<O&&>(o).membername(static_cast<Args&&>(args)...); \
    }                                                                      \
  }

/// KIWI_CREATE_MEMBER_INVOKER_SUITE
///
/// Used to create an invoker type and associated variable bound to a specific
/// member-invocable name. The invoker variable is named like the member-
/// invocable name and the invoker type is named with a suffix of _fn.
///
/// See KIWI_CREATE_MEMBER_INVOKER.
#define KIWI_CREATE_MEMBER_INVOKER_SUITE(membername)       \
  KIWI_CREATE_MEMBER_INVOKER(membername##_fn, membername); \
  [[maybe_unused]] inline constexpr membername##_fn membername {}