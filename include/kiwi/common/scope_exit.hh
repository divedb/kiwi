//===- llvm/ADT/ScopeExit.h - Execute code at scope exit --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines the make_ScopeExit function, which executes user-defined
/// cleanup logic at scope exit.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>
#include <utility>

#include "kiwi/portability/preprocessor.hh"

namespace kiwi {
namespace detail {

template <typename Callable>
class ScopeExit {
 public:
  template <typename Fp>
  explicit ScopeExit(Fp&& fp) : exit_function_(std::forward<Fp>(fp)) {}

  ScopeExit(ScopeExit&& rhs)
      : exit_function_(std::move(rhs.exit_function_)), engaged_(rhs.engaged_) {
    rhs.Dismiss();
  }

  ScopeExit(const ScopeExit&) = delete;
  ScopeExit& operator=(ScopeExit&&) = delete;
  ScopeExit& operator=(const ScopeExit&) = delete;

  void Dismiss() { engaged_ = false; }

  ~ScopeExit() {
    if (engaged_) {
      exit_function_();
    }
  }

 private:
  Callable exit_function_;
  bool engaged_ = true;
};

// Internal use for the macro SCOPE_EXIT below.
enum class ScopeExitTag {};

}  // end namespace detail

/// Keeps the callable object that is passed in, and execute it at the
/// destruction of the returned object (usually at the scope exit where the
/// returned object is kept).
///
/// Interface is specified by p0052r2.
template <typename Callable>
[[nodiscard]] detail::ScopeExit<std::decay_t<Callable>> make_scope_exit(
    Callable&& fp) {
  return detail::ScopeExit<std::decay_t<Callable>>(std::forward<Callable>(fp));
}

template <typename FunctionType>
detail::ScopeExit<typename std::decay<FunctionType>::type> operator+(
    detail::ScopeExitTag, FunctionType&& fn) {
  return detail::ScopeExit<typename std::decay<FunctionType>::type>(
      std::forward<FunctionType>(fn));
}

}  // namespace kiwi

#define SCOPE_EXIT                                 \
  auto KIWI_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = \
      ::kiwi::detail::ScopeExitTag() + [&]() noexcept
