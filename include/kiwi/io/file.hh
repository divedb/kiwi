// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_H_
#define BASE_FILES_FILE_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "kiwi/chrono/time.hh"
#include "kiwi/common/macros.hh"
#include "kiwi/containers/span.hh"
#include "kiwi/io/file_path.hh"
#include "kiwi/io/iovec.hh"
#include "kiwi/io/platform_file.hh"
#include "kiwi/portability/base_export.hh"
#include "kiwi/portability/build_config.hh"
#include "kiwi/portability/compiler_specific.hh"

struct stat;

namespace kiwi {

class IOBuf;
using stat_wrapper_t = struct stat;

/// Thin wrapper around an OS-level file.
/// Note that this class does not provide any support for asynchronous IO, other
/// than the ability to create asynchronous handles on Windows.
///
/// Note about const: this class does not attempt to determine if the underlying
/// file system object is affected by a particular method in order to consider
/// that method const or not. Only methods that deal with member variables in an
/// obvious non-modifying way are marked as const. Any method that forward calls
/// to the OS is not considered const, even if there is no apparent change to
/// member variables.
///
/// On POSIX, if the given file is a symbolic link, most of the methods apply to
/// the file that the symbolic link resolves to.
class BASE_EXPORT File {
  DISALLOW_COPY_AND_ASSIGN(File);

 public:
  enum class LockMode {
    kShared,
    kExclusive,
  };

  /// kFlag(Open|Create).* are mutually exclusive. You should specify exactly
  /// one of the five (possibly combining with other flags) when opening or
  /// creating a file. kFlag(Write|Append) are mutually exclusive. This is so
  /// that APPEND behavior will be consistent with O_APPEND on POSIX.
  enum Flags : uint32_t {
    /// Opens a file, only if it exists.
    kFlagOpen = 1 << 0,
    /// Creates a new file, only if it does not already exist.
    kFlagCreate = 1 << 1,
    /// May create a new file.
    kFlagOpenAlways = 1 << 2,
    /// May overwrite an old file.
    kFlagCreateAlways = 1 << 3,
    /// Opens a file and truncates it, only if it exists.
    kFlagOpenTruncated = 1 << 4,
    kFlagRead = 1 << 5,
    kFlagWrite = 1 << 6,
    kFlagAppend = 1 << 7,
    /// Windows only. Opposite of SHARE.
    kFlagWinExclusiveRead = 1 << 8,
    /// Windows only. Opposite of SHARE.
    kFlagWinExclusiveWrite = 1 << 9,
    kFlagAsync = 1 << 10,
    /// Windows only.
    kFlagWinTemporary = 1 << 11,
    /// Windows only.
    kFlagWinHidden = 1 << 12,
    kFlagDeleteOnClose = 1 << 13,
    /// Allows writing attributes, e.g., SetTimes().
    kFlagWriteAttributes = 1 << 14,
    /// Windows only.
    kFlagWinShareDelete = 1 << 15,
    /// Serial port flags.
    kFlagTerminalDevice = 1 << 16,
    /// Windows only.
    kFlagWinBackupSemantics = 1 << 17,
    /// Windows only.
    kFlagWinExecute = 1 << 18,
    /// Windows only.
    kFlagWinSequentialScan = 1 << 19,
    /// Requests permission to delete via DeleteOnClose().
    kFlagCanDeleteOnClose = 1 << 20,
    /// Windows only. Prevents EXECUTE access.
    kFlagWinNoExecute = 1 << 21,
  };

  /// This enum has been recorded in multiple histograms using PlatformFileError
  /// enum. If the order of the fields needs to change, please ensure that those
  /// histograms are obsolete or have been moved to a different enum.
  ///
  /// kFileErrorAccessDenied is returned when a call fails because of a
  /// filesystem restriction. FILE_ERROR_SECURITY is returned when a browser
  /// policy doesn't allow the operation to be executed.
  enum Error {
    kFileOk = 0,
    kFileErrorFailed = -1,
    kFileErrorInUse = -2,
    kFileErrorExists = -3,
    kFileErrorNotFound = -4,
    kFileErrorAccessDenied = -5,
    kFileErrorTooManyOpened = -6,
    kFileErrorNoMemory = -7,
    kFileErrorNoSpace = -8,
    kFileErrorNotADirectory = -9,
    kFileErrorInvalidOperation = -10,
    kFileErrorSecurity = -11,
    kFileErrorAbort = -12,
    kFileErrorNotAFile = -13,
    kFileErrorNotEmpty = -14,
    kFileErrorInvalidUrl = -15,
    kFileErrorIO = -16,
    /// Put new entries here and increment kFileErrorMax.
    kFileErrorMax = -17
  };

  /// This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
  enum Whence { kFromBegin = 0, kFromCurrent = 1, kFromEnd = 2 };

  /// Used to hold information about a given file.
  /// If you add more fields to this structure (platform-specific fields are
  /// OK), make sure to update all functions that use it in
  /// file_util_{win|posix}.cc, too.
  struct BASE_EXPORT Info {
#if BUILDFLAG(IS_POSIX)
    /// Fills this struct with values from `stat_info`.
    void FromStat(const stat_wrapper_t& stat_info);
#endif

    /// The size of the file in bytes.  Undefined when is_directory is true.
    int64_t size = 0;

    /// True if the file corresponds to a directory.
    bool is_directory = false;

    /// True if the file corresponds to a symbolic link.  For Windows currently
    /// not supported and thus always false.
    bool is_symbolic_link = false;

    /// The last modified time of a file.
    Time last_modified;

    /// The last accessed time of a file.
    Time last_accessed;

    /// The creation time of a file.
    Time creation_time;
  };

  File() = default;

  /// Creates or opens the given file. This will fail with 'access denied' if
  /// the `path` contains path traversal ('..') components.
  ///
  /// \param path The file path.
  /// \param flags Flags for opening or creating \see Flags.
  File(const FilePath& path, uint32_t flags);

  /// Takes ownership of `platform_file` and sets async to false.
  ///
  /// \copydoc File(ScopedPlatformFile platform_file, bool async);
  explicit File(ScopedPlatformFile platform_file);
  explicit File(PlatformFile platform_file);

  /// Takes ownership of `platform_file` and sets async to the given value.
  /// This constructor exists because on Windows you can't check if
  /// platform_file is async or not.
  ///
  /// \param platform_file A scoped handle to an already-opened
  ///                     platform-specific file.
  /// \param async If true, the file will be operated in asynchronous
  ///              (non-blocking) mode; otherwise, operations will be
  ///              synchronous.
  File(ScopedPlatformFile platform_file, bool async);
  File(PlatformFile platform_file, bool async);

  // Creates an object with a specific error_details code.
  explicit File(Error error_details);

  /// Move constructor.
  File(File&& other);

  ~File();

  File& operator=(File&& other);

  // Creates or opens the given file.
  void Initialize(const FilePath& path, uint32_t flags);

  /// \return true if the handle or fd wrapped by this object is valid.  This
  ///         method doesn't interact with the file system and is thus safe to
  ///         be called from threads that disallow blocking.
  bool IsValid() const;

  /// \return true if a new file was created (or an old one truncated to zero
  ///         length to simulate a new file, which can happen with
  ///         kFlagCreateAlways), and false otherwise.
  bool IsCreated() const { return is_created_; }

  /// Note that the way to verify the success of the operation is to use
  /// IsValid(), not this method:
  ///
  /// \code
  /// File file(path, flags);
  /// if (!file.IsValid())
  ///   return;
  /// \endcode
  ///
  /// \return The OS result of opening this file.
  Error ErrorDetails() const { return error_details_; }

  PlatformFile GetPlatformFile() const { return file_.get(); }
  PlatformFile TakePlatformFile() { return file_.Release(); }

  /// Destroying this object closes the file automatically.
  void Close();

  /// Changes current position in the file to an `offset` relative to an origin
  /// defined by `whence`.
  ///
  /// \param whence The reference point for the offset. \see Whence.
  /// \param offset The number of bytes to move the file pointer relative to
  ///               whence.
  /// \return The new offset from the beginning of the file, or -1 on error.
  int64_t Seek(Whence whence, int64_t offset);

  /// Simplified versions of Read() and friends (see below) that check the
  /// return value and just return a boolean.
  ///
  /// \param offset The byte offset in the file from which to start reading.
  /// \param data A span representing the buffer to store the read data.
  /// \return true iff the function read in exactly `data.size()` bytes of data.
  bool ReadAndCheck(int64_t offset, kiwi::span<uint8_t> data);
  bool ReadAtCurrentPosAndCheck(kiwi::span<uint8_t> data);

  /// Reads the given number of bytes (or until EOF is reached) starting with
  /// the given offset.
  ///
  /// Note that this function makes a best effort to read all data on all
  /// platforms, so it is not intended for stream oriented files but instead for
  /// cases when the normal expectation is that actually `size` bytes are read
  /// unless there is an error.
  ///
  /// \param offset The byte offset in the file from which to start reading.
  /// \param data A pointer to buffer.
  /// \param size The number of bytes to read.
  /// \return The number of bytes read, or -1 on error.
  UNSAFE_BUFFER_USAGE int Read(int64_t offset, char* data, int size);
  std::optional<size_t> Read(int64_t offset, kiwi::span<uint8_t> data);

  /// Same as above but without seek.
  ///
  /// \copydoc int Read(int64_t offset, char* data, int size);
  UNSAFE_BUFFER_USAGE int ReadAtCurrentPos(char* data, int size);
  std::optional<size_t> ReadAtCurrentPos(kiwi::span<uint8_t> data);

  /// Reads the given number of bytes (or until EOF is reached) starting with
  /// the given offset, but does not make any effort to read all data on all
  /// platforms. Returns the number of bytes read, or -1/std::nullopt on error.
  UNSAFE_BUFFER_USAGE int ReadNoBestEffort(int64_t offset, char* data,
                                           int size);
  std::optional<size_t> ReadNoBestEffort(int64_t offset,
                                         kiwi::span<uint8_t> data);

  /// Same as above but without seek.
  /// \copydoc nt ReadNoBestEffort(int64_t offset, char* data, int size);
  UNSAFE_BUFFER_USAGE int ReadAtCurrentPosNoBestEffort(char* data, int size);
  std::optional<size_t> ReadAtCurrentPosNoBestEffort(kiwi::span<uint8_t> data);

  /// Simplified versions of Write() and friends (see below) that check the
  /// return value and just return a boolean. They return true if and only if
  /// the function wrote out exactly |data.size()| bytes of data.
  bool WriteAndCheck(int64_t offset, span<const uint8_t> data);
  bool WriteAtCurrentPosAndCheck(span<const uint8_t> data);

  /// Writes the given buffer into the file at the given offset, overwritting
  /// any data that was previously there. Returns the number of bytes written,
  /// or -1 on error. Note that this function makes a best effort to write all
  /// data on all platforms. |data| can be nullptr when |size| is 0. Ignores the
  /// offset and writes to the end of the file if the file was opened with
  /// FLAG_APPEND.
  UNSAFE_BUFFER_USAGE int Write(int64_t offset, const char* data, int size);
  std::optional<size_t> Write(int64_t offset, kiwi::span<const uint8_t> data);

  /// Save as above but without seek.
  UNSAFE_BUFFER_USAGE int WriteAtCurrentPos(const char* data, int size);
  std::optional<size_t> WriteAtCurrentPos(kiwi::span<const uint8_t> data);
  ssize_t WriteIOBufAtCurrentPos(const kiwi::IOBuf& iobuf);

  /// Save as above but does not make any effort to write all data on all
  /// platforms. Returns the number of bytes written, or -1/std::nullopt
  /// on error.
  UNSAFE_BUFFER_USAGE int WriteAtCurrentPosNoBestEffort(const char* data,
                                                        int size);
  std::optional<size_t> WriteAtCurrentPosNoBestEffort(
      kiwi::span<const uint8_t> data);

  /// Returns the current size of this file, or a negative number on failure.
  int64_t GetLength() const;

  /// Truncates the file to the given length. If |length| is greater than the
  /// current size of the file, the file is extended with zeros. If the file
  /// doesn't exist, |false| is returned.
  bool Truncate(int64_t length);

  /// Instructs the filesystem to flush the file to disk. (POSIX: fsync,
  /// Windows: FlushFileBuffers). Calling Flush() does not guarantee file
  /// integrity and thus is not a valid substitute for file integrity checks and
  /// recovery codepaths for malformed files. It can also be *really* slow, so
  /// avoid blocking on Flush(), especially please don't block shutdown on
  /// Flush().
  ///
  /// Latency percentiles of Flush() across all platforms as of July 2016:
  ///
  /// 50 %     > 5 ms
  /// 10 %     > 58 ms
  ///  1 %     > 357 ms
  ///  0.1 %   > 1.8 seconds
  ///  0.01 %  > 7.6 seconds
  bool Flush();

  /// Updates the file times.
  bool SetTimes(Time last_access_time, Time last_modified_time);

  /// Returns some basic information for the given file.
  bool GetInfo(Info* info) const;

  // Attempts to take an exclusive write lock on the file. Returns immediately
  // (i.e. does not wait for another process to unlock the file). If the lock
  // was obtained, the result will be FILE_OK. A lock only guarantees
  // that other processes may not also take a lock on the same file with the
  // same API - it may still be opened, renamed, unlinked, etc.
  //
  // Common semantics:
  //  * Locks are held by processes, but not inherited by child processes.
  //  * Locks are released by the OS on file close or process termination.
  //  * Locks are reliable only on local filesystems.
  //  * Duplicated file handles may also write to locked files.
  // Windows-specific semantics:
  //  * Locks are mandatory for read/write APIs, advisory for mapping APIs.
  //  * Within a process, locking the same file (by the same or new handle)
  //    will fail.
  // POSIX-specific semantics:
  //  * Locks are advisory only.
  //  * Within a process, locking the same file (by the same or new handle)
  //    will succeed. The new lock replaces the old lock.
  //  * Closing any descriptor on a given file releases the lock.
  Error Lock(LockMode mode);

  // Unlock a file previously locked.
  Error Unlock();

  // Returns a new object referencing this file for use within the current
  // process. Handling of FLAG_DELETE_ON_CLOSE varies by OS. On POSIX, the File
  // object that was created or initialized with this flag will have unlinked
  // the underlying file when it was created or opened. On Windows, the
  // underlying file is deleted when the last handle to it is closed.
  File Duplicate() const;

  bool IsAsync() const { return is_async_; }

#if BUILDFLAG(IS_WIN)
  // Sets or clears the DeleteFile disposition on the file. Returns true if
  // the disposition was set or cleared, as indicated by |delete_on_close|.
  //
  // Microsoft Windows deletes a file only when the DeleteFile disposition is
  // set on a file when the last handle to the last underlying kernel File
  // object is closed. This disposition is be set by:
  // - Calling the Win32 DeleteFile function with the path to a file.
  // - Opening/creating a file with FLAG_DELETE_ON_CLOSE and then closing all
  //   handles to that File object.
  // - Opening/creating a file with FLAG_CAN_DELETE_ON_CLOSE and subsequently
  //   calling DeleteOnClose(true).
  //
  // In all cases, all pre-existing handles to the file must have been opened
  // with FLAG_WIN_SHARE_DELETE. Once the disposition has been set by any of the
  // above means, no new File objects can be created for the file.
  //
  // So:
  // - Use FLAG_WIN_SHARE_DELETE when creating/opening a file to allow another
  //   entity on the system to cause it to be deleted when it is closed. (Note:
  //   another entity can delete the file the moment after it is closed, so not
  //   using this permission doesn't provide any protections.)
  // - Use FLAG_DELETE_ON_CLOSE for any file that is to be deleted after use.
  //   The OS will ensure it is deleted even in the face of process termination.
  //   Note that it's possible for deletion to be cancelled via another File
  //   object referencing the same file using DeleteOnClose(false) to clear the
  //   DeleteFile disposition after the original File is closed.
  // - Use FLAG_CAN_DELETE_ON_CLOSE in conjunction with DeleteOnClose() to alter
  //   the DeleteFile disposition on an open handle. This fine-grained control
  //   allows for marking a file for deletion during processing so that it is
  //   deleted in the event of untimely process termination, and then clearing
  //   this state once the file is suitable for persistence.
  bool DeleteOnClose(bool delete_on_close);

  // Precondition: last_error is not 0, also known as ERROR_SUCCESS.
  static Error OSErrorToFileError(DWORD last_error);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  /// Precondition: saved_errno is not 0.
  static Error OSErrorToFileError(int saved_errno);
#endif

  /// Gets the last global error (errno or GetLastError()) and converts it to
  /// the closest base::File::Error equivalent via OSErrorToFileError(). It
  /// should therefore only be called immediately after another base::File
  /// method fails. base::File never resets the global error to zero.
  static Error GetLastFileError();

  /// Converts an error value to a human-readable form. Used for logging.
  static std::string ErrorToString(Error error);

#if BUILDFLAG(IS_POSIX)
  // Wrapper for stat().
  static int Stat(const FilePath& path, stat_wrapper_t* sb);
  // Wrapper for fstat().
  static int Fstat(int fd, stat_wrapper_t* sb);
  // Wrapper for lstat().
  static int Lstat(const FilePath& path, stat_wrapper_t* sb);
#endif

  /// This function can be used to augment `flags` with the correct flags
  /// required to create a File that can be safely passed to an untrusted
  /// process. It must be called if the File is intended to be transferred to an
  /// untrusted process, but can still be safely called even if the File is not
  /// intended to be transferred.
  static constexpr uint32_t AddFlagsForPassingToUntrustedProcess(
      uint32_t flags) {
    if (flags & File::kFlagWrite || flags & File::kFlagAppend ||
        flags & File::kFlagWriteAttributes) {
      flags |= File::kFlagWinNoExecute;
    }

    return flags;
  }

 private:
  // Creates or opens the given file. Only called if |path| has no
  // traversal ('..') components.
  void DoInitialize(const FilePath& path, uint32_t flags);
  void SetPlatformFile(PlatformFile file);

  ScopedPlatformFile file_;
  FilePath path_;
  Error error_details_ = kFileErrorFailed;
  bool is_created_ = false;
  bool is_async_ = false;
};

}  // namespace kiwi

#endif  // BASE_FILES_FILE_H_