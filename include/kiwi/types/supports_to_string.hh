// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace kiwi::internal {

template <typename T>
concept SupportsToString = requires(const T& t) { t.ToString(); };

}  // namespace kiwi::internal
