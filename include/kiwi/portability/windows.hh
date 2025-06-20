/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef _WIN32

#include <stdio.h>

// This header is intended to be used in-place of including
// - <Windows.h>
// - <WinSock2.h>
// - <io.h>
// - <direct.h>.
//
// It includes all of them, and undefines certain names defined by them that
// are used in places in kiwi.
//
// These have to be this way because we define our own versions of `close()`,
// because the normal Windows versions don't handle sockets at all.
//
// There are some ordering issues internally in the SDK; we need to ensure
// stdio.h is included prior to including direct.h and io.h with internal names
// disabled to ensure all of the normal names get declared properly.
#include <direct.h>
#include <io.h>

#if defined(min) || defined(max)
#error Windows.h needs to be included by this header, or else NOMINMAX needs \
 to be defined before including it yourself.
#endif

// This is needed because, for some absurd reason, one of the windows headers
// tries to define "min" and "max" as macros, which messes up most uses of
// std::numeric_limits.
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <WinSock2.h>
#include <Windows.h>

#ifdef CAL_GREGORIAN
#undef CAL_GREGORIAN
#endif

// Defined in the GDI interface.
#ifdef ERROR
#undef ERROR
#endif

// Defined in minwindef.h
#ifdef IN
#undef IN
#endif

// Defined in winerror.h
#ifdef NO_ERROR
#undef NO_ERROR
#endif

// Defined in minwindef.h
#ifdef OUT
#undef OUT
#endif

// Defined in minwindef.h
#ifdef STRICT
#undef STRICT
#endif

// Defined in Winbase.h
#ifdef Yield
#undef Yield
#endif

// Defined in nb30.h
#ifdef REGISTERED
#undef REGISTERED
#endif

#endif
