//===- llvm/ADT/DenseMapInfo.h - Type traits for DenseMap -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines DenseMapInfo traits for DenseMap.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <utility>

namespace kiwi {

namespace densemap::detail {

// A bit mixer with very low latency using one multiplications and one
// xor-shift. The constant is from splitmix64.
inline uint64_t mix(uint64_t x) {
  x *= 0xbf58476d1ce4e5b9u;
  x ^= x >> 31;

  return x;
}

}  // namespace densemap::detail

namespace detail {

/// Simplistic combination of 32-bit hash values into 32-bit hash values.
inline unsigned combine_hash_value(unsigned a, unsigned b) {
  uint64_t x = (uint64_t)a << 32 | (uint64_t)b;

  return (unsigned)densemap::detail::mix(x);
}

}  // namespace detail

/// An information struct used to provide DenseMap with the various necessary
/// components for a given value type `T`. `Enable` is an optional additional
/// parameter that is used to support SFINAE (generally using std::enable_if_t)
/// in derived DenseMapInfo specializations; in non-SFINAE use cases this should
/// just be `void`.
template <typename T, typename Enable = void>
struct DenseMapInfo {
  // static inline T get_empty_key();
  // static inline T get_tombstone_key();
  // static unsigned get_hash_value(const T &val);
  // static bool is_equal(const T &lhs, const T &rhs);
};

/// Provide DenseMapInfo for all pointers. Come up with sentinel pointer values
/// that are aligned to alignof(T) bytes, but try to avoid requiring T to be
/// complete. This allows clients to instantiate DenseMap<T*, ...> with forward
/// declared key types. Assume that no pointer key type requires more than 4096
/// bytes of alignment.
template <typename T>
struct DenseMapInfo<T*> {
  // The following should hold, but it would require T to be complete:
  // static_assert(alignof(T) <= (1 << kLog2MaxAlign),
  //               "DenseMap does not support pointer keys requiring more than "
  //               "kLog2MaxAlign bits of alignment");
  static constexpr uintptr_t kLog2MaxAlign = 12;

  static inline T* get_empty_key() {
    uintptr_t val = static_cast<uintptr_t>(-1);
    val <<= kLog2MaxAlign;

    return reinterpret_cast<T*>(val);
  }

  static inline T* get_tombstone_key() {
    uintptr_t val = static_cast<uintptr_t>(-2);
    val <<= kLog2MaxAlign;

    return reinterpret_cast<T*>(val);
  }

  static unsigned get_hash_value(const T* ptr_val) {
    return (unsigned((uintptr_t)ptr_val) >> 4) ^
           (unsigned((uintptr_t)ptr_val) >> 9);
  }

  static bool is_equal(const T* lhs, const T* rhs) { return lhs == rhs; }
};

/// Provide DenseMapInfo for chars.
template <>
struct DenseMapInfo<char> {
  static inline char get_empty_key() { return ~0; }
  static inline char get_tombstone_key() { return ~0 - 1; }
  static unsigned get_hash_value(const char& val) { return val * 37U; }

  static bool is_equal(const char& lhs, const char& rhs) { return lhs == rhs; }
};

/// Provide DenseMapInfo for unsigned chars.
template <>
struct DenseMapInfo<unsigned char> {
  static inline unsigned char get_empty_key() { return ~0; }
  static inline unsigned char get_tombstone_key() { return ~0 - 1; }
  static unsigned get_hash_value(const unsigned char& val) { return val * 37U; }

  static bool is_equal(const unsigned char& lhs, const unsigned char& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for unsigned shorts.
template <>
struct DenseMapInfo<unsigned short> {
  static inline unsigned short get_empty_key() { return 0xFFFF; }
  static inline unsigned short get_tombstone_key() { return 0xFFFF - 1; }
  static unsigned get_hash_value(const unsigned short& val) {
    return val * 37U;
  }

  static bool is_equal(const unsigned short& lhs, const unsigned short& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for unsigned ints.
template <>
struct DenseMapInfo<unsigned> {
  static inline unsigned get_empty_key() { return ~0U; }
  static inline unsigned get_tombstone_key() { return ~0U - 1; }
  static unsigned get_hash_value(const unsigned& val) { return val * 37U; }

  static bool is_equal(const unsigned& lhs, const unsigned& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for unsigned longs.
template <>
struct DenseMapInfo<unsigned long> {
  static inline unsigned long get_empty_key() { return ~0UL; }
  static inline unsigned long get_tombstone_key() { return ~0UL - 1L; }

  static unsigned get_hash_value(const unsigned long& val) {
    if constexpr (sizeof(val) == 4)
      return DenseMapInfo<unsigned>::get_hash_value(val);
    else
      return densemap::detail::mix(val);
  }

  static bool is_equal(const unsigned long& lhs, const unsigned long& rhs) {
    return lhs == rhs;
  }
};

// Provide DenseMapInfo for unsigned long longs.
template <>
struct DenseMapInfo<unsigned long long> {
  static inline unsigned long long get_empty_key() { return ~0ULL; }
  static inline unsigned long long get_tombstone_key() { return ~0ULL - 1ULL; }

  static unsigned get_hash_value(const unsigned long long& val) {
    return densemap::detail::mix(val);
  }

  static bool is_equal(const unsigned long long& lhs,
                       const unsigned long long& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for shorts.
template <>
struct DenseMapInfo<short> {
  static inline short get_empty_key() { return 0x7FFF; }
  static inline short get_tombstone_key() { return -0x7FFF - 1; }
  static unsigned get_hash_value(const short& val) { return val * 37U; }
  static bool is_equal(const short& lhs, const short& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for ints.
template <>
struct DenseMapInfo<int> {
  static inline int get_empty_key() { return 0x7fffffff; }
  static inline int get_tombstone_key() { return -0x7fffffff - 1; }
  static unsigned get_hash_value(const int& val) {
    return (unsigned)(val * 37U);
  }

  static bool is_equal(const int& lhs, const int& rhs) { return lhs == rhs; }
};

/// Provide DenseMapInfo for longs.
template <>
struct DenseMapInfo<long> {
  static inline long get_empty_key() {
    return (1UL << (sizeof(long) * 8 - 1)) - 1UL;
  }

  static inline long get_tombstone_key() { return get_empty_key() - 1L; }

  static unsigned get_hash_value(const long& val) {
    return (unsigned)(val * 37UL);
  }

  static bool is_equal(const long& lhs, const long& rhs) { return lhs == rhs; }
};

/// Provide DenseMapInfo for long longs.
template <>
struct DenseMapInfo<long long> {
  static inline long long get_empty_key() { return 0x7fffffffffffffffLL; }
  static inline long long get_tombstone_key() {
    return -0x7fffffffffffffffLL - 1;
  }

  static unsigned get_hash_value(const long long& val) {
    return (unsigned)(val * 37ULL);
  }

  static bool is_equal(const long long& lhs, const long long& rhs) {
    return lhs == rhs;
  }
};

/// Provide DenseMapInfo for all pairs whose members have info.
template <typename T, typename U>
struct DenseMapInfo<std::pair<T, U>> {
  using Pair = std::pair<T, U>;
  using FirstInfo = DenseMapInfo<T>;
  using SecondInfo = DenseMapInfo<U>;

  static inline Pair get_empty_key() {
    return std::make_pair(FirstInfo::get_empty_key(),
                          SecondInfo::get_empty_key());
  }

  static inline Pair get_tombstone_key() {
    return std::make_pair(FirstInfo::get_tombstone_key(),
                          SecondInfo::get_tombstone_key());
  }

  static unsigned get_hash_value(const Pair& pair_val) {
    return detail::combine_hash_value(
        FirstInfo::get_hash_value(pair_val.first),
        SecondInfo::get_hash_value(pair_val.second));
  }

  /// Expose an additional function intended to be used by other
  /// specializations of DenseMapInfo without needing to know how
  /// to combine hash values manually
  static unsigned get_hash_value_piecewise(const T& First, const U& Second) {
    return detail::combine_hash_value(FirstInfo::get_hash_value(First),
                                      SecondInfo::get_hash_value(Second));
  }

  static bool is_equal(const Pair& lhs, const Pair& rhs) {
    return FirstInfo::is_equal(lhs.first, rhs.first) &&
           SecondInfo::is_equal(lhs.second, rhs.second);
  }
};

/// Provide DenseMapInfo for all tuples whose members have info.
template <typename... Ts>
struct DenseMapInfo<std::tuple<Ts...>> {
  using Tuple = std::tuple<Ts...>;

  static inline Tuple get_empty_key() {
    return Tuple(DenseMapInfo<Ts>::get_empty_key()...);
  }

  static inline Tuple get_tombstone_key() {
    return Tuple(DenseMapInfo<Ts>::get_tombstone_key()...);
  }

  template <unsigned I>
  static unsigned get_hash_value_impl(const Tuple& values, std::false_type) {
    using EltType = std::tuple_element_t<I, Tuple>;
    std::integral_constant<bool, I + 1 == sizeof...(Ts)> at_end;

    return detail::combine_hash_value(
        DenseMapInfo<EltType>::get_hash_value(std::get<I>(values)),
        get_hash_value_impl<I + 1>(values, at_end));
  }

  template <unsigned I>
  static unsigned get_hash_value_impl(const Tuple&, std::true_type) {
    return 0;
  }

  static unsigned get_hash_value(const std::tuple<Ts...>& values) {
    std::integral_constant<bool, 0 == sizeof...(Ts)> at_end;

    return get_hash_value_impl<0>(values, at_end);
  }

  template <unsigned I>
  static bool is_equal_impl(const Tuple& lhs, const Tuple& rhs,
                            std::false_type) {
    using EltType = std::tuple_element_t<I, Tuple>;
    std::integral_constant<bool, I + 1 == sizeof...(Ts)> at_end;

    return DenseMapInfo<EltType>::is_equal(std::get<I>(lhs),
                                           std::get<I>(rhs)) &&
           is_equal_impl<I + 1>(lhs, rhs, at_end);
  }

  template <unsigned I>
  static bool is_equal_impl(const Tuple&, const Tuple&, std::true_type) {
    return true;
  }

  static bool is_equal(const Tuple& lhs, const Tuple& rhs) {
    std::integral_constant<bool, 0 == sizeof...(Ts)> at_end;

    return is_equal_impl<0>(lhs, rhs, at_end);
  }
};

/// Provide DenseMapInfo for enum classes.
template <typename Enum>
struct DenseMapInfo<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
  using UnderlyingType = std::underlying_type_t<Enum>;
  using Info = DenseMapInfo<UnderlyingType>;

  static Enum get_empty_key() {
    return static_cast<Enum>(Info::get_empty_key());
  }

  static Enum get_tombstone_key() {
    return static_cast<Enum>(Info::get_tombstone_key());
  }

  static unsigned get_hash_value(const Enum& val) {
    return Info::get_hash_value(static_cast<UnderlyingType>(val));
  }

  static bool is_equal(const Enum& lhs, const Enum& rhs) { return lhs == rhs; }
};

}  // namespace kiwi