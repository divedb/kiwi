///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <exception>  // for std::terminate()

namespace kiwi {

#if defined(__clang__) || defined(__GNUC__)
#define KIWI_LIKELY(x) __builtin_expect(!!(x), 1)
#define KIWI_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else
#define KIWI_LIKELY(x) (!!(x))
#define KIWI_UNLIKELY(x) (!!(x))
#endif  // defined(__clang__) || defined(__GNUC__)

namespace detail {

[[noreturn]] inline void terminate() noexcept { std::terminate(); }

}  // namespace detail

#define KIWI_CONTRACT_CHECK(type, cond) \
  (KIWI_LIKELY(cond) ? static_cast<void>(0) : detail::terminate())

#define EXPECT(cond) KIWI_CONTRACT_CHECK("Precondition", cond)
#define ENSURE(cond) KIWI_CONTRACT_CHECK("Postcondition", cond)

}  // namespace kiwi