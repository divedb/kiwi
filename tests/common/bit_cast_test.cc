// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/common/bit_cast.hh"

#include <gtest/gtest.h>

namespace kiwi {
namespace {

TEST(BitCastTest, FloatIntFloat) {
  float f = 3.1415926f;
  int i = bit_cast<int32_t>(f);
  float f2 = bit_cast<float>(i);
  EXPECT_EQ(f, f2);
}

struct A {
  int x;
};

TEST(BitCastTest, StructureInt) {
  A a = {1};
  int b = bit_cast<int>(a);
  EXPECT_EQ(1, b);
}

}  // namespace
}  // namespace kiwi