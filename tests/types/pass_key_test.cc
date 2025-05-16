// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/types/pass_key.hh"

#include <concepts>
#include <utility>

namespace kiwi {
namespace {

class Manager;

// May not be created without a PassKey.
class Restricted {
 public:
  explicit Restricted(kiwi::PassKey<Manager>) {}
};

Restricted ConstructWithCopiedPassKey(kiwi::PassKey<Manager> key) {
  return Restricted(key);
}

Restricted ConstructWithMovedPassKey(kiwi::PassKey<Manager> key) {
  return Restricted(std::move(key));
}

class Manager {
 public:
  enum class ExplicitConstruction { kTag };
  enum class UniformInitialization { kTag };
  enum class CopiedKey { kTag };
  enum class MovedKey { kTag };

  explicit Manager(ExplicitConstruction)
      : restricted_(kiwi::PassKey<Manager>()) {}
  explicit Manager(UniformInitialization) : restricted_({}) {}
  explicit Manager(CopiedKey) : restricted_(ConstructWithCopiedPassKey({})) {}
  explicit Manager(MovedKey) : restricted_(ConstructWithMovedPassKey({})) {}

 private:
  Restricted restricted_;
};

static_assert(std::constructible_from<Manager, Manager::ExplicitConstruction>);
static_assert(std::constructible_from<Manager, Manager::UniformInitialization>);
static_assert(std::constructible_from<Manager, Manager::CopiedKey>);
static_assert(std::constructible_from<Manager, Manager::MovedKey>);

}  // namespace
}  // namespace kiwi