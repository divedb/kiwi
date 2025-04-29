// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FilePath is a container for pathnames stored in a platform's native string
// type, providing containers for manipulation in according with the
// platform's conventions for pathnames.  It supports the following path
// types:
//
//                   POSIX            Windows
//                   ---------------  ----------------------------------
// Fundamental type  char[]           wchar_t[]
// Encoding          unspecified*     UTF-16
// Separator         /                \, tolerant of /
// Drive letters     no               case-insensitive A-Z followed by :
// Alternate root    // (surprise!)   \\ (2 Separators), for UNC paths
//
// * The encoding need not be specified on POSIX systems, although some
//   POSIX-compliant systems do specify an encoding.  Mac OS X uses UTF-8.
//   Chrome OS also uses UTF-8.
//   Linux does not specify an encoding, but in practice, the locale's
//   character set may be used.
//
// For more arcane bits of path trivia, see below.
//
// FilePath objects are intended to be used anywhere paths are.  An
// application may pass FilePath objects around internally, masking the
// underlying differences between systems, only differing in implementation
// where interfacing directly with the system.  For example, a single
// OpenFile(const FilePath &) function may be made available, allowing all
// callers to operate without regard to the underlying implementation.  On
// POSIX-like platforms, OpenFile might wrap fopen, and on Windows, it might
// wrap _wfopen_s, perhaps both by calling file_path.value().c_str().  This
// allows each platform to pass pathnames around without requiring conversions
// between encodings, which has an impact on performance, but more imporantly,
// has an impact on correctness on platforms that do not have well-defined
// encodings for pathnames.
//
// Several methods are available to perform common operations on a FilePath
// object, such as determining the parent directory (DirName), isolating the
// final path component (BaseName), and appending a relative pathname string
// to an existing FilePath object (Append).  These methods are highly
// recommended over attempting to split and concatenate strings directly.
// These methods are based purely on string manipulation and knowledge of
// platform-specific pathname conventions, and do not consult the filesystem
// at all, making them safe to use without fear of blocking on I/O operations.
// These methods do not function as mutators but instead return distinct
// instances of FilePath objects, and are therefore safe to use on const
// objects.  The objects themselves are safe to share between threads.
//
// To aid in initialization of FilePath objects from string literals, a
// FILE_PATH_LITERAL macro is provided, which accounts for the difference
// between char[]-based pathnames on POSIX systems and wchar_t[]-based
// pathnames on Windows.
//
// As a precaution against premature truncation, paths can't contain NULs.
//
// Because a FilePath object should not be instantiated at the global scope,
// instead, use a FilePath::CharType[] and initialize it with
// FILE_PATH_LITERAL.  At runtime, a FilePath object can be created from the
// character array.  Example:
//
// | const FilePath::CharType kLogFileName[] = FILE_PATH_LITERAL("log.txt");
// |
// | void Function() {
// |   FilePath log_file_path(kLogFileName);
// |   [...]
// | }
//
// WARNING: FilePaths should ALWAYS be displayed with LTR directionality, even
// when the UI language is RTL. This means you always need to pass filepaths
// through base::i18n::WrapPathWithLTRFormatting() before displaying it in the
// RTL UI.
//
// This is a very common source of bugs, please try to keep this in mind.
//
// ARCANE BITS OF PATH TRIVIA
//
//  - A double leading slash is actually part of the POSIX standard.  Systems
//    are allowed to treat // as an alternate root, as Windows does for UNC
//    (network share) paths.  Most POSIX systems don't do anything special
//    with two leading slashes, but FilePath handles this case properly
//    in case it ever comes across such a system.  FilePath needs this support
//    for Windows UNC paths, anyway.
//    References:
//    The Open Group Base Specifications Issue 7, sections 3.267 ("Pathname")
//    and 4.12 ("Pathname Resolution"), available at:
//    http://www.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
//    http://www.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_12
//
//  - Windows treats c:\\ the same way it treats \\.  This was intended to
//    allow older applications that require drive letters to support UNC paths
//    like \\server\share\path, by permitting c:\\server\share\path as an
//    equivalent.  Since the OS treats these paths specially, FilePath needs
//    to do the same.  Since Windows can use either / or \ as the separator,
//    FilePath treats c://, c:\\, //, and \\ all equivalently.
//    Reference:
//    The Old New Thing, "Why is a drive letter permitted in front of UNC
//    paths (sometimes)?", available at:
//    http://blogs.msdn.com/oldnewthing/archive/2005/11/22/495740.aspx

#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "kiwi/portability/base_export.hh"
#include "kiwi/portability/build_config.hh"
#include "kiwi/portability/compiler_specific.hh"

// Windows-style drive letter support and pathname separator characters can be
// enabled and disabled independently, to aid testing.  These #defines are
// here so that the same setting can be used in both the implementation and
// in the unit test.
#if BUILDFLAG(IS_WIN)
#define FILE_PATH_USES_DRIVE_LETTERS
#define FILE_PATH_USES_WIN_SEPARATORS
#endif  // BUILDFLAG(IS_WIN)

// To print path names portably use PR_FILE_PATH (based on PRIuS and friends
// from C99 and format_macros.h) like this: base::StringPrintf("Path is %"
// PR_FILE_PATH ".\n", path.value().c_str());
#if BUILDFLAG(IS_WIN)
#define PR_FILE_PATH "ls"
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define PR_FILE_PATH "s"
#endif  // BUILDFLAG(IS_WIN)

// Macros for string literal initialization of FilePath::CharType[].
#if BUILDFLAG(IS_WIN)

// The `FILE_PATH_LITERAL_INTERNAL` indirection allows `FILE_PATH_LITERAL` to
// work correctly with macro parameters, for example
// `FILE_PATH_LITERAL(TEST_FILE)` where `TEST_FILE` is a macro #defined as
// "TestFile".
#define FILE_PATH_LITERAL_INTERNAL(x) L##x
#define FILE_PATH_LITERAL(x) FILE_PATH_LITERAL_INTERNAL(x)

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define FILE_PATH_LITERAL(x) x
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
using CFStringRef = const struct __CFString*;
#endif

namespace kiwi {

class SafeBaseName;
class Pickle;
class PickleIterator;

/// An abstraction to isolate users from the differences between native
/// pathnames on different platforms.
class BASE_EXPORT FilePath {
  /// Remove trailing separators from this object.  If the path is absolute, it
  /// will never be stripped any more than to refer to the absolute root
  /// directory, so "////" will become "/", not "".  A leading pair of
  /// separators is never stripped, to support alternate roots.  This is used to
  /// support UNC paths on Windows.
  void StripTrailingSeparatorsInternal();

  bool IsParentFast(const FilePath& child) const;
  bool IsParentSlow(const FilePath& child) const;

 public:
#if BUILDFLAG(IS_WIN)
  /// On Windows, for Unicode-aware applications, native pathnames are wchar_t
  /// arrays encoded in UTF-16.
  using StringType = std::wstring;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  /// On most platforms, native pathnames are char arrays, and the encoding
  /// may or may not be specified.  On Mac OS X, native pathnames are encoded
  /// in UTF-8.
  using StringType = std::string;
#endif  // BUILDFLAG(IS_WIN)

  using CharType = StringType::value_type;
  using StringViewType = std::basic_string_view<CharType>;

  /// Null-terminated array of separators used to separate components in paths.
  /// Each character in this array is a valid separator, but kSeparators[0] is
  /// treated as the canonical separator and is used when composing pathnames.
  static constexpr CharType kSeparators[] =
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
      FILE_PATH_LITERAL("\\/");
#else   // FILE_PATH_USES_WIN_SEPARATORS
      FILE_PATH_LITERAL("/");
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  /// std::size(kSeparators), i.e., the number of separators in kSeparators plus
  /// one (the null terminator at the end of kSeparators).
  static constexpr size_t kSeparatorsLength = std::size(kSeparators);

  /// The special path component meaning "this directory."
  static constexpr CharType kCurrentDirectory[] = FILE_PATH_LITERAL(".");

  /// The special path component meaning "the parent directory."
  static constexpr CharType kParentDirectory[] = FILE_PATH_LITERAL("..");

  /// The character used to identify a file extension.
  static constexpr CharType kExtensionSeparator = FILE_PATH_LITERAL('.');

  /// Initializes features for this class.
  /// \see `base::features::Init()`.
  static void InitializeFeatures();

  /// \return A FilePath object from a path name in ASCII.
  static FilePath FromASCII(std::string_view ascii);

  /// Like AsUTF8Unsafe(), this function is unsafe. This function
  /// internally calls SysWideToNativeMB() on POSIX systems other than Mac
  /// and Chrome OS, to mitigate the encoding issue. See the comment at
  /// AsUTF8Unsafe() for details.
  ///
  /// \return A FilePath object from a path name in UTF-8. This function
  ///         should only be used for cases where you are sure that the input
  ///         string is UTF-8.
  static FilePath FromUTF8Unsafe(std::string_view utf8);

  // Similar to FromUTF8Unsafe, but accepts UTF-16 instead.
  static FilePath FromUTF16Unsafe(std::u16string_view utf16);

  /// Returns true if \p character is in \c kSeparators.
  ///
  /// \param character The character to check.
  /// \return True if the character is a path separator.
  static bool IsSeparator(CharType character);

  /// Compare two strings in the same way the file system does.
  /// Note that these always ignore case, even on file systems that are case-
  /// sensitive. If case-sensitive comparison is ever needed, add corresponding
  /// methods here.
  /// The methods are written as a static method so that they can also be used
  /// on parts of a file path, e.g., just the extension.
  /// CompareIgnoreCase() returns -1, 0 or 1 for less-than, equal-to and
  /// greater-than respectively.
  ///
  /// \param string1 The first string view.
  /// \param string2 The second string view.
  /// \return -1, 0, or 1 for less-than, equal-to, or greater-than.
  static int CompareIgnoreCase(StringViewType string1, StringViewType string2);
  static bool CompareEqualIgnoreCase(StringViewType string1,
                                     StringViewType string2) {
    return CompareIgnoreCase(string1, string2) == 0;
  }
  static bool CompareLessIgnoreCase(StringViewType string1,
                                    StringViewType string2) {
    return CompareIgnoreCase(string1, string2) < 0;
  }

#if BUILDFLAG(IS_APPLE)
  /// Returns the string in the special canonical decomposed form as defined for
  /// HFS, which is close to, but not quite, decomposition form D. See
  /// http://developer.apple.com/mac/library/technotes/tn/tn1150.html#UnicodeSubtleties
  /// for further comments.
  /// Returns the empty string if the conversion failed.
  ///
  /// \param string The input string view.
  /// \return The decomposed string, or the empty string if the conversion
  ///         failed.
  static StringType GetHFSDecomposedForm(StringViewType string);
  static StringType GetHFSDecomposedForm(CFStringRef cfstring);

  /// Special UTF-8 version of FastUnicodeCompare. Cf:
  /// http://developer.apple.com/mac/library/technotes/tn/tn1150.html#StringComparisonAlgorithm
  /// IMPORTANT: The input strings must be in the special HFS decomposed form!
  /// (cf. above GetHFSDecomposedForm method)
  ///
  /// \param string1 The first HFS decomposed string view.
  /// \param string2 The second HFS decomposed string view.
  /// \return The result of the comparison.
  static int HFSFastUnicodeCompare(StringViewType string1,
                                   StringViewType string2);
#endif

  /// Constructs an empty FilePath.
  FilePath();

  /// Constructs a FilePath from a string view.
  ///
  /// \param path The path string view.
  explicit FilePath(StringViewType path);

  /// Constructs a FilePath by copying another FilePath.
  ///
  /// \param that The FilePath object to copy.
  FilePath(const FilePath& that);

  /// Constructs FilePath with the contents of |that|, which is left in valid
  /// but unspecified state.
  ///
  /// \param that The FilePath object to move.
  FilePath(FilePath&& that) noexcept;

  /// Destroys the FilePath object.
  ~FilePath();

  /// Assigns the contents of another FilePath to this one by copying.
  ///
  /// \param that The FilePath object to copy.
  /// \return A reference to this FilePath object.
  FilePath& operator=(const FilePath& that);

  /// Replaces the contents with those of |that|, which is left in valid but
  /// unspecified state.
  ///
  /// \param that The FilePath object to move.
  /// \return A reference to this FilePath object.
  FilePath& operator=(FilePath&& that) noexcept;

  // On systems which use drive letters, the drive letters are compared
  // case-insensitively.

  /// Checks if this FilePath is equal to another.
  /// On systems which use drive letters, the drive letters are compared
  /// case-insensitively.
  ///
  /// \param that The FilePath to compare with.
  /// \return True if the paths are equal, otherwise false.
  bool operator==(const FilePath& that) const;

  /// Performs a three-way comparison with another FilePath.
  /// Required for some STL containers and operations.
  ///
  /// \param that The FilePath to compare with.
  /// \return The result of the comparison.
  auto operator<=>(const FilePath& that) const = default;

  /// \return A const reference to the underlying path string.
  const StringType& value() const KIWI_LIFETIME_BOUND { return path_; }

  /// \return True if the path string is empty, otherwise false.
  [[nodiscard]] bool empty() const { return path_.empty(); }

  /// Clears the path, making it empty.
  void clear() { path_.clear(); }

  /// Returns a vector of all of the components of the provided path.
  /// It is equivalent to calling DirName().value() on the path's root
  /// component, and BaseName().value() on each child component.
  ///
  /// To make sure this is lossless so we can differentiate absolute and
  /// relative paths, the root slash will be included even though no other
  /// slashes will be. The precise behavior is:
  ///
  /// Posix:  "/foo/bar"  ->  [ "/", "foo", "bar" ]
  /// Windows:  "C:\foo\bar"  ->  [ "C:", "\\", "foo", "bar" ]
  ///
  /// \return A vector of path components.
  std::vector<FilePath::StringType> GetComponents() const;

  /// Returns true if this FilePath is a parent or ancestor of the \p child.
  /// Absolute and relative paths are accepted i.e. /foo is a parent to
  /// /foo/bar, and foo is a parent to foo/bar. Any ancestor is considered a
  /// parent i.e. /a is a parent to both /a/b and /a/b/c.  Does not convert
  /// paths to absolute, follow symlinks or directory navigation (e.g. ".."). A
  /// path is *NOT* its own parent.
  ///
  /// \param child The potential child path.
  /// \return True if this path is a parent or ancestor of \p child.
  bool IsParent(const FilePath& child) const;

  /// If \c IsParent(child) holds, appends to \p path (if non-NULL) the
  /// relative path to \p child and returns true.  For example, if parent
  /// holds "/Users/johndoe/Library/Application Support", child holds
  /// "/Users/johndoe/Library/Application Support/Google/Chrome/Default", and
  /// *path holds "/Users/johndoe/Library/Caches", then after
  /// parent.AppendRelativePath(child, path) is called *path will hold
  /// "/Users/johndoe/Library/Caches/Google/Chrome/Default".  Otherwise,
  /// returns false.
  ///
  /// \param child The child path.
  /// \param path Output parameter where the relative path segment will be
  ///             appended if successful.
  /// \return True if this path is a parent of \p child and the relative path
  ///         was appended.
  bool AppendRelativePath(const FilePath& child, FilePath* path) const;

  /// Returns a FilePath corresponding to the directory containing the path
  /// named by this object, stripping away the file component.  If this object
  /// only contains one component, returns a FilePath identifying
  /// kCurrentDirectory.  If this object already refers to the root directory,
  /// returns a FilePath identifying the root directory. Please note that this
  /// doesn't resolve directory navigation, e.g. the result for "../a" is "..".
  ///
  /// \return A FilePath representing the directory.
  [[nodiscard]] FilePath DirName() const;

  /// Returns a FilePath corresponding to the last path component of this
  /// object, either a file or a directory.  If this object already refers to
  /// the root directory, returns a FilePath identifying the root directory;
  /// this is the only situation in which BaseName will return an absolute path.
  ///
  /// \return A FilePath representing the base name.
  [[nodiscard]] FilePath BaseName() const;

  /// Returns the extension of a file path.  This method works very similarly to
  /// FinalExtension(), except when the file path ends with a common
  /// double-extension.  For common double-extensions like ".tar.gz" and
  /// ".user.js", this method returns the combined extension.
  ///
  /// Common means that detecting double-extensions is based on a hard-coded
  /// allow-list (including but not limited to ".*.gz" and ".user.js") and isn't
  /// solely dependent on the number of dots.  Specifically, even if somebody
  /// invents a new Blah compression algorithm:
  ///   - calling this function with "foo.tar.bz2" will return ".tar.bz2", but
  ///   - calling this function with "foo.tar.blah" will return just ".blah"
  ///     until ".*.blah" is added to the hard-coded allow-list.
  ///
  /// That hard-coded allow-list is case-insensitive: ".GZ" and ".gz" are
  /// equivalent. However, the StringType returned is not canonicalized for
  /// case: "foo.TAR.bz2" input will produce ".TAR.bz2", not ".tar.bz2", and
  /// "bar.EXT", which is not a double-extension, will produce ".EXT".
  ///
  /// The following code should always work regardless of the value of path.
  /// \code
  ///   new_path = path.RemoveExtension().value().append(path.Extension());
  ///   ASSERT(new_path == path.value());
  /// \endcode
  ///
  /// NOTE: this is different from the original file_util implementation which
  /// returned the extension without a leading "." ("jpg" instead of ".jpg").
  ///
  /// \return The full extension (including the leading dot).
  [[nodiscard]] StringType Extension() const;

  /// Returns the final extension of a file path, or an empty string if the file
  /// path has no extension.  In most cases, the final extension of a file path
  /// refers to the part of the file path from the last dot to the end
  /// (including the dot itself).  For example, this method applied to
  /// "/pics/jojo.jpg" and "/pics/jojo." returns ".jpg" and ".", respectively.
  /// However, if the base name of the file path is either "." or "..", this
  /// method returns an empty string.
  ///
  /// TODO(davidben): Check all our extension-sensitive code to see if
  /// we can rename this to Extension() and the other to something like
  /// LongExtension(), defaulting to short extensions and leaving the
  /// long "extensions" to logic like base::GetUniquePathNumber().
  ///
  /// \return The final extension (including the leading dot), or an empty
  ///         string.
  [[nodiscard]] StringType FinalExtension() const;

  /// Returns "C:\pics\jojo" for path "C:\pics\jojo.jpg"
  /// NOTE: this is slightly different from the similar file_util implementation
  /// which returned simply 'jojo'.
  ///
  /// \return A FilePath with the full extension removed.
  [[nodiscard]] FilePath RemoveExtension() const;

  /// Removes the path's file extension, as in RemoveExtension(), but
  /// ignores double extensions.
  ///
  /// \return A FilePath with the final extension removed.
  [[nodiscard]] FilePath RemoveFinalExtension() const;

  /// Inserts \p suffix after the file name portion of |path| but before the
  /// extension.  Returns "" if BaseName() == "." or "..".
  /// Examples:
  /// \code
  /// path = "C:\pics\jojo.jpg" suffix = " (1)", returns "C:\pics\jojo (1).jpg"
  /// path = "jojo.jpg"         suffix = " (1)", returns "jojo (1).jpg"
  /// path = "C:\pics\jojo"     suffix = " (1)", returns "C:\pics\jojo (1)"
  /// path = "C:\pics.old\jojo" suffix = " (1)", returns "C:\pics.old\jojo (1)"
  /// \endcode
  ///
  /// \return A new FilePath with the suffix inserted, or an empty string if \c
  ///         BaseName() == "." or "..".
  [[nodiscard]] FilePath InsertBeforeExtension(StringViewType suffix) const;

  /// Like above, but takes the suffix as an ASCII string.
  [[nodiscard]] FilePath InsertBeforeExtensionASCII(
      std::string_view suffix) const;

  /// Like above, but takes the `suffix` as an UTF8 string.
  /// While all modern OSes support UTF-8, there is no requirement for the
  /// filenames to actually be UTF-8, e.g. on Linux. So inserting UTF-8 could
  /// result in Mojibake filenames.
  ///
  /// \param suffix The UTF8 suffix to insert.
  /// \return A new FilePath with the suffix inserted, or an empty string.
  [[nodiscard]] FilePath InsertBeforeExtensionUTF8(
      std::string_view suffix) const;

  /// Adds \p extension to |file_name|.
  ///
  /// \param extension The extension to add.
  /// \return A new FilePath with the extension added, or the current path if \p
  ///         extension is empty, or "" if \c BaseName() == "." or "..".
  [[nodiscard]] FilePath AddExtension(StringViewType extension) const;

  /// Like above, but takes the extension as an ASCII string. See AppendASCII
  /// for details on how this is handled.
  [[nodiscard]] FilePath AddExtensionASCII(std::string_view extension) const;

  /// Like above, but takes the extension as an UTF8 string. See AppendUTF8 for
  /// details on how this is handled.
  /// While all modern OSes support UTF-8, there is no requirement for the
  /// filenames to actually be UTF-8, e.g. on Linux. So appending UTF-8 could
  /// result in Mojibake filenames.
  [[nodiscard]] FilePath AddExtensionUTF8(std::string_view extension) const;

  /// Replaces the extension of |file_name| with \p extension.  If |file_name|
  /// does not have an extension, then |extension| is added.  If |extension| is
  /// empty, then the extension is removed from |file_name|.
  ///
  /// \param extension The new extension.
  /// \return A new FilePath with the extension replaced/added/removed, or ""
  ///         if \c BaseName() == "." or "..".
  [[nodiscard]] FilePath ReplaceExtension(StringViewType extension) const;

  /// Returns true if file path's \c Extension() matches \p extension. The test
  /// is case insensitive. Don't forget the leading period if appropriate.
  ///
  /// \param extension The extension to match against.
  /// \return True if the full extension matches \p extension
  ///         (case-insensitive).
  bool MatchesExtension(StringViewType extension) const;

  /// Returns true if file path's FinalExtension() matches `extension`. The
  /// test is case insensitive. Don't forget the leading period if appropriate.
  bool MatchesFinalExtension(StringViewType extension) const;

  /// Returns a FilePath by appending a separator and the supplied path
  /// component to this object's path.  Append takes care to avoid adding
  /// excessive separators if this object's path already ends with a separator.
  /// If this object's path is kCurrentDirectory ('.'), a new FilePath
  /// corresponding only to |component| is returned.  |component| must be a
  /// relative path; it is an error to pass an absolute path.
  ///
  /// \param component The component to append.
  /// \return A new FilePath with the component appended.
  [[nodiscard]] FilePath Append(StringViewType component) const;
  [[nodiscard]] FilePath Append(const FilePath& component) const;
  [[nodiscard]] FilePath Append(const SafeBaseName& component) const;

  /// Although Windows StringType is std::wstring, since the encoding it uses
  /// for paths is well defined, it can handle ASCII path components as well.
  /// Mac uses UTF8, and since ASCII is a subset of that, it works there as
  /// well. On Linux, although it can use any 8-bit encoding for paths, we
  /// assume that ASCII is a valid subset, regardless of the encoding, since
  /// many operating system paths will always be ASCII.
  ///
  /// \param component The ASCII component to append.
  /// \return A new FilePath with the component appended.
  [[nodiscard]] FilePath AppendASCII(std::string_view component) const;

  /// Like above, but takes the `component` as an UTF8 string.
  /// While all modern OSes support UTF-8, there is no requirement for the
  /// filenames to actually be UTF-8, e.g. on Linux. So appending UTF-8 could
  /// result in Mojibake filenames.
  [[nodiscard]] FilePath AppendUTF8(std::string_view component) const;

  /// Returns true if this FilePath contains an absolute path.  On Windows, an
  /// absolute path begins with either a drive letter specification followed by
  /// a separator character, or with two separator characters.  On POSIX
  /// platforms, an absolute path begins with a separator character.
  ///
  /// \return True if the path is absolute.
  bool IsAbsolute() const;

  /// Returns true if this FilePath is a network path which starts with 2 path
  /// separators. See class documentation for 'Alternate root'.
  ///
  /// \return True if the path is a network path.
  bool IsNetwork() const;

  /// \return True if the patch ends with a path separator character.
  [[nodiscard]] bool EndsWithSeparator() const;

  /// \return A copy of this FilePath that ends with a trailing separator. If
  ///         the input path is empty, an empty FilePath will be returned.
  [[nodiscard]] FilePath AsEndingWithSeparator() const;

  /// \return A copy of this FilePath that does not end with a trailing
  ///         separator.
  [[nodiscard]] FilePath StripTrailingSeparators() const;

  /// \return True if this FilePath contains an attempt to reference a parent
  ///         directory (e.g. has a path component that is "..").
  bool ReferencesParent() const;

  /// \return A Unicode human-readable version of this path.
  ///         Warning: you can *not*, in general, go from a display name back to
  ///         a real path.  Only use this when displaying paths to users, not
  ///         just when you want to stuff a std::u16string into some other API.
  std::u16string LossyDisplayName() const;

  /// \return The path as ASCII, or the empty string if the path is not ASCII.
  ///         This should only be used for cases where the FilePath is
  ///         representing a known-ASCII filename.
  std::string MaybeAsASCII() const;

  /// This function is *unsafe* as there is no way to tell what encoding is
  /// used in file names on POSIX systems other than Mac and Chrome OS,
  /// although UTF-8 is practically used everywhere these days. To mitigate
  /// the encoding issue, this function internally calls
  /// SysNativeMBToWide() on POSIX systems other than Mac and Chrome OS,
  /// per assumption that the current locale's encoding is used in file
  /// names, but this isn't a perfect solution.
  ///
  /// Once it becomes safe to to stop caring about non-UTF-8 file names,
  /// the SysNativeMBToWide() hack will be removed from the code, along
  /// with "Unsafe" in the function name.
  ///
  /// \return The path as UTF-8.
  std::string AsUTF8Unsafe() const;

  /// Similar to AsUTF8Unsafe, but returns UTF-16 instead.
  std::u16string AsUTF16Unsafe() const;

  void WriteToPickle(Pickle* pickle) const;
  bool ReadFromPickle(PickleIterator* iter);

  /// Normalize all path separators to backslash on Windows
  /// (if FILE_PATH_USES_WIN_SEPARATORS is true), or do nothing on POSIX
  /// systems.
  ///
  /// \return A new FilePath with normalized separators.
  [[nodiscard]] FilePath NormalizePathSeparators() const;

  /// Normalize all path separators to given type on Windows
  /// (if FILE_PATH_USES_WIN_SEPARATORS is true), or do nothing on POSIX
  /// systems.
  ///
  /// \param separator The separator character to normalize to.
  /// \return A new FilePath with normalized separators.
  [[nodiscard]] FilePath NormalizePathSeparatorsTo(CharType separator) const;

  // Serialise this object into a trace.
  // void WriteIntoTrace(perfetto::TracedValue context) const;

#if BUILDFLAG(IS_ANDROID)
  /// On android, file selection dialog can return a file with content uri
  /// scheme(starting with content://). Content uri needs to be opened with
  /// ContentResolver to guarantee that the app has appropriate permissions
  /// to access it.
  /// Returns true if the path is a content uri, or false otherwise.
  bool IsContentUri() const;
#endif

  /// NOTE: When adding a new public method, consider adding it to
  /// file_path_fuzzer.cc as well.

 private:
  StringType path_;
};

BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const FilePath& file_path);

}  // namespace kiwi

namespace std {

template <>
struct hash<kiwi::FilePath> {
  typedef kiwi::FilePath argument_type;
  typedef std::size_t result_type;

  result_type operator()(argument_type const& f) const {
    return hash<kiwi::FilePath::StringType>()(f.value());
  }
};

}  // namespace std