// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdint.h>
#include <string.h>

#include "kiwi/common/logging.hh"
#include "kiwi/io/file_enumerator.hh"
#include "kiwi/portability/build_config.hh"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace kiwi {
namespace {

bool GetStat(const FilePath& path, bool show_links, stat_wrapper_t* st) {
  DCHECK(st);
  const int res = show_links ? File::Lstat(path, st) : File::Stat(path, st);
  if (res < 0) {
    // Print the stat() error message unless it was ENOENT and we're following
    // symlinks.
    LOG_IF(ERROR, errno != ENOENT || show_links)
        << "Cannot stat '" << path << "'";
    memset(st, 0, sizeof(*st));
    return false;
  }

  return true;
}

#if BUILDFLAG(IS_FUCHSIA)
bool ShouldShowSymLinks(int file_type) { return false; }
#else
bool ShouldShowSymLinks(int file_type) {
  return file_type & FileEnumerator::kShowSymLinks;
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_FUCHSIA)
bool ShouldTrackVisitedDirectories(int file_type) { return false; }
#else
bool ShouldTrackVisitedDirectories(int file_type) {
  return !(file_type & FileEnumerator::kShowSymLinks);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

}  // namespace

// FileEnumerator::FileInfo ----------------------------------------------------

FileEnumerator::FileInfo::FileInfo() { memset(&stat_, 0, sizeof(stat_)); }

#if BUILDFLAG(IS_ANDROID)
FileEnumerator::FileInfo::FileInfo(kiwi::FilePath content_uri,
                                   kiwi::FilePath filename, bool is_directory,
                                   off_t size, Time time)
    : content_uri_(std::move(content_uri)), filename_(std::move(filename)) {
  memset(&stat_, 0, sizeof(stat_));
  stat_.st_mode = is_directory ? S_IFDIR : S_IFREG;
  stat_.st_size = size;
  stat_.st_mtime = time.ToTimeT();
}
#endif

bool FileEnumerator::FileInfo::IsDirectory() const {
  return S_ISDIR(stat_.st_mode);
}

FilePath FileEnumerator::FileInfo::GetName() const { return filename_; }

int64_t FileEnumerator::FileInfo::GetSize() const { return stat_.st_size; }

kiwi::Time FileEnumerator::FileInfo::GetLastModifiedTime() const {
  return kiwi::Time::FromTimeT(stat_.st_mtime);
}

// FileEnumerator --------------------------------------------------------------

FileEnumerator::FileEnumerator(const FilePath& root_path, bool recursive,
                               int file_type)
    : FileEnumerator(root_path, recursive, file_type, FilePath::StringType(),
                     FolderSearchPolicy::MATCH_ONLY) {}

FileEnumerator::FileEnumerator(const FilePath& root_path, bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern)
    : FileEnumerator(root_path, recursive, file_type, pattern,
                     FolderSearchPolicy::MATCH_ONLY) {}

FileEnumerator::FileEnumerator(const FilePath& root_path, bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern,
                               FolderSearchPolicy folder_search_policy)
    : FileEnumerator(root_path, recursive, file_type, pattern,
                     folder_search_policy, ErrorPolicy::IGNORE_ERRORS) {}

FileEnumerator::FileEnumerator(const FilePath& root_path, bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern,
                               FolderSearchPolicy folder_search_policy,
                               ErrorPolicy error_policy)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(pattern),
      folder_search_policy_(folder_search_policy),
      error_policy_(error_policy) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (kIncludeDotDot & file_type_)));

#if BUILDFLAG(IS_ANDROID)
  // Content-URIs have limited support.
  if (root_path.IsContentUri()) {
    CHECK_EQ(file_type_, FileType::FILES | FileType::DIRECTORIES);
    // Get display-name of root path.
    FileInfo root_info;
    internal::ContentUriGetFileInfo(root_path, &root_info);
    pending_subdirs_.push(
        std::vector<std::string>({root_info.GetName().value()}));
  }
#endif

  if (file_type_ & FileType::kNamesOnly) {
    DCHECK(!recursive_);
    DCHECK_EQ(file_type_ & ~(FileType::kNamesOnly | FileType::kIncludeDotDot),
              0);
    file_type_ |= (FileType::kFiles | FileType::kDirectories);
  }

  if (recursive && ShouldTrackVisitedDirectories(file_type_)) {
    if (stat_wrapper_t st; GetStat(root_path, false, &st)) {
      MarkVisited(st);
    }
  }

  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() = default;

FilePath FileEnumerator::Next() {
  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty()) {
      return FilePath();
    }

    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();
    pending_paths_.pop();

#if BUILDFLAG(IS_ANDROID)
    if (root_path_.IsContentUri()) {
      subdirs_ = pending_subdirs_.top();
      pending_subdirs_.pop();
      directory_entries_ = internal::ListContentUriDirectory(root_path_);
      current_directory_entry_ = 0;
      if (directory_entries_.empty()) {
        continue;
      }
      if (recursive_) {
        for (auto& info : directory_entries_) {
          info.subdirs_ = subdirs_;
          if (info.IsDirectory()) {
            pending_paths_.push(info.content_uri_);
            pending_subdirs_.push(subdirs_);
            pending_subdirs_.top().push_back(info.GetName().value());
          }
        }
      }
      break;
    }
#endif

    DIR* dir = opendir(root_path_.value().c_str());
    if (!dir) {
      if (errno == 0 || error_policy_ == ErrorPolicy::IGNORE_ERRORS) {
        continue;
      }
      error_ = File::OSErrorToFileError(errno);
      return FilePath();
    }

    directory_entries_.clear();

#if BUILDFLAG(IS_FUCHSIA)
    // Fuchsia does not support .. on the file system server side, see
    // https://fuchsia.googlesource.com/docs/+/master/dotdot.md and
    // https://crbug.com/735540. However, for UI purposes, having the parent
    // directory show up in directory listings makes sense, so we add it here to
    // match the expectation on other operating systems. In cases where this
    // is useful it should be resolvable locally.
    FileInfo dotdot;
    dotdot.stat_.st_mode = S_IFDIR;
    dotdot.filename_ = FilePath("..");
    if (!ShouldSkip(dotdot.filename_)) {
      directory_entries_.push_back(std::move(dotdot));
    }
#endif  // BUILDFLAG(IS_FUCHSIA)

    current_directory_entry_ = 0;
    struct dirent* dent;
    // NOTE: Per the readdir() documentation, when the end of the directory is
    // reached with no errors, null is returned and errno is not changed.
    // Therefore we must reset errno to zero before calling readdir() if we
    // wish to know whether a null result indicates an error condition.
    while (errno = 0, dent = readdir(dir)) {
      FileInfo info;
      info.filename_ = FilePath(dent->d_name);

      if (ShouldSkip(info.filename_)) {
        continue;
      }

      const bool is_pattern_matched = IsPatternMatched(info.filename_);

      // MATCH_ONLY policy enumerates files and directories which matching
      // pattern only. So we can early skip further checks.
      if (folder_search_policy_ == FolderSearchPolicy::MATCH_ONLY &&
          !is_pattern_matched) {
        continue;
      }

      // Do not call OS stat/lstat if there is no sense to do it. If pattern is
      // not matched (file will not appear in results) and search is not
      // recursive (possible directory will not be added to pending paths) -
      // there is no sense to obtain item below.
      if (!recursive_ && !is_pattern_matched) {
        continue;
      }

      // If the caller only wants the names of files and directories, then
      // continue without populating `info` further.
      if (file_type_ & FileType::kNamesOnly) {
        directory_entries_.push_back(std::move(info));
        continue;
      }

      FilePath full_path = root_path_.Append(info.filename_);
      if (!GetStat(full_path, ShouldShowSymLinks(file_type_), &info.stat_)) {
        continue;
      }

      const bool is_dir = info.IsDirectory();

      // Recursive mode: schedule traversal of a directory if either
      // SHOW_SYM_LINKS is on or we haven't visited the directory yet.
      if (recursive_ && is_dir &&
          (!ShouldTrackVisitedDirectories(file_type_) ||
           MarkVisited(info.stat_))) {
        pending_paths_.push(std::move(full_path));
      }

      if (is_pattern_matched && IsTypeMatched(is_dir)) {
        directory_entries_.push_back(std::move(info));
      }
    }
    int readdir_errno = errno;
    closedir(dir);
    if (readdir_errno != 0 && error_policy_ != ErrorPolicy::IGNORE_ERRORS) {
      error_ = File::OSErrorToFileError(readdir_errno);
      return FilePath();
    }

    // MATCH_ONLY policy enumerates files in matched subfolders by "*" pattern.
    // ALL policy enumerates files in all subfolders by origin pattern.
    if (folder_search_policy_ == FolderSearchPolicy::MATCH_ONLY) {
      pattern_.clear();
    }
  }

#if BUILDFLAG(IS_ANDROID)
  if (root_path_.IsContentUri()) {
    return directory_entries_[current_directory_entry_].content_uri_;
  }
#endif

  return root_path_.Append(
      directory_entries_[current_directory_entry_].filename_);
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const {
  DCHECK(!(file_type_ & FileType::kNamesOnly));
  return directory_entries_[current_directory_entry_];
}

bool FileEnumerator::IsPatternMatched(const FilePath& path) const {
  return pattern_.empty() ||
         !fnmatch(pattern_.c_str(), path.value().c_str(), FNM_NOESCAPE);
}

}  // namespace kiwi