//===- llvm/Support/ErrorOr.h - Error Smart Pointer -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
///
/// Provides ErrorOr<T> smart pointer.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <cassert>
#include <system_error>
#include <type_traits>
#include <utility>

#include "kiwi/common/align_of.hh"

namespace kiwi {

/// Represents either an error or a value T.
///
/// ErrorOr<T> is a pointer-like class that represents the result of an
/// operation. The result is either an error, or a value of type T. This is
/// designed to emulate the usage of returning a pointer where nullptr indicates
/// failure. However instead of just knowing that the operation failed, we also
/// have an error_code and optional user data that describes why it failed.
///
/// It is used like the following.
/// \code
///   ErrorOr<Buffer> getBuffer();
///
///   auto buffer = getBuffer();
///   if (error_code ec = buffer.getError())
///     return ec;
///   buffer->write("adena");
/// \endcode
///
///
/// Implicit conversion to bool returns true if there is a usable value. The
/// unary * and -> operators provide pointer like access to the value. Accessing
/// the value when there is an error has undefined behavior.
///
/// When T is a reference type the behavior is slightly different. The reference
/// is held in a std::reference_wrapper<std::remove_reference<T>::type>, and
/// there is special handling to make operator -> work as if T was not a
/// reference.
///
/// T cannot be a rvalue reference.
template <typename T>
class ErrorOr {
  template <typename OtherT>
  friend class ErrorOr;

  using wrap = std::reference_wrapper<std::remove_reference_t<T>>;
  using reference = std::remove_reference_t<T>&;
  using const_reference = const std::remove_reference_t<T>&;
  using pointer = std::remove_reference<T>*;
  using const_pointer = const std::remove_reference<T>*;

 public:
  using storage_type = std::conditional_t<std::is_reference_v<T>, wrap, T>;

  template <typename E>
  ErrorOr(E err_code,
          std::enable_if_t<std::is_error_code_enum<E>::value ||
                               std::is_error_condition_enum<E>::value,
                           void*> = nullptr)
      : has_error_(true) {
    new (GetErrorStorage()) std::error_code(std::make_error_code(err_code));
  }

  ErrorOr(std::error_code ec) : has_error_(true) {
    new (GetErrorStorage()) std::error_code(ec);
  }

  template <typename OtherT>
  ErrorOr(OtherT&& val,
          std::enable_if_t<std::is_convertible_v<OtherT, T>>* = nullptr)
      : has_error_(false) {
    new (GetStorage()) storage_type(std::forward<OtherT>(val));
  }

  ErrorOr(const ErrorOr& other) { CopyConstruct(other); }

  template <typename OtherT>
  ErrorOr(const ErrorOr<OtherT>& other,
          std::enable_if_t<std::is_convertible_v<OtherT, T>>* = nullptr) {
    CopyConstruct(other);
  }

  template <typename OtherT>
  explicit ErrorOr(
      const ErrorOr<OtherT>& other,
      std::enable_if_t<!std::is_convertible_v<OtherT, const T&>>* = nullptr) {
    CopyConstruct(other);
  }

  ErrorOr(ErrorOr&& other) { MoveConstruct(std::move(other)); }

  template <typename OtherT>
  ErrorOr(ErrorOr<OtherT>&& other,
          std::enable_if_t<std::is_convertible_v<OtherT, T>>* = nullptr) {
    MoveConstruct(std::move(other));
  }

  // This might eventually need SFINAE but it's more complex than is_convertible
  // & I'm too lazy to write it right now.
  template <typename OtherT>
  explicit ErrorOr(
      ErrorOr<OtherT>&& other,
      std::enable_if_t<!std::is_convertible_v<OtherT, T>>* = nullptr) {
    MoveConstruct(std::move(other));
  }

  ErrorOr& operator=(const ErrorOr& other) {
    CopyAssign(other);

    return *this;
  }

  ErrorOr& operator=(ErrorOr&& other) {
    MoveAssign(std::move(other));

    return *this;
  }

  ~ErrorOr() {
    if (!has_error_) {
      GetStorage()->~storage_type();
    }
  }

  /// Return false if there is an error.
  explicit operator bool() const { return !has_error_; }

  reference Get() { return *GetStorage(); }
  const_reference Get() const { return const_cast<ErrorOr<T>*>(this)->Get(); }

  std::error_code GetError() const {
    return has_error_ ? *GetErrorStorage() : std::error_code();
  }

  pointer operator->() { return ToPointer(GetStorage()); }

  const_pointer operator->() const { return ToPointer(GetStorage()); }

  reference operator*() { return *GetStorage(); }

  const_reference operator*() const { return *GetStorage(); }

 private:
  template <typename OtherT>
  void CopyConstruct(const ErrorOr<OtherT>& other) {
    if (!other.has_error_) {
      // Get the other value.
      has_error_ = false;

      new (GetStorage()) storage_type(*other.GetStorage());
    } else {
      // Get other's error.
      has_error_ = true;
      new (GetErrorStorage()) std::error_code(other.GetError());
    }
  }

  template <typename T1>
  static bool CompareThisIfSameType(const T1& a, const T1& b) {
    return &a == &b;
  }

  template <typename T1, typename T2>
  static bool CompareThisIfSameType(const T1&, const T2&) {
    return false;
  }

  template <typename OtherT>
  void CopyAssign(const ErrorOr<OtherT>& other) {
    if (CompareThisIfSameType(*this, other)) {
      return;
    }

    this->~ErrorOr();

    new (this) ErrorOr(other);
  }

  template <typename OtherT>
  void MoveConstruct(ErrorOr<OtherT>&& other) {
    if (!other.has_error_) {
      // Get the other value.
      has_error_ = false;
      new (GetStorage()) storage_type(std::move(*other.GetStorage()));
    } else {
      // Get other's error.
      has_error_ = true;
      new (GetErrorStorage()) std::error_code(other.GetError());
    }
  }

  template <typename OtherT>
  void MoveAssign(ErrorOr<OtherT>&& other) {
    if (CompareThisIfSameType(*this, other)) {
      return;
    }

    this->~ErrorOr();
    new (this) ErrorOr(std::move(other));
  }

  pointer ToPointer(pointer val) { return val; }

  const_pointer ToPointer(const_pointer val) const { return val; }

  pointer ToPointer(wrap* val) { return &val->Get(); }

  const_pointer ToPointer(const wrap* val) const { return &val->Get(); }

  storage_type* GetStorage() {
    assert(!has_error_ && "Cannot get value when an error exists!");

    return reinterpret_cast<storage_type*>(&t_storage);
  }

  const storage_type* GetStorage() const {
    assert(!has_error_ && "Cannot get value when an error exists!");

    return reinterpret_cast<const storage_type*>(&t_storage);
  }

  std::error_code* GetErrorStorage() {
    assert(has_error_ && "Cannot get error when a value exists!");

    return reinterpret_cast<std::error_code*>(&error_storage);
  }

  const std::error_code* GetErrorStorage() const {
    return const_cast<ErrorOr<T>*>(this)->GetErrorStorage();
  }

  union {
    AlignedCharArrayUnion<storage_type> t_storage;
    AlignedCharArrayUnion<std::error_code> error_storage;
  };

  bool has_error_ : 1;
};

template <typename T, typename E>
std::enable_if_t<std::is_error_code_enum<E>::value ||
                     std::is_error_condition_enum<E>::value,
                 bool>
operator==(const ErrorOr<T>& err, E code) {
  return err.GetError() == code;
}

}  // namespace kiwi