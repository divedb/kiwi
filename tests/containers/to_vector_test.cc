// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/containers/to_vector.hh"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <ranges>
#include <set>

#include "kiwi/containers/adapters.hh"
#include "kiwi/containers/flat_set.hh"

namespace kiwi::test {

template <class C>
void IdentityTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c);
  EXPECT_THAT(vec, testing::ElementsAre(1, 2, 3, 4, 5));
}

template <class C>
void ProjectionTest() {
  C c = {1, 2, 3, 4, 5};
  auto vec = ToVector(c, [](int x) { return x + 1; });
  EXPECT_THAT(vec, testing::ElementsAre(2, 3, 4, 5, 6));
}

TEST(ToVectorTest, Identity) {
  IdentityTest<std::vector<int>>();
  IdentityTest<std::set<int>>();
  IdentityTest<int[]>();
  IdentityTest<kiwi::flat_set<int>>();
}

TEST(ToVectorTest, Projection) {
  ProjectionTest<std::vector<int>>();
  ProjectionTest<std::set<int>>();
  ProjectionTest<int[]>();
  ProjectionTest<kiwi::flat_set<int>>();
}

TEST(ToVectorTest, MoveOnly) {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::make_unique<int>(1));
  v.push_back(std::make_unique<int>(2));
  v.push_back(std::make_unique<int>(3));

  auto v2 = kiwi::ToVector(kiwi::RangeAsRvalues(std::move(v)));
  EXPECT_THAT(v2, testing::ElementsAre(testing::Pointee(1), testing::Pointee(2),
                                       testing::Pointee(3)));

  // The old vector should be consumed. The standard guarantees that a
  // moved-from std::unique_ptr will be null.
  // NOLINT(bugprone-use-after-move)
  EXPECT_THAT(v, testing::ElementsAre(testing::IsNull(), testing::IsNull(),
                                      testing::IsNull()));

  // Another method which is more verbose so not preferable.
  auto v3 = kiwi::ToVector(
      std::move(v2), [](std::unique_ptr<int>& p) { return std::move(p); });
  EXPECT_THAT(v3, testing::ElementsAre(testing::Pointee(1), testing::Pointee(2),
                                       testing::Pointee(3)));
  // NOLINT(bugprone-use-after-move)
  EXPECT_THAT(v2, testing::ElementsAre(testing::IsNull(), testing::IsNull(),
                                       testing::IsNull()));
}

template <typename C, typename Proj, typename T>
constexpr bool CorrectlyProjected =
    std::is_same_v<T,
                   typename decltype(ToVector(
                       std::declval<C>(), std::declval<Proj>()))::value_type>;

TEST(ToVectorTest, CorrectlyProjected) {
  // Tests that projected types are deduced correctly.
  constexpr auto proj = [](const auto& value) -> const auto& { return value; };
  static_assert(CorrectlyProjected<std::vector<std::string>, decltype(proj),
                                   std::string>);
  static_assert(
      CorrectlyProjected<std::set<std::string>, decltype(&std::string::length),
                         std::size_t>);
}

}  // namespace kiwi::test