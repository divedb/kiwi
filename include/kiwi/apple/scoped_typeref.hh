// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "kiwi/common/logging.hh"
#include "kiwi/memory/scoped_policy.hh"

namespace kiwi::apple {

// ScopedTypeRef<> is patterned after std::shared_ptr<>, but maintains ownership
// of a reference to any type that is maintained by Retain and Release methods.
//
// The Traits structure must provide the Retain and Release methods for type T.
// A default ScopedTypeRefTraits is used but not defined, and should be defined
// for each type to use this interface. For example, an appropriate definition
// of ScopedTypeRefTraits for CGLContextObj would be:
//
//   template<>
//   struct ScopedTypeRefTraits<CGLContextObj> {
//     static CGLContextObj InvalidValue() { return nullptr; }
//     static CGLContextObj Retain(CGLContextObj object) {
//       CGLContextRetain(object);
//       return object;
//     }
//     static void Release(CGLContextObj object) { CGLContextRelease(object); }
//   };
//
// For the many types that have pass-by-pointer create functions, the function
// InitializeInto() is provided to allow direct initialization and assumption
// of ownership of the object. For example, continuing to use the above
// CGLContextObj specialization:
//
//   kiwi::apple::ScopedTypeRef<CGLContextObj> context;
//   CGLCreateContext(pixel_format, share_group, context.InitializeInto());
//
// For initialization with an existing object, the caller may specify whether
// the ScopedTypeRef<> being initialized is assuming the caller's existing
// ownership of the object (and should not call Retain in initialization) or if
// it should not assume this ownership and must create its own (by calling
// Retain in initialization). This behavior is based on the `policy` parameter,
// with `kAssume` for the former and `kRetain` for the latter. The default
// policy is to `kAssume`.

template <typename T>
struct ScopedTypeRefTraits;

template <typename T, typename Traits = ScopedTypeRefTraits<T>>
class ScopedTypeRef {
 public:
  using element_type = T;

  // Construction from underlying type

  explicit constexpr ScopedTypeRef(element_type object = Traits::InvalidValue(),
                                   kiwi::scoped_policy::OwnershipPolicy policy =
                                       kiwi::scoped_policy::kAssume)
      : object_(object) {
    if (object_ != Traits::InvalidValue() &&
        policy == kiwi::scoped_policy::kRetain) {
      object_ = Traits::Retain(object_);
    }
  }

  // The pattern in the four [copy|move] [constructors|assignment operators]
  // below is that for each of them there is the standard version for use by
  // scopers wrapping objects of this type, and a templated version to handle
  // scopers wrapping objects of subtypes. One might think that one could get
  // away only the templated versions, as their templates should match the
  // usage, but that doesn't work. Having a templated function that matches the
  // types of, say, a copy constructor, doesn't count as a copy constructor, and
  // the compiler's generated copy constructor is incorrect.

  // Copy construction

  ScopedTypeRef(const ScopedTypeRef<T, Traits>& that) : object_(that.get()) {
    if (object_ != Traits::InvalidValue()) {
      object_ = Traits::Retain(object_);
    }
  }

  template <typename R, typename RTraits>
  ScopedTypeRef(const ScopedTypeRef<R, RTraits>& that) : object_(that.get()) {
    if (object_ != Traits::InvalidValue()) {
      object_ = Traits::Retain(object_);
    }
  }

  // Copy assignment

  ScopedTypeRef& operator=(const ScopedTypeRef<T, Traits>& that) {
    reset(that.get(), kiwi::scoped_policy::kRetain);
    return *this;
  }

  template <typename R, typename RTraits>
  ScopedTypeRef& operator=(const ScopedTypeRef<R, RTraits>& that) {
    reset(that.get(), kiwi::scoped_policy::kRetain);
    return *this;
  }

  // Move construction

  ScopedTypeRef(ScopedTypeRef<T, Traits>&& that) : object_(that.release()) {}

  template <typename R, typename RTraits>
  ScopedTypeRef(ScopedTypeRef<R, RTraits>&& that) : object_(that.release()) {}

  // Move assignment

  ScopedTypeRef& operator=(ScopedTypeRef<T, Traits>&& that) {
    reset(that.release(), kiwi::scoped_policy::kAssume);
    return *this;
  }

  template <typename R, typename RTraits>
  ScopedTypeRef& operator=(ScopedTypeRef<R, RTraits>&& that) {
    reset(that.release(), kiwi::scoped_policy::kAssume);
    return *this;
  }

  // Resetting

  template <typename R, typename RTraits>
  void reset(const ScopedTypeRef<R, RTraits>& that) {
    reset(that.get(), kiwi::scoped_policy::kRetain);
  }

  void reset(element_type object = Traits::InvalidValue(),
             kiwi::scoped_policy::OwnershipPolicy policy =
                 kiwi::scoped_policy::kAssume) {
    if (object != Traits::InvalidValue() &&
        policy == kiwi::scoped_policy::kRetain) {
      object = Traits::Retain(object);
    }
    if (object_ != Traits::InvalidValue()) {
      Traits::Release(object_);
    }
    object_ = object;
  }

  // Destruction

  ~ScopedTypeRef() {
    if (object_ != Traits::InvalidValue()) {
      Traits::Release(object_);
    }
  }

  // This is to be used only to take ownership of objects that are created by
  // pass-by-pointer create functions. To enforce this, require that this object
  // be empty before use.
  [[nodiscard]] element_type* InitializeInto() {
    CHECK_EQ(object_, Traits::InvalidValue());
    return &object_;
  }

  bool operator==(const ScopedTypeRef& that) const {
    return object_ == that.object_;
  }

  explicit operator bool() const { return object_ != Traits::InvalidValue(); }

  element_type get() const { return object_; }

  void swap(ScopedTypeRef& that) {
    element_type temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // ScopedTypeRef<>::release() is like std::unique_ptr<>::release.  It is NOT
  // a wrapper for Release().  To force a ScopedTypeRef<> object to call
  // Release(), use ScopedTypeRef<>::reset().
  [[nodiscard]] element_type release() {
    element_type temp = object_;
    object_ = Traits::InvalidValue();
    return temp;
  }

 private:
  element_type object_;
};

}  // namespace kiwi::apple
