// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/io/scoped_temp_file.hh"

#include <utility>

#include "kiwi/common/logging.hh"
#include "kiwi/io/file_util.hh"

namespace kiwi {

ScopedTempFile::ScopedTempFile() = default;

ScopedTempFile::ScopedTempFile(ScopedTempFile&& other) noexcept
    : path_(std::move(other.path_)) {}

ScopedTempFile& ScopedTempFile::operator=(ScopedTempFile&& other) noexcept {
  if (!path_.empty()) {
    CHECK_NE(path_, other.path_);
  }
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp file in operator=().";
  }
  path_ = std::move(other.path_);

  return *this;
}

ScopedTempFile::~ScopedTempFile() {
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp file in destructor.";
  }
}

bool ScopedTempFile::Create() {
  CHECK(path_.empty());
  return kiwi::CreateTemporaryFile(&path_);
}

bool ScopedTempFile::Delete() {
  if (path_.empty()) {
    return true;
  }
  if (DeleteFile(path_)) {
    path_.clear();
    return true;
  }
  return false;
}

void ScopedTempFile::Reset() {
  if (!Delete()) {
    DLOG(WARNING) << "Could not delete temp file in Reset().";
  }
  path_.clear();
}

}  // namespace kiwi