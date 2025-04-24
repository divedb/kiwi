// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/metrics/crc32.hh"

#include <gtest/gtest.h>
#include <stdint.h>

#include <array>

#include "kiwi/containers/span.hh"

namespace kiwi {

TEST(Crc32Test, ZeroTest) {
  span<const uint8_t> empty_data;
  EXPECT_EQ(0U, Crc32(0, empty_data));
}

TEST(Crc32Test, EmptyNonzeroTest) {
  span<const uint8_t> empty_data;
  EXPECT_EQ(99U, Crc32(99, empty_data));
}

TEST(Crc32Test, NonemptyTest) {
  std::array<uint8_t, 5> arr = {1, 2, 3, 4, 5};
  EXPECT_EQ(0x81296ee9U, Crc32(0, arr));
}

}  // namespace kiwi