#pragma once

/// _M_X64: When compiling to target x64 processors or ARM64EC, it is defined to
/// be the integer literal value 100; otherwise, it is undefined.
///
/// https://learn.microsoft.com/ja-jp/cpp/preprocessor/predefined-macros?view=msvc-170
#ifndef IS_X64
#define IS_X64() (defined(_M_X64) || defined(__x86_64__) || defined(__amd64__))
#endif

#ifndef IS_I386
#define IS_I386() (defined(_M_IX86) || defined(__i386__) || defined(__i386))
#endif

/// _MSC_VER=MMNN
/// _MSC_FULL_VER=MMNNBBBBB
/// _MSC_BUILD=R
///
/// For example, the compiler version for Visual Studio 2022 version 17.9.0
/// is 19.39.33519:
///
/// The major version is 19
/// The minor version is 39
/// Build is 33519
/// Revision 0
/// The macros reflect these values ​​as follows:
///
/// _MSC_VER = 1939
/// _MSC_FULL_VER = 193933519
/// _MSC_BUILD(revised) is 0.
#ifndef IS_MSC
#define IS_MSC() (defined(_MSC_VER))
#endif

#ifndef IS_ARM32
#define IS_ARM32() (defined(__arm__) || defined(__thumb__) || defined(_M_ARM))
#endif

#ifndef IS_ARM64
#define IS_ARM64() (defined(__aarch64__) || defined(_M_ARM64))
#endif

#if defined(__SSE__)
#define SSE_VER 1
#define SSE_MINOR 0
#elif defined(__SSE2__)
#define SSE_VER 2
#define SSE_MINOR 0
#elif defined(__SSE3__)
#define SSE_VER 3
#define SSE_MINOR 0
#elif defined(__SSE4__)
#define SSE_VER 4
#define SSE_MINOR 0
#elif defined(__SSE4_1__)
#define SSE_VER 4
#define SSE_MINOR 1
#elif defined(__SSE4_2__)
#define SSE_VER 4
#define SSE_MINOR 2
#else
#define SSE_VER 0
#define SSE_MINOR 0
#endif

#ifndef HAS_SSSE3
#define HAS_SSSE3() (defined(__SSSE3__))
#endif

#ifndef HAS_SSE_MATH
#define HAS_SSE_MATH() (defined(__SSE_MATH__))
#endif

#ifndef HAS_SSE2_MATH
#define HAS_SSE2_MATH() (defined(__SSE2_MATH__))
#endif

#ifndef HAS_AVX
#define HAS_AVX() (defined(__AVX__))
#endif

#ifndef HAS_AVX2
#define HAS_AVX2() (defined(__AVX2__))
#endif

#ifndef HAS_AVX512F
#define HAS_AVX512F() (defined(__AVX512F__))
#endif

#ifndef HAS_AVX512CD
#define HAS_AVX512CD() (defined(__AVX512CD__))
#endif

#ifndef HAS_AVX512ER
#define HAS_AVX512ER() (defined(__AVX512ER__))
#endif

#ifndef HAS_AVX512PF
#define HAS_AVX512PF() (defined(__AVX512PF__))
#endif

inline constexpr auto kIsLittleEndian =
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
inline constexpr auto kIsBigEndian = !kIsLittleEndian;
