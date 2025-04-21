// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/strings/strcat.hh"

#include <string>
#include <string_view>

#include "kiwi/strings/strcat_internal.hh"

namespace kiwi {

std::string StrCat(span<const std::string_view> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const std::u16string_view> pieces) {
  return internal::StrCatT(pieces);
}

std::string StrCat(span<const std::string> pieces) {
  return internal::StrCatT(pieces);
}

std::u16string StrCat(span<const std::u16string> pieces) {
  return internal::StrCatT(pieces);
}

void StrAppend(std::string* dest, span<const std::string_view> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const std::u16string_view> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::string* dest, span<const std::string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

void StrAppend(std::u16string* dest, span<const std::u16string> pieces) {
  internal::StrAppendT(*dest, pieces);
}

}  // namespace kiwi