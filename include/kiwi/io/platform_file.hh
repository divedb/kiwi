
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_PLATFORM_FILE_H_
#define BASE_FILES_PLATFORM_FILE_H_

#include "kiwi/io/scoped_file.hh"
#include "kiwi/portability/build_config.hh"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#endif

/// This file defines platform-independent types for dealing with
/// platform-dependent files. If possible, use the higher-level kiwi::File class
/// rather than these primitives.
namespace kiwi {

#if BUILDFLAG(IS_WIN)

using PlatformFile = HANDLE;
using ScopedPlatformFile = ::kiwi::win::ScopedHandle;

/// It would be nice to make this constexpr but INVALID_HANDLE_VALUE is a
/// ((void*)(-1)) which Clang rejects since reinterpret_cast is technically
/// disallowed in constexpr. Visual Studio accepts this, however.
const PlatformFile kInvalidPlatformFile = INVALID_HANDLE_VALUE;

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

using PlatformFile = int;
using ScopedPlatformFile = ::kiwi::ScopedFD;

inline constexpr PlatformFile kInvalidPlatformFile = -1;

#endif

}  // namespace kiwi

#endif  // BASE_FILES_PLATFORM_FILE_H_
