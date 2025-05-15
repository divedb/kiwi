#pragma once

// Reference:
//   https://en.cppreference.com/w/cpp/feature_test

#if defined(_MSC_VER)
#define KIWI_CPLUSPLUS _MSVC_LANG
#else
#define KIWI_CPLUSPLUS __cplusplus
#endif

// C++20 permits more cases to be marked constexpr, including constructors that
// leave members uninitialized and virtual functions.
#if KIWI_CPLUSPLUS >= 202002L
#define KIWI_CXX20_CONSTEXPR constexpr
#else
#define KIWI_CXX20_CONSTEXPR
#endif

// C++23 permits more cases to be marked constexpr, including definitions of
// variables of non-literal type in constexpr function as long as they are not
// constant-evaluated.
#if KIWI_CPLUSPLUS >= 202302L
#define KIWI_CXX23_CONSTEXPR constexpr
#else
#define KIWI_CXX23_CONSTEXPR
#endif

// C++20 constinit.
#if defined(__cpp_constinit) && __cpp_constinit >= 201907L
#define KIWI_CONSTINIT constinit
#else
#define KIWI_CONSTINIT
#endif

// C++20 consteval.
#if KIWI_CPLUSPLUS >= 202002L
#define KIWI_CONSTEVAL consteval
#else
#define KIWI_CONSTEVAL constexpr
#endif