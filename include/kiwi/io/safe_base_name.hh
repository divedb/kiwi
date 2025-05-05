// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "kiwi/io/file_path.hh"
#include "kiwi/portability/base_export.hh"
#include "kiwi/portability/compiler_specific.hh"

namespace kiwi {

/// Represents the last path component of a FilePath object, either a file or a
/// directory. This type does not allow absolute paths or references to parent
/// directories and is considered safe to be passed over IPC.
/// \see FilePath::BaseName().
///
/// Usage example:
/// \code
/// auto a = (SafeBaseName::Create(FILE_PATH_LITERAL("file.txt")));
/// FilePath dir(FILE_PATH_LITERAL("foo"));
/// dir.Append(*a);
/// \endcode
class BASE_EXPORT SafeBaseName {
  /// Constructs a new SafeBaseName from the given FilePath.
  explicit SafeBaseName(const FilePath& path) : path_(path) {}

 public:
  /// Factory method that attempts to create a SafeBaseName from a FilePath.
  /// Performs validation to ensure the input FilePath represents a safe base
  /// name (not absolute, no parent references like '..', not ends with
  /// separator).
  ///
  /// \param path The FilePath to attempt to convert.
  /// \return A valid SafeBaseName wrapped in std::optional if validation
  ///         succeeds, otherwise returns std::nullopt.
  static std::optional<SafeBaseName> Create(const FilePath& path);

  // Same as above, but takes a StringViewType for convenience.
  static std::optional<SafeBaseName> Create(FilePath::StringViewType);

  /// Creates an empty SafeBaseName.
  SafeBaseName() = default;

  /// \return A const reference to the internal FilePath object.
  const FilePath& path() const KIWI_LIFETIME_BOUND { return path_; }

  /// Get the underlying path as a UTF8 std::string (potentially unsafe).
  ///
  /// Note: "Unsafe" in the name implies that this conversion might not handle
  /// all possible platform native path characters correctly if they are not
  /// valid UTF8.
  ///
  /// \return The path as a UTF8 string.
  const std::string AsUTF8Unsafe() const { return path_.AsUTF8Unsafe(); }

  /// \return A const reference to the internal platform-native string value.
  const FilePath::StringType& value() const KIWI_LIFETIME_BOUND {
    return path_.value();
  }

  /// \return True if the path is empty, otherwise false.
  [[nodiscard]] bool empty() const { return path_.empty(); }

  /// \param that The other SafeBaseName object to compare with.
  /// \return True if the underlying paths are equal, otherwise false.
  bool operator==(const SafeBaseName& that) const {
    return path_ == that.path_;
  }

  bool operator!=(const SafeBaseName& that) const { return !(*this == that); }

 private:
  FilePath path_;
};

}  // namespace kiwi
