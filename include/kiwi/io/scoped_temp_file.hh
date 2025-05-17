// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "kiwi/io/file_path.hh"
#include "kiwi/portability/base_export.hh"

namespace kiwi {

// An owned FilePath that's deleted when this object goes out of scope.
// Deletion is attempted on destruction, but is not guaranteed.
class BASE_EXPORT ScopedTempFile {
 public:
  // No file is owned/created initially.
  ScopedTempFile();

  ScopedTempFile(ScopedTempFile&&) noexcept;
  ScopedTempFile& operator=(ScopedTempFile&&) noexcept;

  ~ScopedTempFile();

  // The owned path must be empty before calling Create().
  // Returns true on success.
  [[nodiscard]] bool Create();

  // Returns true on success or if the file was never created.
  [[nodiscard]] bool Delete();

  // Attempts to delete the file.  The managed path is reset regardless of
  // if the deletion was successful.
  void Reset();

  [[nodiscard]] const kiwi::FilePath& Path() const { return path_; }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator bool() const { return !path_.empty(); }

 private:
  FilePath path_;
};

}  // namespace kiwi
