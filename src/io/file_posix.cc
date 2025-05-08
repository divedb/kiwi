// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "kiwi/common/logging.hh"
#include "kiwi/io/file.hh"

// The only 32-bit platform that uses this file is Android. On Android APIs
// >= 21, this standard define is the right way to express that you want a
// 64-bit offset in struct stat, and the stat64 struct and functions aren't
// useful.
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

static_assert(sizeof(kiwi::stat_wrapper_t::st_size) >= 8);

#include <atomic>
#include <optional>

#include "kiwi/numerics/safe_conversions.hh"
#include "kiwi/portability/build_config.hh"
#include "kiwi/portability/compiler_specific.hh"
#include "kiwi/posix/eintr_wrapper.hh"
#include "kiwi/strings/utf_string_conversions.hh"

namespace kiwi {

// Make sure our Whence mappings match the system headers.
static_assert(File::kFromBegin == SEEK_SET && File::kFromCurrent == SEEK_CUR &&
                  File::kFromEnd == SEEK_END,
              "whence mapping must match the system headers");

namespace {

// NaCl doesn't provide the following system calls, so either simulate them or
// wrap them in order to minimize the number of #ifdef's in this file.
#if !BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)
bool IsOpenAppend(PlatformFile file) {
  return (fcntl(file, F_GETFL) & O_APPEND) != 0;
}

int CallFtruncate(PlatformFile file, int64_t length) {
#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  static_assert(sizeof(off_t) >= sizeof(int64_t),
                "off_t is not a 64-bit integer");
  return HANDLE_EINTR(ftruncate(file, length));
#else
  return HANDLE_EINTR(ftruncate64(file, length));
#endif
}

int CallFutimes(PlatformFile file, const struct timeval times[2]) {
#ifdef __USE_XOPEN2K8
  // futimens should be available, but futimes might not be
  // http://pubs.opengroup.org/onlinepubs/9699919799/

  timespec ts_times[2];
  ts_times[0].tv_sec = times[0].tv_sec;
  ts_times[0].tv_nsec = times[0].tv_usec * 1000;
  ts_times[1].tv_sec = times[1].tv_sec;
  ts_times[1].tv_nsec = times[1].tv_usec * 1000;

  return futimens(file, ts_times);
#else
  KIWI_PUSH_WARNING
  KIWI_CLANG_DISABLE_WARNING("-Wunguarded-availability")
  return futimes(file, times);
  KIWI_POP_WARNING
#endif
}

short FcntlFlockType(std::optional<File::LockMode> mode) {
  if (!mode.has_value()) {
    return F_UNLCK;
  }
  switch (mode.value()) {
    case File::LockMode::kShared:
      return F_RDLCK;
    case File::LockMode::kExclusive:
      return F_WRLCK;
  }

  UNREACHABLE();
}

File::Error CallFcntlFlock(PlatformFile file,
                           std::optional<File::LockMode> mode) {
  struct flock lock;
  lock.l_type = FcntlFlockType(std::move(mode));
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;  // Lock entire file.

  if (HANDLE_EINTR(fcntl(file, F_SETLK, &lock)) == -1) {
    return File::GetLastFileError();
  }

  return File::kFileOk;
}

#else   // BUILDFLAG(IS_NACL) && !BUILDFLAG(IS_AIX)

bool IsOpenAppend(PlatformFile file) {
  // NaCl doesn't implement fcntl. Since NaCl's write conforms to the POSIX
  // standard and always appends if the file is opened with O_APPEND, just
  // return false here.
  return false;
}

int CallFtruncate(PlatformFile file, int64_t length) {
  NOTIMPLEMENTED();  // NaCl doesn't implement ftruncate.
  return 0;
}

int CallFutimes(PlatformFile file, const struct timeval times[2]) {
  NOTIMPLEMENTED();  // NaCl doesn't implement futimes.
  return 0;
}

File::Error CallFcntlFlock(PlatformFile file,
                           std::optional<File::LockMode> mode) {
  NOTIMPLEMENTED();  // NaCl doesn't implement flock struct.
  return File::FILE_ERROR_INVALID_OPERATION;
}
#endif  // BUILDFLAG(IS_NACL)

}  // namespace

void File::Info::FromStat(const stat_wrapper_t& stat_info) {
  is_directory = S_ISDIR(stat_info.st_mode);
  is_symbolic_link = S_ISLNK(stat_info.st_mode);
  size = stat_info.st_size;

  // Get last modification time, last access time, and creation time from
  // |stat_info|.
  // Note: st_ctime is actually last status change time when the inode was last
  // updated, which happens on any metadata change. It is not the file's
  // creation time. However, other than on Mac & iOS where the actual file
  // creation time is included as st_birthtime, the rest of POSIX platforms have
  // no portable way to get the creation time.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  time_t last_modified_sec = stat_info.st_mtim.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtim.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atim.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atim.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctim.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctim.tv_nsec;
#elif BUILDFLAG(IS_ANDROID)
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = stat_info.st_mtime_nsec;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = stat_info.st_atime_nsec;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = stat_info.st_ctime_nsec;
#elif BUILDFLAG(IS_APPLE)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_birthtimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_birthtimespec.tv_nsec;
#elif BUILDFLAG(IS_BSD)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctimespec.tv_nsec;
#else
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = 0;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = 0;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = 0;
#endif

  last_modified =
      Time::FromTimeT(last_modified_sec) +
      Microseconds(last_modified_nsec / Time::kNanosecondsPerMicrosecond);

  last_accessed =
      Time::FromTimeT(last_accessed_sec) +
      Microseconds(last_accessed_nsec / Time::kNanosecondsPerMicrosecond);

  creation_time =
      Time::FromTimeT(creation_time_sec) +
      Microseconds(creation_time_nsec / Time::kNanosecondsPerMicrosecond);
}

bool File::IsValid() const { return file_.is_valid(); }

void File::Close() {
  if (!IsValid()) {
    return;
  }

  file_.Reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  DCHECK(IsValid());

  static_assert(sizeof(int64_t) == sizeof(off_t), "off_t must be 64 bits");

  return lseek(file_.get(), static_cast<off_t>(offset),
               static_cast<int>(whence));
}

int File::Read(int64_t offset, char* data, int size) {
  DCHECK(IsValid());

  if (size < 0 || !IsValueInRangeForNumericType<off_t>(offset + size - 1)) {
    return -1;
  }

  ssize_t nbytes;
  int bytes_read = 0;

  do {
    nbytes = HANDLE_EINTR(pread(file_.get(), data + bytes_read,
                                static_cast<size_t>(size - bytes_read),
                                static_cast<off_t>(offset + bytes_read)));
    // Upon reading end-of-file, zero is returned.  Otherwise, a -1 is returned
    // the global variable errno is set to indicate the error.
    if (nbytes <= 0) {
      break;
    }

    bytes_read += nbytes;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(nbytes);
}

int File::ReadAtCurrentPos(char* data, int size) {
  DCHECK(IsValid());

  if (size < 0) {
    return -1;
  }

  int bytes_read = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(read(file_.get(), data + bytes_read,
                           static_cast<size_t>(size - bytes_read)));
    if (rv <= 0) {
      break;
    }

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : checked_cast<int>(rv);
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  DCHECK(IsValid());

  if (size < 0 || !IsValueInRangeForNumericType<off_t>(offset)) {
    return -1;
  }

  return checked_cast<int>(
      HANDLE_EINTR(pread(file_.get(), data, static_cast<size_t>(size),
                         static_cast<off_t>(offset))));
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  DCHECK(IsValid());

  if (size < 0) {
    return -1;
  }

  return checked_cast<int>(
      HANDLE_EINTR(read(file_.get(), data, static_cast<size_t>(size))));
}

int File::Write(int64_t offset, const char* data, int size) {
  if (IsOpenAppend(file_.get())) {
    return WriteAtCurrentPos(data, size);
  }

  DCHECK(IsValid());

  if (size < 0) {
    return -1;
  }

  int bytes_written = 0;
  long rv;
  do {
#if BUILDFLAG(IS_ANDROID)
    // In case __USE_FILE_OFFSET64 is not used, we need to call pwrite64()
    // instead of pwrite().
    static_assert(sizeof(int64_t) == sizeof(off64_t),
                  "off64_t must be 64 bits");
    rv = HANDLE_EINTR(pwrite64(file_.get(), data + bytes_written,
                               static_cast<size_t>(size - bytes_written),
                               offset + bytes_written));
#else
    rv = HANDLE_EINTR(pwrite(file_.get(), data + bytes_written,
                             static_cast<size_t>(size - bytes_written),
                             offset + bytes_written));
#endif
    if (rv <= 0) {
      break;
    }

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPos(const char* data, int size) {
  DCHECK(IsValid());

  if (size < 0) {
    return -1;
  }

  int bytes_written = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(write(file_.get(), data + bytes_written,
                            static_cast<size_t>(size - bytes_written)));
    if (rv <= 0) {
      break;
    }

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  DCHECK(IsValid());

  if (size < 0) {
    return -1;
  }

  return checked_cast<int>(
      HANDLE_EINTR(write(file_.get(), data, static_cast<size_t>(size))));
}

int64_t File::GetLength() const {
  DCHECK(IsValid());

  Info info;

  if (!GetInfo(&info)) {
    return -1;
  }

  return info.size;
}

bool File::Truncate(int64_t length) {
  assert(IsValid());

  return !CallFtruncate(file_.get(), length);
}

bool File::SetTimes(Time last_access_time, Time last_modified_time) {
  assert(IsValid());

  timeval times[2];
  times[0] = last_access_time.ToTimeVal();
  times[1] = last_modified_time.ToTimeVal();

  return !CallFutimes(file_.get(), times);
}

bool File::GetInfo(Info* info) const {
  DCHECK(IsValid());

  stat_wrapper_t file_info;
  bool success = (Fstat(file_.get(), &file_info) == 0);

  if (success) {
    info->FromStat(file_info);
  }

  return success;
}

File::Error File::Lock(File::LockMode mode) {
  return CallFcntlFlock(file_.get(), mode);
}

File::Error File::Unlock() {
  return CallFcntlFlock(file_.get(), std::optional<File::LockMode>());
}

File File::Duplicate() const {
  if (!IsValid()) {
    return File();
  }

  ScopedPlatformFile other_fd(HANDLE_EINTR(dup(GetPlatformFile())));

  if (!other_fd.is_valid()) {
    return File(File::GetLastFileError());
  }

  return File(std::move(other_fd), IsAsync());
}

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return kFileErrorAccessDenied;
    case EBUSY:
#if !BUILDFLAG(IS_NACL)  // ETXTBSY not defined by NaCl.
    case ETXTBSY:
#endif
      return kFileErrorInUse;
    case EEXIST:
      return kFileErrorExists;
    case EIO:
      return kFileErrorIO;
    case ENOENT:
      return kFileErrorNotFound;
    case ENFILE:
      [[fallthrough]];
    case EMFILE:
      return kFileErrorTooManyOpened;
    case ENOMEM:
      return kFileErrorNoMemory;
    case ENOSPC:
      return kFileErrorNoSpace;
    case ENOTDIR:
      return kFileErrorNotADirectory;
    default:
      // This function should only be called for errors.
      assert(saved_errno != 0);

      return kFileErrorFailed;
  }
}

void File::DoInitialize(const FilePath& path, uint32_t flags) {
  static_assert(O_RDONLY == 0, "O_RDONLY must equal zero");
  DCHECK(!IsValid());

  int open_flags = 0;
  is_created_ = false;

  // If the kFlagCreate is set, attempt to create a new file exclusively.
  if (flags & kFlagCreate) {
    open_flags = O_CREAT | O_EXCL;
  }

  // This flag is mutually exclusive with kFlagCreate and kFlagOpenTruncated and
  // it also requires write access.
  if (flags & kFlagCreateAlways) {
    DCHECK(!open_flags);
    DCHECK(flags & kFlagWrite);

    open_flags = O_CREAT | O_TRUNC;
  }

  // If the kFlagOpenTruncated is set, open an existing file and truncate it to
  // zero length. This flag is mutually exclusive with creation flags. It also
  // requires write access.
  if (flags & kFlagOpenTruncated) {
    DCHECK(!open_flags);
    DCHECK(flags & kFlagWrite);

    open_flags = O_TRUNC;
  }

  // If no specific creation/truncation flags were set, ensure at least one of
  // kFlagOpen or kFlagOpenAlways is set to indicate that an existing file
  // should be opened.
  if (!open_flags && !(flags & kFlagOpen) && !(flags & kFlagOpenAlways)) {
    UNREACHABLE();
  }

  if (flags & kFlagWrite && flags & kFlagRead) {
    open_flags |= O_RDWR;
  } else if (flags & kFlagWrite) {
    open_flags |= O_WRONLY;
  } else if (!(flags & kFlagRead) && !(flags & kFlagWriteAttributes) &&
             !(flags & kFlagAppend) && !(flags & kFlagOpenAlways)) {
    // Note: For FLAG_WRITE_ATTRIBUTES and no other read/write flags, we'll
    // open the file in O_RDONLY mode (== 0, see static_assert below), so that
    // we get a fd that can be used for SetTimes().
    UNREACHABLE();
  }

  if (flags & kFlagTerminalDevice) {
    open_flags |= O_NOCTTY | O_NDELAY;
  }

  if (flags & kFlagAppend && flags & kFlagRead) {
    open_flags |= O_APPEND | O_RDWR;
  } else if (flags & kFlagAppend) {
    open_flags |= O_APPEND | O_WRONLY;
  }

  mode_t mode = S_IRUSR | S_IWUSR;

  int descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));

  if (flags & kFlagOpenAlways) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));

      if (descriptor >= 0) {
        is_created_ = true;
      }
    }
  }

  if (descriptor < 0) {
    error_details_ = File::GetLastFileError();
    return;
  }

  if (flags & (kFlagCreateAlways | kFlagCreate)) {
    is_created_ = true;
  }

  // If kFlagDeleteOnClose is set, immediately unlink (delete) the file from the
  // filesystem. The file will remain accessible through the open descriptor but
  // will be removed when the last descriptor is close.
  if (flags & kFlagDeleteOnClose) {
    unlink(path.value().c_str());
  }

  is_async_ = ((flags & kFlagAsync) == kFlagAsync);
  error_details_ = kFileOk;
  file_.Reset(descriptor);
}

bool File::Flush() {
  DCHECK(IsValid());

#if BUILDFLAG(IS_NACL)
  NOTIMPLEMENTED();  // NaCl doesn't implement fsync.
  return true;
#elif BUILDFLAG(IS_LINUX)
  return !HANDLE_EINTR(fdatasync(file_.get()));
#elif BUILDFLAG(IS_APPLE)
  // On macOS and iOS, fsync() is guaranteed to send the file's data to the
  // underlying storage device, but may return before the device actually writes
  // the data to the medium. When used by database systems, this may result in
  // unexpected data loss. Depending on experiment state, this function may use
  // F_BARRIERFSYNC or F_FULLFSYNC to provide stronger guarantees than fsync().
  //
  // See documentation:
  // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html
  //
  // "relaxed" because there is no dependency between this memory operation and
  // other memory operations.
  if (!HANDLE_EINTR(fcntl(file_.get(), F_FULLFSYNC))) {
    return true;
  }

  // `fsync()` if `F_BARRIERFSYNC` or `F_FULLFSYNC` failed, or if the mechanism
  // is `kFlush`. Some file systems do not support `F_FULLFSYNC` /
  // `F_BARRIERFSYNC` but we cannot use the error code as a definitive indicator
  // that it's the case, so we'll keep trying `F_FULLFSYNC` / `F_BARRIERFSYNC`
  // for every call to this method when it's the case. See the CL description at
  // https://crrev.com/c/1400159 for details.
  return !HANDLE_EINTR(fsync(file_.get()));
#else
  return !HANDLE_EINTR(fsync(file_.get()));
#endif
}

void File::SetPlatformFile(PlatformFile file) {
  DCHECK(!file_.is_valid());

  file_.Reset(file);
}

// static
File::Error File::GetLastFileError() {
  return kiwi::File::OSErrorToFileError(errno);
}

int File::Stat(const FilePath& path, stat_wrapper_t* sb) {
  return stat(path.value().c_str(), sb);
}

int File::Fstat(int fd, stat_wrapper_t* sb) { return fstat(fd, sb); }

int File::Lstat(const FilePath& path, stat_wrapper_t* sb) {
  return lstat(path.value().c_str(), sb);
}

}  // namespace kiwi