// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <queue>

#include "kiwi/containers/circular_deque.hh"

namespace kiwi {

/// Provides a definition of kiwi::queue that's like std::queue but uses a
/// kiwi::circular_deque instead of std::deque. Since std::queue is just a
/// wrapper for an underlying type, we can just provide a typedef for it that
/// defaults to the base circular_deque.
template <typename T, typename Container = circular_deque<T>>
using queue = std::queue<T, Container>;

}  // namespace kiwi
