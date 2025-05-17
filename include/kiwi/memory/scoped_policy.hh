// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace kiwi {
namespace scoped_policy {

// Defines the ownership policy for a scoped object.
enum OwnershipPolicy {
  /// The scoped object takes ownership of an object by taking over an existing
  /// ownership claim.
  kAssume,

  /// The scoped object will retain the object and any initial ownership is not
  /// changed.
  kRetain
};

}  // namespace scoped_policy
}  // namespace kiwi
