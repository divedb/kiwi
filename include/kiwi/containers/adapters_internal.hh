// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stddef.h>

#include <iterator>
#include <ranges>
#include <utility>

#include "kiwi/portability/compiler_specific.hh"

namespace kiwi::internal {
template <typename Range>
class RangeOfRvaluesAdapter;
template <typename Range>
class ReversedAdapter;
}  // namespace kiwi::internal

// This is technically correct, but presently always evaluates to false since
// `RangeAsRvalues()` bans non-borrowed ranges.
template <typename Range>
inline constexpr bool std::ranges::enable_borrowed_range<
    kiwi::internal::RangeOfRvaluesAdapter<Range>> =
    std::ranges::borrowed_range<Range>;

template <typename Range>
inline constexpr bool
    std::ranges::enable_borrowed_range<kiwi::internal::ReversedAdapter<Range>> =
        std::ranges::borrowed_range<Range>;

namespace kiwi::internal {

template <typename Range>
class RangeOfRvaluesAdapter {
 public:
  explicit RangeOfRvaluesAdapter(Range&& range KIWI_LIFETIME_BOUND)
      : range_(std::forward<Range>(range)) {}
  RangeOfRvaluesAdapter(const RangeOfRvaluesAdapter&) = default;
  RangeOfRvaluesAdapter& operator=(const RangeOfRvaluesAdapter&) = delete;

  auto size() const
    requires std::ranges::sized_range<Range>
  {
    return std::ranges::size(range_);
  }

  auto begin() { return std::make_move_iterator(std::ranges::begin(range_)); }
  auto end() { return std::make_move_iterator(std::ranges::end(range_)); }

 private:
  // RAW_PTR_EXCLUSION: References a STACK_ALLOCATED class. It is only used
  // inside for loops. Ideally, the container being iterated over should be the
  // one held via a raw_ref/raw_ptrs.
  Range&& range_;
};

// Internal adapter class for implementing kiwi::Reversed.
// TODO(crbug.com/378623811): Parts of this (e.g. the `size()` helper) should be
// extracted to a base template that can be shared/reused. In addition, this
// should be constrained to Ts that satisfy the std::ranges::range concept.
template <typename Range>
class ReversedAdapter {
 public:
  explicit ReversedAdapter(Range&& range KIWI_LIFETIME_BOUND)
      : range_(std::forward<Range>(range)) {}
  ReversedAdapter(const ReversedAdapter&) = default;
  ReversedAdapter& operator=(const ReversedAdapter&) = delete;

  auto begin() { return std::rbegin(range_); }
  auto begin() const { return std::rbegin(range_); }
  auto cbegin() const { return std::crbegin(range_); }

  auto end() { return std::rend(range_); }
  auto end() const { return std::rend(range_); }
  auto cend() const { return std::crend(range_); }

  auto size() const
    requires std::ranges::sized_range<Range>
  {
    return std::ranges::size(range_);
  }

 private:
  // RAW_PTR_EXCLUSION: References a STACK_ALLOCATED class. It is only used
  // inside for loops. Ideally, the container being iterated over should be the
  // one held via a raw_ref/raw_ptrs.
  Range&& range_;
};

}  // namespace kiwi::internal
