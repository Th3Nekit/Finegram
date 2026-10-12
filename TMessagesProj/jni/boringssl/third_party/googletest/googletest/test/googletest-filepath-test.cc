// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Google Test filepath utilities
//
// This file tests classes and functions used internally by
// Google Test.  They are subject to change without notice.
//
// This file is #included from gtest-internal.h.
// Do not #include this file anywhere else!

#include <string>

#include "gtest/gtest.h"
#include "gtest/internal/gtest-filepath.h"
#include "src/gtest-internal-inl.h"

#ifdef GTEST_OS_WINDOWS_MOBILE
#include <windows.h>
#elif defined(GTEST_OS_WINDOWS)
#include <direct.h>
#endif

namespace testing {
namespace internal {
namespace {

#ifdef GTEST_OS_WINDOWS_MOBILE

int remove(const char* path) {
  LPCWSTR wpath = String::AnsiToUtf16(path);
  int ret = DeleteFile(wpath) ? 0 : -1;
  delete[] wpath;
  return ret;
}

int _rmdir(const char* path) {
  FilePath filepath(path);
  LPCWSTR wpath =
      String::AnsiToUtf16(filepath.RemoveTrailingPathSeparator().c_str());
  int ret = RemoveDirectory(wpath) ? 0 : -1;
  delete[] wpath;
  return ret;
}

#else

TEST(GetCurrentDirTest, ReturnsCurrentDir) {
  const FilePath original_dir = FilePath::GetCurrentDir();
  EXPECT_FALSE(original_dir.IsEmpty());

  posix::ChDir(GTEST_PATH_SEP_);
  const FilePath cwd = FilePath::GetCurrentDir();
  posix::ChDir(original_dir.c_str());

#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_OS2)

  const char* const cwd_without_drive = strchr(cwd.c_str(), ':');
  ASSERT_TRUE(cwd_without_drive != NULL);
  EXPECT_STREQ(GTEST_PATH_SEP_, cwd_without_drive + 1);

#else

  EXPECT_EQ(GTEST_PATH_SEP_, cwd.string());

#endif
}

#endif

TEST(IsEmptyTest, ReturnsTrueForEmptyPath) {
  EXPECT_TRUE(FilePath("").IsEmpty());
}

TEST(IsEmptyTest, ReturnsFalseForNonEmptyPath) {
  EXPECT_FALSE(FilePath("a").IsEmpty());
  EXPECT_FALSE(FilePath(".").IsEmpty());
  EXPECT_FALSE(FilePath("a/b").IsEmpty());
  EXPECT_FALSE(FilePath("a\\b\\").IsEmpty());
}

TEST(RemoveDirectoryNameTest, WhenEmptyName) {
  EXPECT_EQ("", FilePath("").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, ButNoDirectory) {
  EXPECT_EQ("afile", FilePath("afile").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, RootFileShouldGiveFileName) {
  EXPECT_EQ("afile",
            FilePath(GTEST_PATH_SEP_ "afile").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, WhereThereIsNoFileName) {
  EXPECT_EQ("",
            FilePath("adir" GTEST_PATH_SEP_).RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, ShouldGiveFileName) {
  EXPECT_EQ(
      "afile",
      FilePath("adir" GTEST_PATH_SEP_ "afile").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, ShouldAlsoGiveFileName) {
  EXPECT_EQ("afile",
            FilePath("adir" GTEST_PATH_SEP_ "subdir" GTEST_PATH_SEP_ "afile")
                .RemoveDirectoryName()
                .string());
}

#if GTEST_HAS_ALT_PATH_SEP_

TEST(RemoveDirectoryNameTest, RootFileShouldGiveFileNameForAlternateSeparator) {
  EXPECT_EQ("afile", FilePath("/afile").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, WhereThereIsNoFileNameForAlternateSeparator) {
  EXPECT_EQ("", FilePath("adir/").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, ShouldGiveFileNameForAlternateSeparator) {
  EXPECT_EQ("afile", FilePath("adir/afile").RemoveDirectoryName().string());
}

TEST(RemoveDirectoryNameTest, ShouldAlsoGiveFileNameForAlternateSeparator) {
  EXPECT_EQ("afile",
            FilePath("adir/subdir/afile").RemoveDirectoryName().string());
}

#endif

TEST(RemoveFileNameTest, EmptyName) {
#ifdef GTEST_OS_WINDOWS_MOBILE

  EXPECT_EQ(GTEST_PATH_SEP_, FilePath("").RemoveFileName().string());
#else
  EXPECT_EQ("." GTEST_PATH_SEP_, FilePath("").RemoveFileName().string());
#endif
}

TEST(RemoveFileNameTest, ButNoFile) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_,
            FilePath("adir" GTEST_PATH_SEP_).RemoveFileName().string());
}

TEST(RemoveFileNameTest, GivesDirName) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_,
            FilePath("adir" GTEST_PATH_SEP_ "afile").RemoveFileName().string());
}

TEST(RemoveFileNameTest, GivesDirAndSubDirName) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_ "subdir" GTEST_PATH_SEP_,
            FilePath("adir" GTEST_PATH_SEP_ "subdir" GTEST_PATH_SEP_ "afile")
                .RemoveFileName()
                .string());
}

TEST(RemoveFileNameTest, GivesRootDir) {
  EXPECT_EQ(GTEST_PATH_SEP_,
            FilePath(GTEST_PATH_SEP_ "afile").RemoveFileName().string());
}

#if GTEST_HAS_ALT_PATH_SEP_

TEST(RemoveFileNameTest, ButNoFileForAlternateSeparator) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_,
            FilePath("adir/").RemoveFileName().string());
}

TEST(RemoveFileNameTest, GivesDirNameForAlternateSeparator) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_,
            FilePath("adir/afile").RemoveFileName().string());
}

TEST(RemoveFileNameTest, GivesDirAndSubDirNameForAlternateSeparator) {
  EXPECT_EQ("adir" GTEST_PATH_SEP_ "subdir" GTEST_PATH_SEP_,
            FilePath("adir/subdir/afile").RemoveFileName().string());
}

TEST(RemoveFileNameTest, GivesRootDirForAlternateSeparator) {
  EXPECT_EQ(GTEST_PATH_SEP_, FilePath("/afile").RemoveFileName().string());
}

#endif

TEST(MakeFileNameTest, GenerateWhenNumberIsZero) {
  FilePath actual =
      FilePath::MakeFileName(FilePath("foo"), FilePath("bar"), 0, "xml");
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar.xml", actual.string());
}

TEST(MakeFileNameTest, GenerateFileNameNumberGtZero) {
  FilePath actual =
      FilePath::MakeFileName(FilePath("foo"), FilePath("bar"), 12, "xml");
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar_12.xml", actual.string());
}

TEST(MakeFileNameTest, GenerateFileNameWithSlashNumberIsZero) {
  FilePath actual = FilePath::MakeFileName(FilePath("foo" GTEST_PATH_SEP_),
                                           FilePath("bar"), 0, "xml");
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar.xml", actual.string());
}

TEST(MakeFileNameTest, GenerateFileNameWithSlashNumberGtZero) {
  FilePath actual = FilePath::MakeFileName(FilePath("foo" GTEST_PATH_SEP_),
                                           FilePath("bar"), 12, "xml");
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar_12.xml", actual.string());
}

TEST(MakeFileNameTest, GenerateWhenNumberIsZeroAndDirIsEmpty) {
  FilePath actual =
      FilePath::MakeFileName(FilePath(""), FilePath("bar"), 0, "xml");
  EXPECT_EQ("bar.xml", actual.string());
}

TEST(MakeFileNameTest, GenerateWhenNumberIsNotZeroAndDirIsEmpty) {
  FilePath actual =
      FilePath::MakeFileName(FilePath(""), FilePath("bar"), 14, "xml");
  EXPECT_EQ("bar_14.xml", actual.string());
}

TEST(ConcatPathsTest, WorksWhenDirDoesNotEndWithPathSep) {
  FilePath actual = FilePath::ConcatPaths(FilePath("foo"), FilePath("bar.xml"));
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar.xml", actual.string());
}

TEST(ConcatPathsTest, WorksWhenPath1EndsWithPathSep) {
  FilePath actual = FilePath::ConcatPaths(FilePath("foo" GTEST_PATH_SEP_),
                                          FilePath("bar.xml"));
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar.xml", actual.string());
}

TEST(ConcatPathsTest, Path1BeingEmpty) {
  FilePath actual = FilePath::ConcatPaths(FilePath(""), FilePath("bar.xml"));
  EXPECT_EQ("bar.xml", actual.string());
}

TEST(ConcatPathsTest, Path2BeingEmpty) {
  FilePath actual = FilePath::ConcatPaths(FilePath("foo"), FilePath(""));
  EXPECT_EQ("foo" GTEST_PATH_SEP_, actual.string());
}

TEST(ConcatPathsTest, BothPathBeingEmpty) {
  FilePath actual = FilePath::ConcatPaths(FilePath(""), FilePath(""));
  EXPECT_EQ("", actual.string());
}

TEST(ConcatPathsTest, Path1ContainsPathSep) {
  FilePath actual = FilePath::ConcatPaths(FilePath("foo" GTEST_PATH_SEP_ "bar"),
                                          FilePath("foobar.xml"));
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar" GTEST_PATH_SEP_ "foobar.xml",
            actual.string());
}

TEST(ConcatPathsTest, Path2ContainsPathSep) {
  FilePath actual =
      FilePath::ConcatPaths(FilePath("foo" GTEST_PATH_SEP_),
                            FilePath("bar" GTEST_PATH_SEP_ "bar.xml"));
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar" GTEST_PATH_SEP_ "bar.xml",
            actual.string());
}

TEST(ConcatPathsTest, Path2EndsWithPathSep) {
  FilePath actual =
      FilePath::ConcatPaths(FilePath("foo"), FilePath("bar" GTEST_PATH_SEP_));
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar" GTEST_PATH_SEP_, actual.string());
}

TEST(RemoveTrailingPathSeparatorTest, EmptyString) {
  EXPECT_EQ("", FilePath("").RemoveTrailingPathSeparator().string());
}

TEST(RemoveTrailingPathSeparatorTest, FileNoSlashString) {
  EXPECT_EQ("foo", FilePath("foo").RemoveTrailingPathSeparator().string());
}

TEST(RemoveTrailingPathSeparatorTest, ShouldRemoveTrailingSeparator) {
  EXPECT_EQ(
      "foo",
      FilePath("foo" GTEST_PATH_SEP_).RemoveTrailingPathSeparator().string());
#if GTEST_HAS_ALT_PATH_SEP_
  EXPECT_EQ("foo", FilePath("foo/").RemoveTrailingPathSeparator().string());
#endif
}

TEST(RemoveTrailingPathSeparatorTest, ShouldRemoveLastSeparator) {
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar",
            FilePath("foo" GTEST_PATH_SEP_ "bar" GTEST_PATH_SEP_)
                .RemoveTrailingPathSeparator()
                .string());
}

TEST(RemoveTrailingPathSeparatorTest, ShouldReturnUnmodified) {
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar", FilePath("foo" GTEST_PATH_SEP_ "bar")
                                             .RemoveTrailingPathSeparator()
                                             .string());
}

TEST(DirectoryTest, RootDirectoryExists) {
#ifdef GTEST_OS_WINDOWS
  char current_drive[_MAX_PATH];
  current_drive[0] = static_cast<char>(_getdrive() + 'A' - 1);
  current_drive[1] = ':';
  current_drive[2] = '\\';
  current_drive[3] = '\0';
  EXPECT_TRUE(FilePath(current_drive).DirectoryExists());
#else
  EXPECT_TRUE(FilePath("/").DirectoryExists());
#endif
}

#ifdef GTEST_OS_WINDOWS
TEST(DirectoryTest, RootOfWrongDriveDoesNotExists) {
  const int saved_drive_ = _getdrive();

  for (char drive = 'Z'; drive >= 'A'; drive--)
    if (_chdrive(drive - 'A' + 1) == -1) {
      char non_drive[_MAX_PATH];
      non_drive[0] = drive;
      non_drive[1] = ':';
      non_drive[2] = '\\';
      non_drive[3] = '\0';
      EXPECT_FALSE(FilePath(non_drive).DirectoryExists());
      break;
    }
  _chdrive(saved_drive_);
}
#endif

#ifndef GTEST_OS_WINDOWS_MOBILE

TEST(DirectoryTest, EmptyPathDirectoryDoesNotExist) {
  EXPECT_FALSE(FilePath("").DirectoryExists());
}
#endif

TEST(DirectoryTest, CurrentDirectoryExists) {
#ifdef GTEST_OS_WINDOWS
#ifndef _WIN32_CE

  EXPECT_TRUE(FilePath(".").DirectoryExists());
  EXPECT_TRUE(FilePath(".\\").DirectoryExists());

#endif
#else
  EXPECT_TRUE(FilePath(".").DirectoryExists());
  EXPECT_TRUE(FilePath("./").DirectoryExists());
#endif
}

TEST(NormalizeTest, MultipleConsecutiveSeparatorsInMidstring) {
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar",
            FilePath("foo" GTEST_PATH_SEP_ "bar").string());
  EXPECT_EQ("foo" GTEST_PATH_SEP_ "bar",
            FilePath("foo" GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar").string());
  EXPECT_EQ(
      "foo" GTEST_PATH_SEP_ "bar",
      FilePath("foo" GTEST_PATH_SEP_ GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar")
          .string());
}

TEST(NormalizeTest, MultipleConsecutiveSeparatorsAtStringStart) {
  EXPECT_EQ(GTEST_PATH_SEP_ "bar", FilePath(GTEST_PATH_SEP_ "bar").string());
#ifdef GTEST_OS_WINDOWS
  EXPECT_EQ(GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar",
            FilePath(GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar").string());
#else
  EXPECT_EQ(GTEST_PATH_SEP_ "bar",
            FilePath(GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar").string());
#endif
  EXPECT_EQ(
      GTEST_PATH_SEP_ "bar",
      FilePath(GTEST_PATH_SEP_ GTEST_PATH_SEP_ GTEST_PATH_SEP_ "bar").string());
}

TEST(NormalizeTest, MultipleConsecutiveSeparatorsAtStringEnd) {
  EXPECT_EQ("foo" GTEST_PATH_SEP_, FilePath("foo" GTEST_PATH_SEP_).string());
  EXPECT_EQ("foo" GTEST_PATH_SEP_,
            FilePath("foo" GTEST_PATH_SEP_ GTEST_PATH_SEP_).string());
  EXPECT_EQ(
      "foo" GTEST_PATH_SEP_,
      FilePath("foo" GTEST_PATH_SEP_ GTEST_PATH_SEP_ GTEST_PATH_SEP_).string());
}

#if GTEST_HAS_ALT_PATH_SEP_

TEST(NormalizeTest, MixAlternateSeparatorAtStringEnd) {
  EXPECT_EQ("foo" GTEST_PATH_SEP_, FilePath("foo/").string());
  EXPECT_EQ("foo" GTEST_PATH_SEP_,
            FilePath("foo" GTEST_PATH_SEP_ "/").string());
  EXPECT_EQ("foo" GTEST_PATH_SEP_, FilePath("foo//" GTEST_PATH_SEP_).string());
}

#endif

TEST(AssignmentOperatorTest, DefaultAssignedToNonDefault) {
  FilePath default_path;
  FilePath non_default_path("path");
  non_default_path = default_path;
  EXPECT_EQ("", non_default_path.string());
  EXPECT_EQ("", default_path.string());
}

TEST(AssignmentOperatorTest, NonDefaultAssignedToDefault) {
  FilePath non_default_path("path");
  FilePath default_path;
  default_path = non_default_path;
  EXPECT_EQ("path", default_path.string());
  EXPECT_EQ("path", non_default_path.string());
}

TEST(AssignmentOperatorTest, ConstAssignedToNonConst) {
  const FilePath const_default_path("const_path");
  FilePath non_default_path("path");
  non_default_path = const_default_path;
  EXPECT_EQ("const_path", non_default_path.string());
}

class DirectoryCreationTest : public Test {
 protected:
  void SetUp() override {
    testdata_path_.Set(
        FilePath(TempDir() + GetCurrentExecutableName().string() +
                 "_directory_creation" GTEST_PATH_SEP_ "test" GTEST_PATH_SEP_));
    testdata_file_.Set(testdata_path_.RemoveTrailingPathSeparator());

    unique_file0_.Set(
        FilePath::MakeFileName(testdata_path_, FilePath("unique"), 0, "txt"));
    unique_file1_.Set(
        FilePath::MakeFileName(testdata_path_, FilePath("unique"), 1, "txt"));

    remove(testdata_file_.c_str());
    remove(unique_file0_.c_str());
    remove(unique_file1_.c_str());
    posix::RmDir(testdata_path_.c_str());
  }

  void TearDown() override {
    remove(testdata_file_.c_str());
    remove(unique_file0_.c_str());
    remove(unique_file1_.c_str());
    posix::RmDir(testdata_path_.c_str());
  }

  void CreateTextFile(const char* filename) {
    FILE* f = posix::FOpen(filename, "w");
    fprintf(f, "text\n");
    fclose(f);
  }

  FilePath testdata_path_;
  FilePath testdata_file_;
  FilePath unique_file0_;
  FilePath unique_file1_;
};

TEST_F(DirectoryCreationTest, CreateDirectoriesRecursively) {
  EXPECT_FALSE(testdata_path_.DirectoryExists()) << testdata_path_.string();
  EXPECT_TRUE(testdata_path_.CreateDirectoriesRecursively());
  EXPECT_TRUE(testdata_path_.DirectoryExists());
}

TEST_F(DirectoryCreationTest, CreateDirectoriesForAlreadyExistingPath) {
  EXPECT_FALSE(testdata_path_.DirectoryExists()) << testdata_path_.string();
  EXPECT_TRUE(testdata_path_.CreateDirectoriesRecursively());

  EXPECT_TRUE(testdata_path_.CreateDirectoriesRecursively());
}

TEST_F(DirectoryCreationTest, CreateDirectoriesAndUniqueFilename) {
  FilePath file_path(FilePath::GenerateUniqueFileName(
      testdata_path_, FilePath("unique"), "txt"));
  EXPECT_EQ(unique_file0_.string(), file_path.string());
  EXPECT_FALSE(file_path.FileOrDirectoryExists());

  testdata_path_.CreateDirectoriesRecursively();
  EXPECT_FALSE(file_path.FileOrDirectoryExists());
  CreateTextFile(file_path.c_str());
  EXPECT_TRUE(file_path.FileOrDirectoryExists());

  FilePath file_path2(FilePath::GenerateUniqueFileName(
      testdata_path_, FilePath("unique"), "txt"));
  EXPECT_EQ(unique_file1_.string(), file_path2.string());
  EXPECT_FALSE(file_path2.FileOrDirectoryExists());
  CreateTextFile(file_path2.c_str());
  EXPECT_TRUE(file_path2.FileOrDirectoryExists());
}

TEST_F(DirectoryCreationTest, CreateDirectoriesFail) {

  CreateTextFile(testdata_file_.c_str());
  EXPECT_TRUE(testdata_file_.FileOrDirectoryExists());
  EXPECT_FALSE(testdata_file_.DirectoryExists());
  EXPECT_FALSE(testdata_file_.CreateDirectoriesRecursively());
}

TEST(NoDirectoryCreationTest, CreateNoDirectoriesForDefaultXmlFile) {
  const FilePath test_detail_xml("test_detail.xml");
  EXPECT_FALSE(test_detail_xml.CreateDirectoriesRecursively());
}

TEST(FilePathTest, DefaultConstructor) {
  FilePath fp;
  EXPECT_EQ("", fp.string());
}

TEST(FilePathTest, CharAndCopyConstructors) {
  const FilePath fp("spicy");
  EXPECT_EQ("spicy", fp.string());

  const FilePath fp_copy(fp);
  EXPECT_EQ("spicy", fp_copy.string());
}

TEST(FilePathTest, StringConstructor) {
  const FilePath fp(std::string("cider"));
  EXPECT_EQ("cider", fp.string());
}

TEST(FilePathTest, Set) {
  const FilePath apple("apple");
  FilePath mac("mac");
  mac.Set(apple);
  EXPECT_EQ("apple", mac.string());
  EXPECT_EQ("apple", apple.string());
}

TEST(FilePathTest, ToString) {
  const FilePath file("drink");
  EXPECT_EQ("drink", file.string());
}

TEST(FilePathTest, RemoveExtension) {
  EXPECT_EQ("app", FilePath("app.cc").RemoveExtension("cc").string());
  EXPECT_EQ("app", FilePath("app.exe").RemoveExtension("exe").string());
  EXPECT_EQ("APP", FilePath("APP.EXE").RemoveExtension("exe").string());
}

TEST(FilePathTest, RemoveExtensionWhenThereIsNoExtension) {
  EXPECT_EQ("app", FilePath("app").RemoveExtension("exe").string());
}

TEST(FilePathTest, IsDirectory) {
  EXPECT_FALSE(FilePath("cola").IsDirectory());
  EXPECT_TRUE(FilePath("koala" GTEST_PATH_SEP_).IsDirectory());
#if GTEST_HAS_ALT_PATH_SEP_
  EXPECT_TRUE(FilePath("koala/").IsDirectory());
#endif
}

TEST(FilePathTest, IsAbsolutePath) {
  EXPECT_FALSE(FilePath("is" GTEST_PATH_SEP_ "relative").IsAbsolutePath());
  EXPECT_FALSE(FilePath("").IsAbsolutePath());
#ifdef GTEST_OS_WINDOWS
  EXPECT_TRUE(
      FilePath("c:\\" GTEST_PATH_SEP_ "is_not" GTEST_PATH_SEP_ "relative")
          .IsAbsolutePath());
  EXPECT_FALSE(FilePath("c:foo" GTEST_PATH_SEP_ "bar").IsAbsolutePath());
  EXPECT_TRUE(
      FilePath("c:/" GTEST_PATH_SEP_ "is_not" GTEST_PATH_SEP_ "relative")
          .IsAbsolutePath());
  EXPECT_TRUE(FilePath("d:/Windows").IsAbsolutePath());
  EXPECT_TRUE(FilePath("\\\\Host\\Share").IsAbsolutePath());
  EXPECT_TRUE(FilePath("\\\\Host\\Share\\Folder").IsAbsolutePath());
#else
  EXPECT_TRUE(FilePath(GTEST_PATH_SEP_ "is_not" GTEST_PATH_SEP_ "relative")
                  .IsAbsolutePath());
#endif
}

TEST(FilePathTest, IsRootDirectory) {
#ifdef GTEST_OS_WINDOWS
  EXPECT_TRUE(FilePath("a:\\").IsRootDirectory());
  EXPECT_TRUE(FilePath("Z:/").IsRootDirectory());
  EXPECT_TRUE(FilePath("e://").IsRootDirectory());
  EXPECT_FALSE(FilePath("").IsRootDirectory());
  EXPECT_FALSE(FilePath("b:").IsRootDirectory());
  EXPECT_FALSE(FilePath("b:a").IsRootDirectory());
  EXPECT_FALSE(FilePath("8:/").IsRootDirectory());
  EXPECT_FALSE(FilePath("c|/").IsRootDirectory());
  EXPECT_TRUE(FilePath("c:/").IsRootDirectory());
  EXPECT_FALSE(FilePath("d:/Windows").IsRootDirectory());

  EXPECT_FALSE(FilePath("\\\\Host\\Share").IsRootDirectory());

  EXPECT_TRUE(FilePath("\\\\Host\\Share\\").IsRootDirectory());
  EXPECT_FALSE(FilePath("\\\\Host\\Share\\.").IsRootDirectory());
  EXPECT_FALSE(FilePath("\\\\Host\\Share\\C$\\").IsRootDirectory());
#else
  EXPECT_TRUE(FilePath("/").IsRootDirectory());
  EXPECT_TRUE(FilePath("//").IsRootDirectory());
  EXPECT_FALSE(FilePath("").IsRootDirectory());
  EXPECT_FALSE(FilePath("\\").IsRootDirectory());
  EXPECT_FALSE(FilePath("/x").IsRootDirectory());
#endif
}

}
}
}
