///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>     // for forward
#include <cstddef>       // for ptrdiff_t, nullptr_t, size_t
#include <memory>        // for shared_ptr, unique_ptr
#include <system_error>  // for hash
#include <type_traits>   // for enable_if_t, is_convertible, is_assignable
#include <utility>       // for declval

#include "kiwi/util/assert.hh"

namespace kiwi {

namespace detail {

template <typename T, typename = void>
struct is_comparable_to_nullptr : std::false_type {};

template <typename T>
struct is_comparable_to_nullptr<
    T, std::enable_if_t<std::is_convertible<
           decltype(std::declval<T>() != nullptr), bool>::value>>
    : std::true_type {};

/// Resolves to the more efficient of `const T` or `const T&`, in the context of
/// returning a const-qualified value of type T.
///
/// Copied from cppfront's implementation of the CppCoreGuidelines F.16
/// (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-in)
template <typename T>
using value_or_reference_return_t =
    std::conditional_t<sizeof(T) < 2 * sizeof(void*) &&
                           std::is_trivially_copy_constructible<T>::value,
                       const T, const T&>;

}  // namespace detail

using std::shared_ptr;
using std::unique_ptr;

/// \brief `Owner<T>` is designed as a safety mechanism for code that must deal
///         directly with raw pointers that own memory. Ideally such code should
///         be restricted to the implementation of low-level abstractions.
///         `Owner<T>` can also be used as a stepping point in converting legacy
///         code to use more modern RAII constructs, such as smart pointers.
///
/// @tparam T Must be a pointer type. And disallow construction from any type
///         other than pointer type
template <class T, class = std::enable_if_t<std::is_pointer<T>::value>>
using Owner = T;

/// \brief Restricts a pointer or smart pointer to only hold non-null values.
///        And it has zero size overhead over T.
///
/// If T is a pointer (i.e. T == U*) then
/// - allow construction from U*
/// - disallow construction from nullptr_t
/// - disallow default construction
/// - ensure construction from null U* fails
/// - allow implicit conversion to U*
///
/// \tparam T Must be type of pointer or comparable with pointer.
template <typename T>
class NotNull {
 public:
  static_assert(detail::is_comparable_to_nullptr<T>::value,
                "T cannot be compared to nullptr.");

  /// @name Constructors.
  /// @{

  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  constexpr NotNull(U&& u) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : ptr_(std::forward<U>(u)) {
    EXPECT(ptr_ != nullptr);
  }

  template <
      typename = std::enable_if_t<!std::is_same<std::nullptr_t, T>::value>>
  constexpr NotNull(T u) noexcept(std::is_nothrow_move_constructible<T>::value)
      : ptr_(std::move(u)) {
    EXPECT(ptr_ != nullptr);
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  constexpr NotNull(const NotNull<U>& other) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : NotNull(other.get()) {}

  NotNull(const NotNull& other) = default;

  /// @}

  NotNull& operator=(const NotNull& other) = default;
  constexpr detail::value_or_reference_return_t<T> get() const
      noexcept(noexcept(detail::value_or_reference_return_t<T>{
          std::declval<T&>()})) {
    return ptr_;
  }

  constexpr operator T() const { return get(); }
  constexpr decltype(auto) operator->() const { return get(); }
  constexpr decltype(auto) operator*() const { return *get(); }

  /// \brief Prevents compilation when someone attempts to assign a null pointer
  ///        constant.
  NotNull(std::nullptr_t) = delete;
  NotNull& operator=(std::nullptr_t) = delete;

  /// \brief Unwanted operators...pointers only point to single objects!
  NotNull& operator++() = delete;
  NotNull& operator--() = delete;
  NotNull operator++(int) = delete;
  NotNull operator--(int) = delete;
  NotNull& operator+=(std::ptrdiff_t) = delete;
  NotNull& operator-=(std::ptrdiff_t) = delete;
  void operator[](std::ptrdiff_t) const = delete;

 private:
  T ptr_;
};

template <typename T>
auto make_not_null(T&& t) noexcept {
  return NotNull<std::remove_cv_t<std::remove_reference_t<T>>>{
      std::forward<T>(t)};
}

template <typename T, typename U>
auto operator==(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(lhs.get() == rhs.get())) -> decltype(lhs.get() == rhs.get()) {
  return lhs.get() == rhs.get();
}

template <typename T, typename U>
auto operator!=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(lhs.get() != rhs.get())) -> decltype(lhs.get() != rhs.get()) {
  return lhs.get() != rhs.get();
}

template <typename T, typename U>
auto operator<(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::less<>{}(lhs.get(), rhs.get())))
    -> decltype(std::less<>{}(lhs.get(), rhs.get())) {
  return std::less<>{}(lhs.get(), rhs.get());
}

template <typename T, typename U>
auto operator<=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::less_equal<>{}(lhs.get(), rhs.get())))
    -> decltype(std::less_equal<>{}(lhs.get(), rhs.get())) {
  return std::less_equal<>{}(lhs.get(), rhs.get());
}

template <typename T, typename U>
auto operator>(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::greater<>{}(lhs.get(), rhs.get())))
    -> decltype(std::greater<>{}(lhs.get(), rhs.get())) {
  return std::greater<>{}(lhs.get(), rhs.get());
}

template <typename T, typename U>
auto operator>=(const NotNull<T>& lhs, const NotNull<U>& rhs) noexcept(
    noexcept(std::greater_equal<>{}(lhs.get(), rhs.get())))
    -> decltype(std::greater_equal<>{}(lhs.get(), rhs.get())) {
  return std::greater_equal<>{}(lhs.get(), rhs.get());
}

/// More unwanted operators.
template <typename T, typename U>
std::ptrdiff_t operator-(const NotNull<T>&, const NotNull<U>&) = delete;
template <typename T>
NotNull<T> operator-(const NotNull<T>&, std::ptrdiff_t) = delete;
template <typename T>
NotNull<T> operator+(const NotNull<T>&, std::ptrdiff_t) = delete;
template <typename T>
NotNull<T> operator+(std::ptrdiff_t, const NotNull<T>&) = delete;

template <typename T, typename U = decltype(std::declval<const T&>().get()),
          bool = std::is_default_constructible<std::hash<U>>::value>
struct NotNullHash {
  std::size_t operator()(const T& value) const {
    return std::hash<U>{}(value.get());
  }
};

template <typename T, typename U>
struct NotNullHash<T, U, false> {
  NotNullHash() = delete;
  NotNullHash(const NotNullHash&) = delete;
  NotNullHash& operator=(const NotNullHash&) = delete;
};

}  // namespace kiwi

namespace std {

template <typename T>
struct hash<kiwi::NotNull<T>> : kiwi::NotNullHash<kiwi::NotNull<T>> {};

}  // namespace std

namespace kiwi {

/// Restricts a pointer or smart pointer to only hold non-null values.
/// - provides a strict (i.e. explicit constructor from T) wrapper of not_null
/// - to be used for new code that wishes the design to be cleaner and make
///   not_null checks intentional, or in old code that would like to make the
///   transition.
///
/// To make the transition from not_null, incrementally replace not_null
/// by strict_not_null and fix compilation errors.
///
/// Expect to
/// - remove all unneeded conversions from raw pointer to not_null and back
/// - make API clear by specifying not_null in parameters where needed
/// - remove unnecessary asserts
template <typename T>
class StrictNotNull : public NotNull<T> {
 public:
  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  constexpr explicit StrictNotNull(U&& u) : NotNull<T>(std::forward<U>(u)) {}

  template <
      typename = std::enable_if_t<!std::is_same<std::nullptr_t, T>::value>>
  constexpr explicit StrictNotNull(T u) : NotNull<T>(u) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  constexpr StrictNotNull(const NotNull<U>& other) : NotNull<T>(other) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible<U, T>::value>>
  constexpr StrictNotNull(const StrictNotNull<U>& other) : NotNull<T>(other) {}

  /// To avoid invalidating the "not null" invariant, the contained pointer is
  /// actually copied instead of moved. If it is a custom pointer, its
  /// constructor could in theory throw exceptions.
  StrictNotNull(StrictNotNull&& other) noexcept(
      std::is_nothrow_copy_constructible<T>::value) = default;
  StrictNotNull(const StrictNotNull& other) = default;
  StrictNotNull& operator=(const StrictNotNull& other) = default;
  StrictNotNull& operator=(const NotNull<T>& other) {
    NotNull<T>::operator=(other);
    return *this;
  }

  /// Prevents compilation when someone attempts to assign a null pointer
  /// constant
  StrictNotNull(std::nullptr_t) = delete;
  StrictNotNull& operator=(std::nullptr_t) = delete;

  /// Unwanted operators...pointers only point to single objects!
  StrictNotNull& operator++() = delete;
  StrictNotNull& operator--() = delete;
  StrictNotNull operator++(int) = delete;
  StrictNotNull operator--(int) = delete;
  StrictNotNull& operator+=(std::ptrdiff_t) = delete;
  StrictNotNull& operator-=(std::ptrdiff_t) = delete;
  void operator[](std::ptrdiff_t) const = delete;
};

// more unwanted operators
template <typename T, typename U>
std::ptrdiff_t operator-(const StrictNotNull<T>&,
                         const StrictNotNull<U>&) = delete;
template <typename T>
StrictNotNull<T> operator-(const StrictNotNull<T>&, std::ptrdiff_t) = delete;
template <typename T>
StrictNotNull<T> operator+(const StrictNotNull<T>&, std::ptrdiff_t) = delete;
template <typename T>
StrictNotNull<T> operator+(std::ptrdiff_t, const StrictNotNull<T>&) = delete;

template <typename T>
auto make_strict_not_null(T&& t) noexcept {
  return StrictNotNull<std::remove_cv_t<std::remove_reference_t<T>>>{
      std::forward<T>(t)};
}

#if (__cplusplus >= 201703L)
// Deduction guides to prevent the ctad-maybe-unsupported warning
template <class T>
NotNull(T) -> NotNull<T>;

template <class T>
StrictNotNull(T) -> StrictNotNull<T>;
#endif

}  // namespace kiwi

namespace std {

template <typename T>
struct hash<kiwi::StrictNotNull<T>>
    : kiwi::NotNullHash<kiwi::StrictNotNull<T>> {};

}  // namespace std