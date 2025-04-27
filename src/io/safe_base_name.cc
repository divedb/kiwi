// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/io/safe_base_name.hh"

namespace kiwi {

// static
std::optional<SafeBaseName> SafeBaseName::Create(const FilePath& path) {
  auto basename = path.BaseName();

  if (!basename.IsAbsolute() && !basename.ReferencesParent() &&
      !basename.EndsWithSeparator()) {
    return std::make_optional(SafeBaseName(basename));
  }

  return std::nullopt;
}

// static
std::optional<SafeBaseName> SafeBaseName::Create(
    FilePath::StringViewType path) {
  return Create(FilePath(path));
}

}  // namespace kiwi