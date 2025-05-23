// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/io/file_enumerator.hh"

#include "kiwi/common/function.hh"
#include "kiwi/io/file_util.hh"

namespace kiwi {

FileEnumerator::FileInfo::~FileInfo() = default;

FileEnumerator::FileInfo::FileInfo(const FileEnumerator::FileInfo&) = default;

FileEnumerator::FileInfo::FileInfo(FileEnumerator::FileInfo&&) = default;

FileEnumerator::FileInfo& FileEnumerator::FileInfo::operator=(
    const FileEnumerator::FileInfo& that) = default;

FileEnumerator::FileInfo& FileEnumerator::FileInfo::operator=(
    FileEnumerator::FileInfo&& that) = default;

bool FileEnumerator::ShouldSkip(const FilePath& path) {
  FilePath base_path = path.BaseName();
  const FilePath::StringType& basename = base_path.value();
  return basename == FILE_PATH_LITERAL(".") ||
         (basename == FILE_PATH_LITERAL("..") &&
          !(kIncludeDotDot & file_type_));
}

bool FileEnumerator::IsTypeMatched(bool is_dir) const {
  return (file_type_ & (is_dir ? FileEnumerator::kDirectories
                               : FileEnumerator::kFiles)) != 0;
}

void FileEnumerator::ForEach(FunctionRef<void(const FilePath& path)> ref) {
  for (FilePath name = Next(); !name.empty(); name = Next()) {
    ref(name);
  }
}

}  // namespace kiwi