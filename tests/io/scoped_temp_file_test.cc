// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kiwi/io/scoped_temp_file.hh"

#include <gtest/gtest.h>

#include <utility>

#include "kiwi/io/file_util.hh"

namespace kiwi {

TEST(ScopedTempFile, Basic) {
  ScopedTempFile temp_file;
  EXPECT_TRUE(temp_file.Path().empty());

  EXPECT_TRUE(temp_file.Create());
  EXPECT_TRUE(PathExists(temp_file.Path()));

  EXPECT_TRUE(temp_file.Delete());
  EXPECT_FALSE(PathExists(temp_file.Path()));
}

TEST(ScopedTempFile, MoveConstruct) {
  ScopedTempFile temp1;
  EXPECT_TRUE(temp1.Create());
  FilePath file1 = temp1.Path();
  EXPECT_TRUE(PathExists(file1));

  ScopedTempFile temp2(std::move(temp1));
  EXPECT_TRUE(temp1.Path().empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(file1, temp2.Path());
  EXPECT_TRUE(PathExists(file1));
}

TEST(ScopedTempFile, MoveAssign) {
  ScopedTempFile temp1;
  EXPECT_TRUE(temp1.Create());
  FilePath file1 = temp1.Path();
  EXPECT_TRUE(PathExists(file1));

  ScopedTempFile temp2;
  EXPECT_TRUE(temp2.Create());
  FilePath file2 = temp2.Path();
  EXPECT_TRUE(PathExists(file2));

  temp2 = std::move(temp1);
  EXPECT_TRUE(temp1.Path().empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(temp2.Path(), file1);
  EXPECT_TRUE(PathExists(file1));
  EXPECT_FALSE(PathExists(file2));
}

TEST(ScopedTempFile, Destruct) {
  FilePath file;
  {
    ScopedTempFile temp;
    EXPECT_TRUE(temp.Create());
    file = temp.Path();
    EXPECT_TRUE(PathExists(file));
  }

  EXPECT_FALSE(PathExists(file));
}

TEST(ScopedTempFile, Reset) {
  ScopedTempFile temp_file;
  EXPECT_TRUE(temp_file.Path().empty());

  EXPECT_TRUE(temp_file.Create());
  EXPECT_TRUE(PathExists(temp_file.Path()));

  temp_file.Reset();
  EXPECT_FALSE(PathExists(temp_file.Path()));
}

TEST(ScopedTempFile, OperatorBool) {
  ScopedTempFile temp_file;
  EXPECT_FALSE(temp_file);

  EXPECT_TRUE(temp_file.Create());
  EXPECT_TRUE(temp_file);

  EXPECT_TRUE(temp_file.Delete());
  EXPECT_FALSE(temp_file);
}

}  // namespace kiwi