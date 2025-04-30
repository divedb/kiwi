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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "kiwi/portability/compiler_specific.hh"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace kiwi {
namespace detail {

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
bool generic_checked_add(T* result, T a, T b) {
  if constexpr (std::is_signed_v<T>) {
    if (a >= 0) {
      if (std::numeric_limits<T>::max() - a < b) [[unlikely]] {
        *result = {};

        return false;
      }
    } else if (b < std::numeric_limits<T>::min() - a) [[unlikely]] {
      *result = {};
      return false;
    }
    *result = a + b;

    return true;
  } else {
    if (a < std::numeric_limits<T>::max() - b) [[likely]] {
      *result = a + b;

      return true;
    } else {
      *result = {};

      return false;
    }
  }
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
bool generic_checked_small_mul(T* result, T a, T b) {
  static_assert(sizeof(T) < sizeof(uint64_t), "Too large");

  uint64_t res = static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
  constexpr uint64_t overflow_mask = ~((1ULL << (sizeof(T) * 8)) - 1);
  if ((res & overflow_mask) != 0) [[unlikely]] {
    *result = {};

    return false;
  }
  *result = static_cast<T>(res);

  return true;
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
std::enable_if_t<sizeof(T) < sizeof(uint64_t), bool> generic_checked_mul(
    T* result, T a, T b) {
  return generic_checked_small_mul(result, a, b);
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
std::enable_if_t<sizeof(T) == sizeof(uint64_t), bool> generic_checked_mul(
    T* result, T a, T b) {
  constexpr uint64_t half_bits = 32;
  constexpr uint64_t half_mask = (1ULL << half_bits) - 1ULL;
  uint64_t lhs_high = a >> half_bits;
  uint64_t lhs_low = a & half_mask;
  uint64_t rhs_high = b >> half_bits;
  uint64_t rhs_low = b & half_mask;

  if (lhs_high == 0 && rhs_high == 0) [[likely]] {
    *result = lhs_low * rhs_low;

    return true;
  }

  if (lhs_high != 0 && rhs_high != 0) [[unlikely]] {
    *result = {};

    return false;
  }

  uint64_t mid_bits1 = lhs_low * rhs_high;

  if (mid_bits1 >> half_bits != 0) [[unlikely]] {
    *result = {};

    return false;
  }

  uint64_t mid_bits2 = lhs_high * rhs_low;

  if (mid_bits2 >> half_bits != 0) [[unlikely]] {
    *result = {};

    return false;
  }

  uint64_t mid_bits = mid_bits1 + mid_bits2;

  if (mid_bits >> half_bits != 0) [[unlikely]] {
    *result = {};

    return false;
  }

  uint64_t bot_bits = lhs_low * rhs_low;

  if (!generic_checked_add(result, bot_bits, mid_bits << half_bits))
      [[unlikely]] {
    *result = {};

    return false;
  }

  return true;
}

}  // namespace detail

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
bool checked_add(T* result, T a, T b) {
#if HAS_BUILTIN(__builtin_add_overflow)
  if (!__builtin_add_overflow(a, b, result)) [[likely]] {
    return true;
  }

  *result = {};

  return false;
#else
  return detail::generic_checked_add(result, a, b);
#endif
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
bool checked_add(T* result, T a, T b, T c) {
  T tmp{};

  if (!checked_add(&tmp, a, b)) [[unlikely]] {
    *result = {};

    return false;
  }

  if (!checked_add(&tmp, tmp, c)) [[unlikely]] {
    *result = {};

    return false;
  }

  *result = tmp;

  return true;
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
bool checked_add(T* result, T a, T b, T c, T d) {
  T tmp{};

  if (!checked_add(&tmp, a, b)) [[unlikely]] {
    *result = {};
    return false;
  }

  if (!checked_add(&tmp, tmp, c)) [[unlikely]] {
    *result = {};

    return false;
  }

  if (!checked_add(&tmp, tmp, d)) [[unlikely]] {
    *result = {};

    return false;
  }

  *result = tmp;

  return true;
}

template <typename T>
bool checked_div(T* result, T dividend, T divisor) {
  if (divisor == 0) [[unlikely]] {
    *result = {};

    return false;
  }

  *result = dividend / divisor;

  return true;
}

template <typename T>
bool checked_mod(T* result, T dividend, T divisor) {
  if (divisor == 0) [[unlikely]] {
    *result = {};

    return false;
  }

  *result = dividend % divisor;

  return true;
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
bool checked_mul(T* result, T a, T b) {
  assert(result != nullptr);
#if HAS_BUILTIN(__builtin_mul_overflow)
  if (!__builtin_mul_overflow(a, b, result)) [[likely]] {
    return true;
  }

  *result = {};

  return false;
#elif _MSC_VER && ARCH_CPU_X86_64
  static_assert(sizeof(T) <= sizeof(unsigned __int64), "Too large");

  if (sizeof(T) < sizeof(uint64_t)) {
    return detail::generic_checked_mul(result, a, b);
  } else {
    unsigned __int64 high;
    unsigned __int64 low = _umul128(a, b, &high);

    if (high == 0) [[likely]] {
      *result = static_cast<T>(low);

      return true;
    }

    *result = {};

    return false;
  }
#else
  return detail::generic_checked_mul(result, a, b);
#endif
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
bool checked_muladd(T* result, T base, T mul, T add) {
  T tmp{};

  if (!checked_mul(&tmp, base, mul)) [[unlikely]] {
    *result = {};

    return false;
  }

  if (!checked_add(&tmp, tmp, add)) [[unlikely]] {
    *result = {};

    return false;
  }

  *result = tmp;

  return true;
}

template <typename T, typename T2,
          typename = std::enable_if_t<std::is_pointer<T>::value>,
          typename = std::enable_if_t<std::is_unsigned<T2>::value>>
bool checked_add(T* result, T a, T2 b) {
  size_t out = 0;
  bool ret = checked_muladd(&out, static_cast<size_t>(b),
                            sizeof(std::remove_pointer_t<T>),
                            reinterpret_cast<size_t>(a));

  *result = reinterpret_cast<T>(out);

  return ret;
}

}  // namespace kiwi
