#pragma once

#ifndef IS_GNUC
#define IS_GNUC() (defined(__GNUC__))
#endif

#ifndef IS_CLANG
#define IS_CLANG() (defined(__clang__))
#endif

#ifndef IS_MSC
#define IS_MSC() (defined(_MSC_VER))
#endif

#if defined(__has_feature)
#define KIWI_HAS_FEATURE(...) __has_feature(__VA_ARGS__)
#else
#define KWI_HAS_FEATURE(...) 0
#endif

#if defined(__has_builtin)
#define KIWI_HAS_BUILTIN(...) __has_builtin(__VA_ARGS__)
#else
#define KIWI_HAS_BUILTIN(...) 0
#endif

#if IS_GNUC()
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif
