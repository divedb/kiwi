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

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace kiwi {
namespace detail {

enum class FutexResult {
  kValueChanged,  ///< Futex value didn't match expected
  kWoken,         ///< Wakeup by matching futex wake, or spurious wakeup
  kInterrupted,   ///< Wakup by interrupting signal
  kTimeout,       ///< Wakeup by expiring deadline
};

/// Futex is an atomic 32 bit unsigned integer that provides access to the
/// futex() syscall on that value.  It is templated in such a way that it
/// can interact properly with DeterministicSchedule testing.
///
/// If you don't know how to use futex(), you probably shouldn't be using
/// this class.  Even if you do know how, you should have a good reason
/// (and benchmarks to back you up).
///
/// Because of the semantics of the futex syscall, the futex family of
/// functions are available as free functions rather than member functions
template <template <typename> class Atom = std::atomic>
using Futex = Atom<std::uint32_t>;

}

}  // namespace kiwi
