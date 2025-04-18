//===- llvm/ADT/EpochTracker.h - ADT epoch tracking --------------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines the DebugEpochBase and DebugEpochBase::HandleBase classes.
/// These can be used to write iterators that are fail-fast when LLVM is built
/// with asserts enabled.
///
//===----------------------------------------------------------------------===//

#pragma once

#include <cstdint>

namespace kiwi {

/// A base class for data structure classes wishing to make iterators
/// ("handles") pointing into themselves fail-fast.  When building without
/// asserts, this class is empty and does nothing.
///
/// DebugEpochBase does not by itself track handles pointing into itself.  The
/// expectation is that routines touching the handles will poll on
/// isHandleInSync at appropriate points to assert that the handle they're using
/// is still valid.
///
class DebugEpochBase {
  uint64_t epoch_ = 0;

 public:
  DebugEpochBase() = default;

  /// Calling increment_epoch invalidates all handles pointing into the
  /// calling instance.
  void increment_epoch() { ++epoch_; }

  /// The destructor calls increment_epoch to make use-after-free bugs
  /// more likely to crash deterministically.
  ~DebugEpochBase() { increment_epoch(); }

  /// A base class for iterator classes ("handles") that wish to poll for
  /// iterator invalidating modifications in the underlying data structure.
  /// When LLVM is built without asserts, this class is empty and does nothing.
  ///
  /// HandleBase does not track the parent data structure by itself.  It expects
  /// the routines modifying the data structure to call increment_epoch when
  /// they make an iterator-invalidating modification.
  ///
  class HandleBase {
    const uint64_t* epoch_address_ = nullptr;
    uint64_t epoch_at_creation_ = UINT64_MAX;

   public:
    HandleBase() = default;

    explicit HandleBase(const DebugEpochBase* parent)
        : epoch_address_(&parent->epoch_), epoch_at_creation_(parent->epoch_) {}

    /// Returns true if the DebugEpochBase this Handle is linked to has
    /// not called increment_epoch on itself since the creation of this
    /// HandleBase instance.
    bool is_handle_in_sync() const {
      return *epoch_address_ == epoch_at_creation_;
    }

    /// Returns a pointer to the epoch word stored in the data structure
    /// this handle points into.  Can be used to check if two iterators point
    /// into the same data structure.
    const void* get_epoch_address() const { return epoch_address_; }
  };
};

}  // namespace kiwi