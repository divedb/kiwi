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

#include <cstddef>
#include <iterator>
#include <type_traits>

/// This contains stripped-down workalikes of some Boost classes:
///
///   iterator_adaptor
///   iterator_facade
///
/// Rationale: the boost headers providing those classes are surprisingly large.
/// The bloat comes from the headers themselves, but more so, their transitive
/// includes.
///
/// These implementations are simple and minimal.  They may be missing features
/// provided by the Boost classes mentioned above.  Also at this time they only
/// support forward-iterators.  They provide just enough for the few uses within
/// Folly libs; more features will be slapped in here if and when they're
/// needed.
///
/// These classes may possibly add features as well.  Care is taken not to
/// change functionality where it's expected to be the same (e.g. `dereference`
/// will do the same thing).
///
/// These are currently only intended for use within Folly, hence their living
/// under detail.  Use outside Folly is not recommended.
///
/// To see how to use these classes, find the instances where this is used
/// within Folly libs.  Common use cases can also be found in
/// `IteratorsTest.cpp`.
namespace kiwi {
namespace detail {

/// Currently this only supports forward and bidirectional iteration.  The
/// derived class must must have definitions for these methods:
///
/// \code
///   void Increment();
///   void Decrement(); // optional, to be used with bidirectional
///   reference Dereference() const;
///   bool IsEqual([appropriate iterator type] const& rhs) const;
/// \endcode
///
/// These names are consistent with those used by the Boost iterator
/// facade / adaptor classes to ease migration classes in this file.
///
/// \tparam D The deriving class (CRTP).
/// \tparam V The value type.
/// \tparam Tag The iterator category, one of:
///         - std::forward_iterator_tag
///         - std::bidirectional_iterator_tag
template <typename D, typename V, typename Tag>
class IteratorFacade {
  static bool IsEqual(D const& lhs, D const& rhs) { return lhs.IsEqual(rhs); }

  D& AsDerived() { return static_cast<D&>(*this); }
  D const& AsDerivedConst() const { return static_cast<D const&>(*this); }

 public:
  using value_type = V;
  using reference = value_type&;
  using pointer = value_type*;
  using difference_type = std::ptrdiff_t;
  using iterator_category = Tag;

  friend bool operator==(D const& lhs, D const& rhs) { return equal(lhs, rhs); }
  friend bool operator!=(D const& lhs, D const& rhs) { return !(lhs == rhs); }

  V& operator*() const { return AsDerivedConst().Dereference(); }
  V* operator->() const { return std::addressof(operator*()); }
  D& operator++() {
    AsDerived().Increment();

    return AsDerived();
  }

  D operator++(int) {
    auto ret = AsDerived();  // copy
    AsDerived().Increment();

    return ret;
  }

  D& operator--() {
    AsDerived().Decrement();

    return AsDerived();
  }

  D operator--(int) {
    auto ret = AsDerived();  // copy
    AsDerived().Decrement();

    return ret;
  }
};

/// Wrap one iterator while providing an interator interface with e.g. a
/// different value_type.
///
/// \tparam D The deriving class (CRTP).
/// \tparam I The wrapper iterator type.
/// \tparam V The value type.
/// \tparam Tag The iterator category tag.
template <typename D, typename I, typename V, typename Tag>
class IteratorAdaptor : public IteratorFacade<D, V, Tag> {
 public:
  using Super = IteratorFacade<D, V, Tag>;
  using value_type = typename Super::value_type;
  using iterator_category = typename Super::iterator_category;
  using reference = typename Super::reference;
  using pointer = typename Super::pointer;
  using difference_type = typename Super::difference_type;

  IteratorAdaptor() = default;
  explicit IteratorAdaptor(I base) : base_(std::move(base)) {}

  void Increment() { ++base_; }
  void Decrement() { --base_; }
  V& Dereference() const { return *base_; }
  bool IsEqual(D const& rhs) const { return base_ == rhs.base_; }

  I const& Base() const { return base_; }
  I& Base() { return base_; }

 private:
  I base_;
};

}  // namespace detail
}  // namespace kiwi
