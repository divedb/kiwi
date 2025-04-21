// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/strings/utf_ostream_operators.hh"

#include "kiwi/strings/utf_string_conversions.hh"
#include "kiwi/types/supports_ostream_operator.hh"

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << (wstr ? std::wstring_view(wstr) : std::wstring_view());
}

std::ostream& std::operator<<(std::ostream& out, std::wstring_view wstr) {
  return out << kiwi::WideToUTF8(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << std::wstring_view(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const char16_t* str16) {
  return out << (str16 ? std::u16string_view(str16) : std::u16string_view());
}

std::ostream& std::operator<<(std::ostream& out, std::u16string_view str16) {
  return out << kiwi::UTF16ToUTF8(str16);
}

std::ostream& std::operator<<(std::ostream& out, const std::u16string& str16) {
  return out << std::u16string_view(str16);
}