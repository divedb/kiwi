// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <utility>

#include "kiwi/common/function.hh"
#include "kiwi/common/no_destructor.hh"
#include "kiwi/common/scope_exit.hh"

namespace kiwi {

// A move-only class that holds an integer. This is designed for testing
// containers. See also CopyOnlyInt.
class MoveOnlyInt {
 public:
  explicit MoveOnlyInt(int data = 1) : data_(data) {}
  MoveOnlyInt(MoveOnlyInt&& other) : data_(other.data_) { other.data_ = 0; }

  MoveOnlyInt(const MoveOnlyInt&) = delete;
  MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;

  ~MoveOnlyInt() {
    int old_data = std::exchange(data_, 0);

    if (GetDestructionCallbackStorage()) {
      GetDestructionCallbackStorage()(old_data);
    }
  }

  MoveOnlyInt& operator=(MoveOnlyInt&& other) {
    data_ = other.data_;
    other.data_ = 0;
    return *this;
  }

  friend bool operator==(const MoveOnlyInt& lhs,
                         const MoveOnlyInt& rhs) = default;
  friend auto operator<=>(const MoveOnlyInt& lhs,
                          const MoveOnlyInt& rhs) = default;

  friend bool operator==(const MoveOnlyInt& lhs, int rhs) {
    return lhs.data_ == rhs;
  }
  friend bool operator==(int lhs, const MoveOnlyInt& rhs) {
    return lhs == rhs.data_;
  }
  friend auto operator<=>(const MoveOnlyInt& lhs, int rhs) {
    return lhs.data_ <=> rhs;
  }
  friend auto operator<=>(int lhs, const MoveOnlyInt& rhs) {
    return lhs <=> rhs.data_;
  }

  int data() const { return data_; }

  // Called with the value of `data()` when an instance of `MoveOnlyInt` is
  // destroyed. Returns an `absl::Cleanup` scoper that automatically
  // unregisters the callback when the scoper is destroyed.
  static auto SetScopedDestructionCallback(Function<void(int)> callback) {
    GetDestructionCallbackStorage() = std::move(callback);

    return make_scope_exit([]() { GetDestructionCallbackStorage() = nullptr; });
  }

 private:
  static Function<void(int)>& GetDestructionCallbackStorage() {
    static NoDestructor<Function<void(int)>> callback;

    return *callback;
  }

  volatile int data_;
};

}  // namespace kiwi
