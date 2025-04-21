#include "kiwi/strings/ascii.hh"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstring>
#include <string>

#include "kiwi/base/macros.hh"

namespace {

TEST(AsciiIsFoo, All) {
  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      EXPECT_TRUE(kiwi::ascii_isalpha(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isalpha(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if ((c >= '0' && c <= '9'))
      EXPECT_TRUE(kiwi::ascii_isdigit(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isdigit(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (kiwi::ascii_isalpha(c) || kiwi::ascii_isdigit(c))
      EXPECT_TRUE(kiwi::ascii_isalnum(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isalnum(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i != '\0' && strchr(" \r\n\t\v\f", i))
      EXPECT_TRUE(kiwi::ascii_isspace(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isspace(c)) << ": failed on " << c;
  }

  // Check isprint.
  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i >= 32 && i < 127)
      EXPECT_TRUE(kiwi::ascii_isprint(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isprint(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (kiwi::ascii_isprint(c) && !kiwi::ascii_isspace(c) &&
        !kiwi::ascii_isalnum(c)) {
      EXPECT_TRUE(kiwi::ascii_ispunct(c)) << ": failed on " << c;
    } else {
      EXPECT_TRUE(!kiwi::ascii_ispunct(c)) << ": failed on " << c;
    }
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i == ' ' || i == '\t')
      EXPECT_TRUE(kiwi::ascii_isblank(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isblank(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i < 32 || i == 127)
      EXPECT_TRUE(kiwi::ascii_iscntrl(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_iscntrl(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (kiwi::ascii_isdigit(c) || (i >= 'A' && i <= 'F') ||
        (i >= 'a' && i <= 'f')) {
      EXPECT_TRUE(kiwi::ascii_isxdigit(c)) << ": failed on " << c;
    } else {
      EXPECT_TRUE(!kiwi::ascii_isxdigit(c)) << ": failed on " << c;
    }
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i > 32 && i < 127)
      EXPECT_TRUE(kiwi::ascii_isgraph(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isgraph(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i >= 'A' && i <= 'Z')
      EXPECT_TRUE(kiwi::ascii_isupper(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_isupper(c)) << ": failed on " << c;
  }

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (i >= 'a' && i <= 'z')
      EXPECT_TRUE(kiwi::ascii_islower(c)) << ": failed on " << c;
    else
      EXPECT_TRUE(!kiwi::ascii_islower(c)) << ": failed on " << c;
  }

  for (unsigned char c = 0; c < 128; c++) {
    EXPECT_TRUE(kiwi::ascii_isascii(c)) << ": failed on " << c;
  }

  for (int i = 128; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    EXPECT_TRUE(!kiwi::ascii_isascii(c)) << ": failed on " << c;
  }
}

/// Checks that kiwi::ascii_isfoo returns the same value as isfoo in the C
/// locale.
TEST(AsciiIsFoo, SameAsIsFoo) {
#ifndef __ANDROID__
  // temporarily change locale to C. It should already be C, but just for safety
  const char* old_locale = setlocale(LC_CTYPE, "C");
  ASSERT_TRUE(old_locale != nullptr);
#endif

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    EXPECT_EQ(isalpha(c) != 0, kiwi::ascii_isalpha(c)) << c;
    EXPECT_EQ(isdigit(c) != 0, kiwi::ascii_isdigit(c)) << c;
    EXPECT_EQ(isalnum(c) != 0, kiwi::ascii_isalnum(c)) << c;
    EXPECT_EQ(isspace(c) != 0, kiwi::ascii_isspace(c)) << c;
    EXPECT_EQ(ispunct(c) != 0, kiwi::ascii_ispunct(c)) << c;
    EXPECT_EQ(isblank(c) != 0, kiwi::ascii_isblank(c)) << c;
    EXPECT_EQ(iscntrl(c) != 0, kiwi::ascii_iscntrl(c)) << c;
    EXPECT_EQ(isxdigit(c) != 0, kiwi::ascii_isxdigit(c)) << c;
    EXPECT_EQ(isprint(c) != 0, kiwi::ascii_isprint(c)) << c;
    EXPECT_EQ(isgraph(c) != 0, kiwi::ascii_isgraph(c)) << c;
    EXPECT_EQ(isupper(c) != 0, kiwi::ascii_isupper(c)) << c;
    EXPECT_EQ(islower(c) != 0, kiwi::ascii_islower(c)) << c;
    EXPECT_EQ(isascii(c) != 0, kiwi::ascii_isascii(c)) << c;
  }

#ifndef __ANDROID__
  // restore the old locale.
  ASSERT_TRUE(setlocale(LC_CTYPE, old_locale));
#endif
}

TEST(AsciiToFoo, All) {
#ifndef __ANDROID__
  // temporarily change locale to C. It should already be C, but just for safety
  const char* old_locale = setlocale(LC_CTYPE, "C");
  ASSERT_TRUE(old_locale != nullptr);
#endif

  for (int i = 0; i < 256; i++) {
    const auto c = static_cast<unsigned char>(i);
    if (kiwi::ascii_islower(c))
      EXPECT_EQ(kiwi::ascii_toupper(c), 'A' + (i - 'a')) << c;
    else
      EXPECT_EQ(kiwi::ascii_toupper(c), static_cast<char>(i)) << c;

    if (kiwi::ascii_isupper(c))
      EXPECT_EQ(kiwi::ascii_tolower(c), 'a' + (i - 'A')) << c;
    else
      EXPECT_EQ(kiwi::ascii_tolower(c), static_cast<char>(i)) << c;

    // These CHECKs only hold in a C locale.
    EXPECT_EQ(static_cast<char>(tolower(i)), kiwi::ascii_tolower(c)) << c;
    EXPECT_EQ(static_cast<char>(toupper(i)), kiwi::ascii_toupper(c)) << c;
  }
#ifndef __ANDROID__
  // restore the old locale.
  ASSERT_TRUE(setlocale(LC_CTYPE, old_locale));
#endif
}

TEST(AsciiStrTo, Lower) {
  const char buf[] = "ABCDEF";
  const std::string str("GHIJKL");
  const std::string str2("MNOPQR");
  const std::string_view sp(str2);
  const std::string long_str("ABCDEFGHIJKLMNOPQRSTUVWXYZ1!a");
  std::string mutable_str("_`?@[{AMNOPQRSTUVWXYZ");
  auto fun = []() -> std::string { return "PQRSTU"; };

  EXPECT_EQ("abcdef", kiwi::ascii_str_to_lower(buf));
  EXPECT_EQ("ghijkl", kiwi::ascii_str_to_lower(str));
  EXPECT_EQ("mnopqr", kiwi::ascii_str_to_lower(sp));
  EXPECT_EQ("abcdefghijklmnopqrstuvwxyz1!a",
            kiwi::ascii_str_to_lower(long_str));
  EXPECT_EQ("pqrstu", kiwi::ascii_str_to_lower(fun()));

  kiwi::ascii_str_to_lower(&mutable_str);
  EXPECT_EQ("_`?@[{amnopqrstuvwxyz", mutable_str);

  char mutable_buf[] = "Mutable";
  std::transform(mutable_buf, mutable_buf + strlen(mutable_buf), mutable_buf,
                 kiwi::ascii_tolower);
  EXPECT_STREQ("mutable", mutable_buf);
}

TEST(AsciiStrTo, Upper) {
  const char buf[] = "abcdef";
  const std::string str("ghijkl");
  const std::string str2("_`?@[{amnopqrstuvwxyz");
  const std::string_view sp(str2);
  const std::string long_str("abcdefghijklmnopqrstuvwxyz1!A");
  auto fun = []() -> std::string { return "pqrstu"; };

  EXPECT_EQ("ABCDEF", kiwi::ascii_str_to_upper(buf));
  EXPECT_EQ("GHIJKL", kiwi::ascii_str_to_upper(str));
  EXPECT_EQ("_`?@[{AMNOPQRSTUVWXYZ", kiwi::ascii_str_to_upper(sp));
  EXPECT_EQ("ABCDEFGHIJKLMNOPQRSTUVWXYZ1!A",
            kiwi::ascii_str_to_upper(long_str));
  EXPECT_EQ("PQRSTU", kiwi::ascii_str_to_upper(fun()));

  char mutable_buf[] = "Mutable";
  std::transform(mutable_buf, mutable_buf + strlen(mutable_buf), mutable_buf,
                 kiwi::ascii_toupper);
  EXPECT_STREQ("MUTABLE", mutable_buf);
}

TEST(StripLeadingAsciiWhitespace, FromStringView) {
  EXPECT_EQ(std::string_view{},
            kiwi::strip_leading_ascii_whitespace(std::string_view{}));
  EXPECT_EQ("foo", kiwi::strip_leading_ascii_whitespace({"foo"}));
  EXPECT_EQ("foo", kiwi::strip_leading_ascii_whitespace({"\t  \n\f\r\n\vfoo"}));
  EXPECT_EQ("foo foo\n ",
            kiwi::strip_leading_ascii_whitespace({"\t  \n\f\r\n\vfoo foo\n "}));
  EXPECT_EQ(std::string_view{}, kiwi::strip_leading_ascii_whitespace(
                                    {"\t  \n\f\r\v\n\t  \n\f\r\v\n"}));
}

TEST(StripLeadingAsciiWhitespace, InPlace) {
  std::string str;

  kiwi::strip_leading_ascii_whitespace(&str);
  EXPECT_EQ("", str);

  str = "foo";
  kiwi::strip_leading_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = "\t  \n\f\r\n\vfoo";
  kiwi::strip_leading_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = "\t  \n\f\r\n\vfoo foo\n ";
  kiwi::strip_leading_ascii_whitespace(&str);
  EXPECT_EQ("foo foo\n ", str);

  str = "\t  \n\f\r\v\n\t  \n\f\r\v\n";
  kiwi::strip_leading_ascii_whitespace(&str);
  EXPECT_EQ(std::string_view{}, str);
}

TEST(StripTrailingAsciiWhitespace, FromStringView) {
  EXPECT_EQ(std::string_view{},
            kiwi::strip_trailing_ascii_whitespace(std::string_view{}));
  EXPECT_EQ("foo", kiwi::strip_trailing_ascii_whitespace({"foo"}));
  EXPECT_EQ("foo",
            kiwi::strip_trailing_ascii_whitespace({"foo\t  \n\f\r\n\v"}));
  EXPECT_EQ(" \nfoo foo", kiwi::strip_trailing_ascii_whitespace(
                              {" \nfoo foo\t  \n\f\r\n\v"}));
  EXPECT_EQ(std::string_view{}, kiwi::strip_trailing_ascii_whitespace(
                                    {"\t  \n\f\r\v\n\t  \n\f\r\v\n"}));
}

TEST(StripTrailingAsciiWhitespace, InPlace) {
  std::string str;

  kiwi::strip_trailing_ascii_whitespace(&str);
  EXPECT_EQ("", str);

  str = "foo";
  kiwi::strip_trailing_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = "foo\t  \n\f\r\n\v";
  kiwi::strip_trailing_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = " \nfoo foo\t  \n\f\r\n\v";
  kiwi::strip_trailing_ascii_whitespace(&str);
  EXPECT_EQ(" \nfoo foo", str);

  str = "\t  \n\f\r\v\n\t  \n\f\r\v\n";
  kiwi::strip_trailing_ascii_whitespace(&str);
  EXPECT_EQ(std::string_view{}, str);
}

TEST(StripAsciiWhitespace, FromStringView) {
  EXPECT_EQ(std::string_view{},
            kiwi::strip_ascii_whitespace(std::string_view{}));
  EXPECT_EQ("foo", kiwi::strip_ascii_whitespace({"foo"}));
  EXPECT_EQ("foo",
            kiwi::strip_ascii_whitespace({"\t  \n\f\r\n\vfoo\t  \n\f\r\n\v"}));
  EXPECT_EQ("foo foo", kiwi::strip_ascii_whitespace(
                           {"\t  \n\f\r\n\vfoo foo\t  \n\f\r\n\v"}));
  EXPECT_EQ(std::string_view{},
            kiwi::strip_ascii_whitespace({"\t  \n\f\r\v\n\t  \n\f\r\v\n"}));
}

TEST(StripAsciiWhitespace, InPlace) {
  std::string str;

  kiwi::strip_ascii_whitespace(&str);
  EXPECT_EQ("", str);

  str = "foo";
  kiwi::strip_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = "\t  \n\f\r\n\vfoo\t  \n\f\r\n\v";
  kiwi::strip_ascii_whitespace(&str);
  EXPECT_EQ("foo", str);

  str = "\t  \n\f\r\n\vfoo foo\t  \n\f\r\n\v";
  kiwi::strip_ascii_whitespace(&str);
  EXPECT_EQ("foo foo", str);

  str = "\t  \n\f\r\v\n\t  \n\f\r\v\n";
  kiwi::strip_ascii_whitespace(&str);
  EXPECT_EQ(std::string_view{}, str);
}

TEST(RemoveExtraAsciiWhitespace, InPlace) {
  const char* inputs[] = {"No extra space",
                          "  Leading whitespace",
                          "Trailing whitespace  ",
                          "  Leading and trailing  ",
                          " Whitespace \t  in\v   middle  ",
                          "'Eeeeep!  \n Newlines!\n",
                          "nospaces",
                          "",
                          "\n\t a\t\n\nb \t\n"};

  const char* outputs[] = {
      "No extra space",
      "Leading whitespace",
      "Trailing whitespace",
      "Leading and trailing",
      "Whitespace in middle",
      "'Eeeeep! Newlines!",
      "nospaces",
      "",
      "a\nb",
  };
  const int kNumTests = KIWI_ARRAYSIZE(inputs);

  for (int i = 0; i < kNumTests; i++) {
    std::string s(inputs[i]);
    kiwi::remove_extra_ascii_whitespace(&s);
    EXPECT_EQ(outputs[i], s);
  }
}

}  // namespace
