// This file is part of corral, a lightweight C++20 coroutine library.
//
// Copyright (c) 2024-2025 Hudson River Trading LLC
// <opensource@hudson-trading.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// SPDX-License-Identifier: MIT

#pragma once

namespace kiwi {

namespace detail {

/// Mimics what the compiler does to obtain an awaiter from whatever
/// is passed to co_await, plus a fallback to support AwaitableLambda:
/// any suitable `kiwi::detail::operator co_await(T&&)` will be
/// considered even if it would not be found via ADL, as long as the
/// `operator co_await` is declared before `GetAwaiter()` is defined.
/// You will need a corresponding 'ThisIsAwaitableTrustMe'
/// specialization in order to make the object satisfy Awaitable, since
/// the Awaitable concept was declared before the `operator co_await`.
///
/// The return type of this function is as follows:
/// - If T&& is Awaiter, then T&&. (Like std::forward: you get
///   a lvalue or rvalue reference depending on the value category of `t`,
///   and no additional object is created.)
/// - If T&& defines operator co_await() returning value type A or rvalue
///   reference A&&, then A. (The awaiter is constructed or moved into
///   the return value slot.)
/// - If T&& defines operator co_await() returning lvalue reference A&,
///   then A&. (We do not make a copy.)
///
/// It is important to pay attention to the value category in order to
/// avoid a dangling reference if a function constructs a combination of
/// awaiters and then returns it. Typically the return value of
/// GetAwaiter(T&&) should be used to initialize an object of type
/// AwaiterType<T&&>; AwaiterType will be a value type or lvalue
/// reference, but not an rvalue reference.
template <typename T>
decltype(auto) GetAwaiter(T&& t);

}  // namespace detail

}  // namespace kiwi