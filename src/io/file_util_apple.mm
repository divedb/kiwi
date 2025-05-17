// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <copyfile.h>
#include <stdlib.h>
#include <string.h>

#import <Foundation/Foundation.h>
#include "kiwi/apple/foundation_util.hh"
#include "kiwi/io/file_path.hh"
#include "kiwi/io/file_util.hh"
#include "kiwi/strings/string_util.hh"

namespace kiwi {

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent()) {
    return false;
  }
  return (copyfile(from_path.value().c_str(), to_path.value().c_str(),
                   /*state=*/nullptr, COPYFILE_DATA) == 0);
}

bool GetTempDir(kiwi::FilePath* path) {
  // In order to facilitate hermetic runs on macOS, first check
  // MAC_CHROMIUM_TMPDIR. This is used instead of TMPDIR for historical reasons.
  // This was originally done for https://crbug.com/698759 (TMPDIR too long for
  // process singleton socket path), but is hopefully obsolete as of
  // https://crbug.com/1266817 (allows a longer process singleton socket path).
  // Continue tracking MAC_CHROMIUM_TMPDIR as that's what build infrastructure
  // sets on macOS.
  const char* env_tmpdir = getenv("MAC_CHROMIUM_TMPDIR");
  if (env_tmpdir) {
    *path = kiwi::FilePath(env_tmpdir);
    return true;
  }

  // If we didn't find it, fall back to the native function.
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil) {
    return false;
  }
  *path = kiwi::apple::NSStringToFilePath(tmp);
  return true;
}

FilePath GetHomeDir() {
  NSString* tmp = NSHomeDirectory();
  if (tmp != nil) {
    FilePath mac_home_dir = kiwi::apple::NSStringToFilePath(tmp);
    if (!mac_home_dir.empty()) {
      return mac_home_dir;
    }
  }

  // Fall back on temp dir if no home directory is defined.
  FilePath rv;
  if (GetTempDir(&rv)) {
    return rv;
  }

  // Last resort.
  return FilePath("/tmp");
}

}  // namespace kiwi