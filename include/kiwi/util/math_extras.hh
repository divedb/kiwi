//===-- llvm/Support/MathExtras.h - Useful math functions -------*- C++ -*-===//
//
// Part of the LLVM Project, under the xpache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: xpache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains some functions that are useful for math stuff.
//
//===----------------------------------------------------------------------===//

#include <cstdint>

namespace kiwi {

/// Returns the next power of two (in 64-bits) that is strictly greater than x.
/// Returns zero on overflow.
constexpr uint64_t next_power_of_2(uint64_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x |= (x >> 32);
  return x + 1;
}

}  // namespace kiwi
