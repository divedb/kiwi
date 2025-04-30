//===- llvm/ADT/STLFunctionalExtras.h - Extras for <functional> -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains some extension to <functional>.
//
// No library is required when using these functions.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>

namespace kiwi {

//===----------------------------------------------------------------------===//
//     Extra additions to <functional>
//===----------------------------------------------------------------------===//

/// An efficient, type-erasing, non-owning reference to a callable. This is
/// intended for use as the type of a function parameter that is not used
/// after the function in question returns.
///
/// This class does not own the callable, so it is not in general safe to store
/// a function_ref.
template <typename Fn>
class function_ref;

template <typename Ret, typename... Params>
class function_ref<Ret(Params...)> {
  template <typename Callable>
  static Ret callback_fn(intptr_t callable, Params... params) {
    return (*reinterpret_cast<Callable*>(callable))(
        std::forward<Params>(params)...);
  }

 public:
  function_ref() = default;
  function_ref(std::nullptr_t) {}

  template <typename Callable>
  function_ref(
      Callable&& callable KIWI_LIFETIME_BOUND,
      // This is not the copy-constructor.
      std::enable_if_t<!std::is_same<std::remove_cvref_t<Callable>,
                                     function_ref>::value>* = nullptr,
      // Functor must be callable and return a suitable type.
      std::enable_if_t<std::is_void<Ret>::value ||
                       std::is_convertible<decltype(std::declval<Callable>()(
                                               std::declval<Params>()...)),
                                           Ret>::value>* = nullptr)
      : callback_(callback_fn<std::remove_reference_t<Callable>>),
        callable_(reinterpret_cast<intptr_t>(&callable)) {}

  Ret operator()(Params... params) const {
    return callback_(callable_, std::forward<Params>(params)...);
  }

  explicit operator bool() const { return callback_; }

  bool operator==(const function_ref<Ret(Params...)>& other) const {
    return callable_ == other.callable_;
  }

 private:
  Ret (*callback_)(intptr_t callable, Params... params) = nullptr;
  intptr_t callable_;
};

}  // namespace kiwi