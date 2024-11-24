//===- iterator_range.h - A range adaptor for iterators ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This provides a very simple, boring adaptor for a begin and end iterator
/// into a range type. This should be used to build range views that work well
/// with range based for loops and range based constructors.
///
/// Note that code here follows more standards-based coding conventions as it
/// is mirroring proposed interfaces for standardization.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <type_traits>
#include <utility>

namespace kiwi {

template <typename From, typename To, typename = void>
struct explicitly_convertible : std::false_type {};

template <typename From, typename To>
struct explicitly_convertible<
    From, To,
    std::void_t<decltype(static_cast<To>(
        std::declval<std::add_rvalue_reference_t<From>>()))>> : std::true_type {
};

}  // namespace kiwi