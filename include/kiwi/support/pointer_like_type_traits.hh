//===- llvm/Support/PointerLikeTypeTraits.h - Pointer Traits ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the PointerLikeTypeTraits class.  This allows data
// structures to reason about pointers and other things that are pointer sized.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <cstdint>
#include <type_traits>

namespace kiwi {

/// A traits type that is used to handle pointer types and things that are just
/// wrappers for pointers as a uniform entity.
template <typename T>
struct PointerLikeTypeTraits;

namespace detail {
/// A tiny meta function to compute the log2 of a compile time constant.
template <size_t N>
struct ConstantLog2
    : std::integral_constant<size_t, ConstantLog2<N / 2>::value + 1> {};
template <>
struct ConstantLog2<1> : std::integral_constant<size_t, 0> {};

// Provide a trait to check if T is pointer-like.
template <typename T, typename U = void>
struct HasPointerLikeTypeTraits {
  static const bool value = false;
};

/// sizeof(T) is valid only for a complete T.
template <typename T>
struct HasPointerLikeTypeTraits<
    T, decltype((sizeof(PointerLikeTypeTraits<T>) + sizeof(T)), void())> {
  static const bool value = true;
};

template <typename T>
struct IsPointerLike {
  static const bool value = HasPointerLikeTypeTraits<T>::value;
};

template <typename T>
struct IsPointerLike<T*> {
  static const bool value = true;
};

}  // namespace detail

/// Provide PointerLikeTypeTraits for non-cvr pointers.
template <typename T>
struct PointerLikeTypeTraits<T*> {
  static inline void* get_as_void_pointer(T* p) { return p; }
  static inline T* get_from_void_pointer(void* p) { return static_cast<T*>(p); }

  static constexpr int kNumLowBitsAvailable =
      detail::ConstantLog2<alignof(T)>::value;
};

template <>
struct PointerLikeTypeTraits<void*> {
  static inline void* get_as_void_pointer(void* p) { return p; }
  static inline void* get_from_void_pointer(void* p) { return p; }

  /// Note, we assume here that void* is related to raw malloc'ed memory and
  /// that malloc returns objects at least 4-byte aligned. However, this may be
  /// wrong, or pointers may be from something other than malloc. In this case,
  /// you should specify a real typed pointer or avoid this template.
  ///
  /// All clients should use assertions to do a run-time check to ensure that
  /// this is actually true.
  static constexpr int kNumLowBitsAvailable = 2;
};

/// Provide PointerLikeTypeTraits for const things.
template <typename T>
struct PointerLikeTypeTraits<const T> {
  typedef PointerLikeTypeTraits<T> NonConst;

  static inline const void* get_as_void_pointer(const T p) {
    return NonConst::get_as_void_pointer(p);
  }

  static inline const T get_from_void_pointer(const void* p) {
    return NonConst::get_from_void_pointer(const_cast<void*>(p));
  }

  static constexpr int kNumLowBitsAvailable = NonConst::kNumLowBitsAvailable;
};

/// Provide PointerLikeTypeTraits for const pointers.
template <typename T>
struct PointerLikeTypeTraits<const T*> {
  typedef PointerLikeTypeTraits<T*> NonConst;

  static inline const void* get_as_void_pointer(const T* p) {
    return NonConst::get_as_void_pointer(const_cast<T*>(p));
  }

  static inline const T* get_from_void_pointer(const void* p) {
    return NonConst::get_from_void_pointer(const_cast<void*>(p));
  }

  static constexpr int kNumLowBitsAvailable = NonConst::kNumLowBitsAvailable;
};

/// Provide PointerLikeTypeTraits for uintptr_t.
template <>
struct PointerLikeTypeTraits<uintptr_t> {
  static inline void* get_as_void_pointer(uintptr_t p) {
    return reinterpret_cast<void*>(p);
  }

  static inline uintptr_t get_from_void_pointer(void* p) {
    return reinterpret_cast<uintptr_t>(p);
  }

  // No bits are available!
  static constexpr int kNumLowBitsAvailable = 0;
};

/// Provide suitable custom traits struct for function pointers.
///
/// Function pointers can't be directly given these traits as functions can't
/// have their alignment computed with `alignof` and we need different casting.
///
/// To rely on higher alignment for a specialized use, you can provide a
/// customized form of this template explicitly with higher alignment, and
/// potentially use alignment attributes on functions to satisfy that.
template <int Alignment, typename FunctionPointerT>
struct FunctionPointerLikeTypeTraits {
  static constexpr int kNumLowBitsAvailable =
      detail::ConstantLog2<Alignment>::value;

  static inline void* get_as_void_pointer(FunctionPointerT p) {
    assert((reinterpret_cast<uintptr_t>(p) &
            ~((uintptr_t)-1 << kNumLowBitsAvailable)) == 0 &&
           "Alignment not satisfied for an actual function pointer!");
    return reinterpret_cast<void*>(p);
  }

  static inline FunctionPointerT get_from_void_pointer(void* p) {
    return reinterpret_cast<FunctionPointerT>(p);
  }
};

/// Provide a default specialization for function pointers that assumes 4-byte
/// alignment.
///
/// We assume here that functions used with this are always at least 4-byte
/// aligned. This means that, for example, thumb functions won't work or systems
/// with weird unaligned function pointers won't work. But all practical systems
/// we support satisfy this requirement.
template <typename ReturnT, typename... ParamTs>
struct PointerLikeTypeTraits<ReturnT (*)(ParamTs...)>
    : FunctionPointerLikeTypeTraits<4, ReturnT (*)(ParamTs...)> {};

}  // namespace kiwi