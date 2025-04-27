// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "kiwi/containers/span.hh"
#include "kiwi/portability/base_export.hh"

namespace kiwi {

/// This provides a simple, fast CRC-32 calculation that can be used for
/// checking the integrity of data.  It is not a "secure" calculation!
/// \p sum can start with any seed or be used to continue an operation began
/// with previous data.
BASE_EXPORT uint32_t Crc32(uint32_t sum, span<const uint8_t> data);

}  // namespace kiwi
