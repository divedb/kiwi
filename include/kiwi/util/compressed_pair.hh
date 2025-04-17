// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>
#include <utility>

namespace kiwi {

template <typename Tp, bool>
struct DependentType : public Tp {};

/// Tag used to default initialize one or both of the pair's elements.
struct DefaultInitTag {};
struct ValueInitTag {};

template <typename Tp, int Idx,
          bool CanBeEmptyBase = std::is_empty_v<Tp> && !std::is_final_v<Tp>>
class CompressedPairElem {
 public:
  using ParamT = Tp;
  using reference = Tp&;
  using const_reference = const Tp&;

  constexpr CompressedPairElem(DefaultInitTag) {}
  constexpr CompressedPairElem(ValueInitTag) : value_() {}

  template <typename Up,
            std::enable_if_t<
                !std::is_same_v<CompressedPairElem, std::decay_t<Up>>, int> = 0>
  constexpr explicit CompressedPairElem(Up&& u) : value_(std::forward<Up>(u)) {}

  template <typename... Args, size_t... Indexes>
  constexpr CompressedPairElem(std::piecewise_construct_t,
                               std::tuple<Args...> args,
                               std::index_sequence<Indexes...>)
      : value_(std::forward<Args>(std::get<Indexes>(args))...) {}

  reference Get() noexcept { return value_; }
  const_reference Get() const noexcept { return value_; }

 private:
  Tp value_;
};

template <typename Tp, int Idx>
class CompressedPairElem<Tp, Idx, true> : private Tp {
 public:
  using ParamT = Tp;
  using reference = Tp&;
  using const_reference = const Tp&;
  using value_type = Tp;

  constexpr CompressedPairElem() = default;
  constexpr CompressedPairElem(DefaultInitTag) {}
  constexpr CompressedPairElem(ValueInitTag) : value_type() {}

  template <typename Up,
            std::enable_if_t<
                !std::is_same_v<CompressedPairElem, std::decay_t<Up>>, int> = 0>
  constexpr explicit CompressedPairElem(Up&& u)
      : value_type(std::forward<Up>(u)) {}

  template <typename... Args, size_t... Indexes>
  constexpr CompressedPairElem(std::piecewise_construct_t,
                               std::tuple<Args...> args,
                               std::index_sequence<Indexes...>)
      : value_type(std::forward<Args>(std::get<Indexes>(args))...) {}

  reference Get() noexcept { return *this; }
  const_reference Get() const noexcept { return *this; }
};

template <typename T1, typename T2>
class CompressedPair : private CompressedPairElem<T1, 0>,
                       private CompressedPairElem<T2, 1> {
 public:
  /// NOTE: This static assert should never fire because CompressedPairElem
  /// is *almost never* used in a scenario where it's possible for T1 == T2.
  /// (The exception is std::function where it is possible that the function
  /// object and the allocator have the same type).
  static_assert(
      (!std::is_same_v<T1, T2>),
      "CompressedPairElem cannot be instantiated when T1 and T2 are the same "
      "type; The current implementation is NOT ABI-compatible with the "
      "previous implementation for this configuration");

  using Base1 = CompressedPairElem<T1, 0>;
  using Base2 = CompressedPairElem<T2, 1>;

  template <
      bool Dummy = true,
      typename = std::enable_if_t<
          DependentType<std::is_default_constructible<T1>, Dummy>::value &&
          DependentType<std::is_default_constructible<T2>, Dummy>::value>>
  constexpr CompressedPair() : Base1(ValueInitTag()), Base2(ValueInitTag()) {}

  template <typename U1, typename U2>
  constexpr CompressedPair(U1&& u1, U2&& u2)
      : Base1(std::forward<U1>(u1)), Base2(std::forward<U2>(u2)) {}

  template <typename... Args1, typename... Args2>
  constexpr CompressedPair(std::piecewise_construct_t pc,
                           std::tuple<Args1...> first_args,
                           std::tuple<Args2...> second_args)
      : Base1(pc, std::move(first_args),
              std::make_index_sequence<sizeof...(Args1)>()),
        Base2(pc, std::move(second_args),
              std::make_index_sequence<sizeof...(Args2)>()) {}

  typename Base1::reference First() noexcept {
    return static_cast<Base1&>(*this).Get();
  }

  typename Base1::const_reference First() const noexcept {
    return static_cast<const Base1&>(*this).Get();
  }

  typename Base2::reference Second() noexcept {
    return static_cast<Base2&>(*this).Get();
  }

  typename Base2::const_reference Second() const noexcept {
    return static_cast<const Base2&>(*this).Get();
  }

  constexpr static Base1* GetFirstBase(CompressedPair* pair) noexcept {
    return static_cast<Base1*>(pair);
  }

  constexpr static Base2* GetSecondBase(CompressedPair* pair) noexcept {
    return static_cast<Base2*>(pair);
  }

  void swap(CompressedPair& other) noexcept(std::is_nothrow_swappable_v<T1> &&
                                            std::is_nothrow_swappable_v<T2>) {
    using std::swap;

    swap(First(), other.First());
    swap(Second(), other.Second());
  }
};

template <typename T1, typename T2>
inline void swap(
    CompressedPair<T1, T2>& lhs,
    CompressedPair<T1, T2>& rhs) noexcept(std::is_nothrow_swappable_v<T1> &&
                                          std::is_nothrow_swappable_v<T2>) {
  lhs.swap(rhs);
}

}  // namespace kiwi
