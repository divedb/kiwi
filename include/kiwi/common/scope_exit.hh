//===- llvm/ADT/ScopeExit.h - Execute code at scope exit --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines the make_scope_exit function, which executes user-defined
/// cleanup logic at scope exit.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>
#include <utility>

namespace kiwi {
namespace detail {

template <typename Callable>
class scope_exit {
 public:
  template <typename Fp>
  explicit scope_exit(Fp&& fp) : exit_function_(std::forward<Fp>(fp)) {}

  scope_exit(scope_exit&& rhs)
      : exit_function_(std::move(rhs.exit_function_)), engaged_(rhs.engaged_) {
    rhs.Dismiss();
  }

  scope_exit(const scope_exit&) = delete;
  scope_exit& operator=(scope_exit&&) = delete;
  scope_exit& operator=(const scope_exit&) = delete;

  void Dismiss() { engaged_ = false; }

  ~scope_exit() {
    if (engaged_) {
      exit_function_();
    }
  }

 private:
  Callable exit_function_;
  bool engaged_ = true;
};

}  // end namespace detail

/// Keeps the callable object that is passed in, and execute it at the
/// destruction of the returned object (usually at the scope exit where the
/// returned object is kept).
///
/// Interface is specified by p0052r2.
template <typename Callable>
[[nodiscard]] detail::scope_exit<std::decay_t<Callable>> make_scope_exit(
    Callable&& fp) {
  return detail::scope_exit<std::decay_t<Callable>>(std::forward<Callable>(fp));
}

}  // namespace kiwi
