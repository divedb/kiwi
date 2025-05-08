// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/io/file.hh"

#include <utility>

#include "kiwi/common/logging.hh"
#include "kiwi/io/file_path.hh"
#include "kiwi/numerics/safe_conversions.hh"
#include "kiwi/portability/build_config.hh"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#endif

namespace kiwi {

File::File(const FilePath& path, uint32_t flags) : error_details_(kFileOk) {
  Initialize(path, flags);
}

File::File(ScopedPlatformFile platform_file)
    : File(std::move(platform_file), false) {}

File::File(PlatformFile platform_file) : File(platform_file, false) {}

File::File(ScopedPlatformFile platform_file, bool async)
    : file_(std::move(platform_file)),
      error_details_(kFileOk),
      is_async_(async) {
#if BUILDFLAG(IS_POSIX)
  DCHECK_GE(file_.get(), -1);
#endif
}

File::File(PlatformFile platform_file, bool async)
    : file_(platform_file), error_details_(kFileOk), is_async_(async) {
#if BUILDFLAG(IS_POSIX)
  DCHECK_GE(platform_file, -1);
#endif
}

File::File(Error error_details) : error_details_(error_details) {}

File::File(File&& other)
    : file_(other.TakePlatformFile()),
      path_(other.path_),
      error_details_(other.ErrorDetails()),
      is_created_(other.IsCreated()),
      is_async_(other.is_async_) {}

File::~File() {
  // Go through the AssertIOAllowed logic.
  Close();
}

File& File::operator=(File&& other) {
  Close();
  SetPlatformFile(other.TakePlatformFile());
  path_ = other.path_;
  error_details_ = other.ErrorDetails();
  is_created_ = other.IsCreated();
  is_async_ = other.IsAsync();

  return *this;
}

void File::Initialize(const FilePath& path, uint32_t flags) {
  if (path.ReferencesParent()) {
#if BUILDFLAG(IS_WIN)
    ::SetLastError(kFileErrorAccessDenied);
#elif BUILDFLAG(IS_POSIX)
    errno = EACCES;
#else
#error Unsupported platform
#endif
    error_details_ = kFileErrorAccessDenied;
    return;
  }

  DoInitialize(path, flags);
}

std::optional<size_t> File::Read(int64_t offset, span<uint8_t> data) {
  kiwi::span<char> chars = kiwi::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(Read(offset, chars.data(), size));

  if (result < 0) {
    return std::nullopt;
  }

  return checked_cast<size_t>(result);
}

bool File::ReadAndCheck(int64_t offset, span<uint8_t> data) {
  // Size checked in span form of Read() above.
  return Read(offset, data) == static_cast<int>(data.size());
}

std::optional<size_t> File::ReadAtCurrentPos(span<uint8_t> data) {
  span<char> chars = kiwi::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadAtCurrentPos(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::ReadAtCurrentPosAndCheck(span<uint8_t> data) {
  // Size checked in span form of ReadAtCurrentPos() above.
  return ReadAtCurrentPos(data) == static_cast<int>(data.size());
}

std::optional<size_t> File::Write(int64_t offset, span<const uint8_t> data) {
  span<const char> chars = kiwi::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(Write(offset, chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::WriteAndCheck(int64_t offset, span<const uint8_t> data) {
  // Size checked in span form of Write() above.
  return Write(offset, data) == static_cast<int>(data.size());
}

std::optional<size_t> File::WriteAtCurrentPos(span<const uint8_t> data) {
  span<const char> chars = kiwi::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(WriteAtCurrentPos(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

bool File::WriteAtCurrentPosAndCheck(span<const uint8_t> data) {
  // Size checked in span form of WriteAtCurrentPos() above.
  return WriteAtCurrentPos(data) == static_cast<int>(data.size());
}

std::optional<size_t> File::ReadNoBestEffort(int64_t offset,
                                             kiwi::span<uint8_t> data) {
  span<char> chars = kiwi::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadNoBestEffort(offset, chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

std::optional<size_t> File::ReadAtCurrentPosNoBestEffort(
    kiwi::span<uint8_t> data) {
  span<char> chars = kiwi::as_writable_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result = UNSAFE_BUFFERS(ReadAtCurrentPosNoBestEffort(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }
  return checked_cast<size_t>(result);
}

std::optional<size_t> File::WriteAtCurrentPosNoBestEffort(
    kiwi::span<const uint8_t> data) {
  span<const char> chars = kiwi::as_chars(data);
  int size = checked_cast<int>(chars.size());
  // SAFETY: `chars.size()` describes valid portion of `chars.data()`.
  int result =
      UNSAFE_BUFFERS(WriteAtCurrentPosNoBestEffort(chars.data(), size));
  if (result < 0) {
    return std::nullopt;
  }

  return checked_cast<size_t>(result);
}

// static
std::string File::ErrorToString(Error error) {
  switch (error) {
    case kFileOk:
      return "FILE_OK";
    case kFileErrorFailed:
      return "FILE_ERROR_FAILED";
    case kFileErrorInUse:
      return "FILE_ERROR_IN_USE";
    case kFileErrorExists:
      return "FILE_ERROR_EXISTS";
    case kFileErrorNotFound:
      return "FILE_ERROR_NOT_FOUND";
    case kFileErrorAccessDenied:
      return "FILE_ERROR_ACCESS_DENIED";
    case kFileErrorTooManyOpened:
      return "FILE_ERROR_TOO_MANY_OPENED";
    case kFileErrorNoMemory:
      return "FILE_ERROR_NO_MEMORY";
    case kFileErrorNoSpace:
      return "FILE_ERROR_NO_SPACE";
    case kFileErrorNotADirectory:
      return "FILE_ERROR_NOT_A_DIRECTORY";
    case kFileErrorInvalidOperation:
      return "FILE_ERROR_INVALID_OPERATION";
    case kFileErrorSecurity:
      return "FILE_ERROR_SECURITY";
    case kFileErrorAbort:
      return "FILE_ERROR_ABORT";
    case kFileErrorNotAFile:
      return "FILE_ERROR_NOT_A_FILE";
    case kFileErrorNotEmpty:
      return "FILE_ERROR_NOT_EMPTY";
    case kFileErrorInvalidUrl:
      return "FILE_ERROR_INVALID_URL";
    case kFileErrorIO:
      return "FILE_ERROR_IO";
    case kFileErrorMax:
      break;
  }

  UNREACHABLE();
}

}  // namespace kiwi