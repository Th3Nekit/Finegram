// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file makes extensive use of RFC 3092.  :)

#include <algorithm>

#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {

static void AddToDatabase(SimpleDescriptorDatabase* database,
                          const char* file_text) {
  FileDescriptorProto file_proto;
  EXPECT_TRUE(TextFormat::ParseFromString(file_text, &file_proto));
  database->Add(file_proto);
}

static void ExpectContainsType(const FileDescriptorProto& proto,
                               const string& type_name) {
  for (int i = 0; i < proto.message_type_size(); i++) {
    if (proto.message_type(i).name() == type_name) return;
  }
  ADD_FAILURE() << "\"" << proto.name()
                << "\" did not contain expected type \""
                << type_name << "\".";
}

#if GTEST_HAS_PARAM_TEST

class DescriptorDatabaseTestCase {
 public:
  virtual ~DescriptorDatabaseTestCase() {}

  virtual DescriptorDatabase* GetDatabase() = 0;
  virtual bool AddToDatabase(const FileDescriptorProto& file) = 0;
};

typedef DescriptorDatabaseTestCase* DescriptorDatabaseTestCaseFactory();

class SimpleDescriptorDatabaseTestCase : public DescriptorDatabaseTestCase {
 public:
  static DescriptorDatabaseTestCase* New() {
    return new SimpleDescriptorDatabaseTestCase;
  }

  virtual ~SimpleDescriptorDatabaseTestCase() {}

  virtual DescriptorDatabase* GetDatabase() {
    return &database_;
  }
  virtual bool AddToDatabase(const FileDescriptorProto& file) {
    return database_.Add(file);
  }

 private:
  SimpleDescriptorDatabase database_;
};

class EncodedDescriptorDatabaseTestCase : public DescriptorDatabaseTestCase {
 public:
  static DescriptorDatabaseTestCase* New() {
    return new EncodedDescriptorDatabaseTestCase;
  }

  virtual ~EncodedDescriptorDatabaseTestCase() {}

  virtual DescriptorDatabase* GetDatabase() {
    return &database_;
  }
  virtual bool AddToDatabase(const FileDescriptorProto& file) {
    string data;
    file.SerializeToString(&data);
    return database_.AddCopy(data.data(), data.size());
  }

 private:
  EncodedDescriptorDatabase database_;
};

class DescriptorPoolDatabaseTestCase : public DescriptorDatabaseTestCase {
 public:
  static DescriptorDatabaseTestCase* New() {
    return new EncodedDescriptorDatabaseTestCase;
  }

  DescriptorPoolDatabaseTestCase() : database_(pool_) {}
  virtual ~DescriptorPoolDatabaseTestCase() {}

  virtual DescriptorDatabase* GetDatabase() {
    return &database_;
  }
  virtual bool AddToDatabase(const FileDescriptorProto& file) {
    return pool_.BuildFile(file);
  }

 private:
  DescriptorPool pool_;
  DescriptorPoolDatabase database_;
};

class DescriptorDatabaseTest
    : public testing::TestWithParam<DescriptorDatabaseTestCaseFactory*> {
 protected:
  virtual void SetUp() {
    test_case_.reset(GetParam()());
    database_ = test_case_->GetDatabase();
  }

  void AddToDatabase(const char* file_descriptor_text) {
    FileDescriptorProto file_proto;
    EXPECT_TRUE(TextFormat::ParseFromString(file_descriptor_text, &file_proto));
    EXPECT_TRUE(test_case_->AddToDatabase(file_proto));
  }

  void AddToDatabaseWithError(const char* file_descriptor_text) {
    FileDescriptorProto file_proto;
    EXPECT_TRUE(TextFormat::ParseFromString(file_descriptor_text, &file_proto));
    EXPECT_FALSE(test_case_->AddToDatabase(file_proto));
  }

  scoped_ptr<DescriptorDatabaseTestCase> test_case_;
  DescriptorDatabase* database_;
};

TEST_P(DescriptorDatabaseTest, FindFileByName) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { name:\"Foo\" }");
  AddToDatabase(
    "name: \"bar.proto\" "
    "message_type { name:\"Bar\" }");

  {
    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileByName("foo.proto", &file));
    EXPECT_EQ("foo.proto", file.name());
    ExpectContainsType(file, "Foo");
  }

  {
    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileByName("bar.proto", &file));
    EXPECT_EQ("bar.proto", file.name());
    ExpectContainsType(file, "Bar");
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileByName("baz.proto", &file));
  }
}

TEST_P(DescriptorDatabaseTest, FindFileContainingSymbol) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "  field { name:\"qux\" }"
    "  nested_type { name: \"Grault\" } "
    "  enum_type { name: \"Garply\" } "
    "} "
    "enum_type { "
    "  name: \"Waldo\" "
    "  value { name:\"FRED\" } "
    "} "
    "extension { name: \"plugh\" } "
    "service { "
    "  name: \"Xyzzy\" "
    "  method { name: \"Thud\" } "
    "}"
    );
  AddToDatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "message_type { name: \"Bar\" }");

  {
    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Foo", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Foo.qux", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Foo.Grault", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Foo.Garply", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Waldo", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Waldo.FRED", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("plugh", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Xyzzy", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("Xyzzy.Thud", &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingSymbol("corge.Bar", &file));
    EXPECT_EQ("bar.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileContainingSymbol("Baz", &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileContainingSymbol("Bar", &file));
  }
}

TEST_P(DescriptorDatabaseTest, FindFileContainingExtension) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "  extension_range { start: 1 end: 1000 } "
    "  extension { name:\"qux\" label:LABEL_OPTIONAL type:TYPE_INT32 number:5 "
    "              extendee: \".Foo\" }"
    "}");
  AddToDatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "dependency: \"foo.proto\" "
    "message_type { "
    "  name: \"Bar\" "
    "  extension_range { start: 1 end: 1000 } "
    "} "
    "extension { name:\"grault\" extendee: \".Foo\"       number:32 } "
    "extension { name:\"garply\" extendee: \".corge.Bar\" number:70 } "
    "extension { name:\"waldo\"  extendee: \"Bar\"        number:56 } ");

  {
    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingExtension("Foo", 5, &file));
    EXPECT_EQ("foo.proto", file.name());
  }

  {
    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingExtension("Foo", 32, &file));
    EXPECT_EQ("bar.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(database_->FindFileContainingExtension("corge.Bar", 70, &file));
    EXPECT_EQ("bar.proto", file.name());
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileContainingExtension("Bar", 56, &file));
    EXPECT_FALSE(
        database_->FindFileContainingExtension("corge.Bar", 56, &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileContainingExtension("Foo", 12, &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(
        database_->FindFileContainingExtension("NoSuchType", 5, &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(database_->FindFileContainingExtension("Bar", 70, &file));
  }
}

TEST_P(DescriptorDatabaseTest, FindAllExtensionNumbers) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "  extension_range { start: 1 end: 1000 } "
    "  extension { name:\"qux\" label:LABEL_OPTIONAL type:TYPE_INT32 number:5 "
    "              extendee: \".Foo\" }"
    "}");
  AddToDatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "dependency: \"foo.proto\" "
    "message_type { "
    "  name: \"Bar\" "
    "  extension_range { start: 1 end: 1000 } "
    "} "
    "extension { name:\"grault\" extendee: \".Foo\"       number:32 } "
    "extension { name:\"garply\" extendee: \".corge.Bar\" number:70 } "
    "extension { name:\"waldo\"  extendee: \"Bar\"        number:56 } ");

  {
    vector<int> numbers;
    EXPECT_TRUE(database_->FindAllExtensionNumbers("Foo", &numbers));
    ASSERT_EQ(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    EXPECT_EQ(5, numbers[0]);
    EXPECT_EQ(32, numbers[1]);
  }

  {
    vector<int> numbers;
    EXPECT_TRUE(database_->FindAllExtensionNumbers("corge.Bar", &numbers));

    ASSERT_EQ(1, numbers.size());
    EXPECT_EQ(70, numbers[0]);
  }

  {

    vector<int> numbers;
    EXPECT_FALSE(database_->FindAllExtensionNumbers("NoSuchType", &numbers));
  }

  {

    vector<int> numbers;
    EXPECT_FALSE(database_->FindAllExtensionNumbers("Bar", &numbers));
  }
}

TEST_P(DescriptorDatabaseTest, ConflictingFileError) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "}");
  AddToDatabaseWithError(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Bar\" "
    "}");
}

TEST_P(DescriptorDatabaseTest, ConflictingTypeError) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "}");
  AddToDatabaseWithError(
    "name: \"bar.proto\" "
    "message_type { "
    "  name: \"Foo\" "
    "}");
}

TEST_P(DescriptorDatabaseTest, ConflictingExtensionError) {
  AddToDatabase(
    "name: \"foo.proto\" "
    "extension { name:\"foo\" label:LABEL_OPTIONAL type:TYPE_INT32 number:5 "
    "            extendee: \".Foo\" }");
  AddToDatabaseWithError(
    "name: \"bar.proto\" "
    "extension { name:\"bar\" label:LABEL_OPTIONAL type:TYPE_INT32 number:5 "
    "            extendee: \".Foo\" }");
}

INSTANTIATE_TEST_CASE_P(Simple, DescriptorDatabaseTest,
    testing::Values(&SimpleDescriptorDatabaseTestCase::New));
INSTANTIATE_TEST_CASE_P(MemoryConserving, DescriptorDatabaseTest,
    testing::Values(&EncodedDescriptorDatabaseTestCase::New));
INSTANTIATE_TEST_CASE_P(Pool, DescriptorDatabaseTest,
    testing::Values(&DescriptorPoolDatabaseTestCase::New));

#endif

TEST(EncodedDescriptorDatabaseExtraTest, FindNameOfFileContainingSymbol) {

  FileDescriptorProto file1, file2a, file2b;
  file1.set_name("foo.proto");
  file1.set_package("foo");
  file1.add_message_type()->set_name("Foo");
  file2a.set_name("bar.proto");
  file2b.set_package("bar");
  file2b.add_message_type()->set_name("Bar");

  string data1 = file1.SerializeAsString();

  string data2 = file2b.SerializeAsString() + file2a.SerializeAsString();

  EncodedDescriptorDatabase db;
  db.Add(data1.data(), data1.size());
  db.Add(data2.data(), data2.size());

  string filename;
  EXPECT_TRUE(db.FindNameOfFileContainingSymbol("foo.Foo", &filename));
  EXPECT_EQ("foo.proto", filename);
  EXPECT_TRUE(db.FindNameOfFileContainingSymbol("foo.Foo.Blah", &filename));
  EXPECT_EQ("foo.proto", filename);
  EXPECT_TRUE(db.FindNameOfFileContainingSymbol("bar.Bar", &filename));
  EXPECT_EQ("bar.proto", filename);
  EXPECT_FALSE(db.FindNameOfFileContainingSymbol("foo", &filename));
  EXPECT_FALSE(db.FindNameOfFileContainingSymbol("bar", &filename));
  EXPECT_FALSE(db.FindNameOfFileContainingSymbol("baz.Baz", &filename));
}

class MergedDescriptorDatabaseTest : public testing::Test {
 protected:
  MergedDescriptorDatabaseTest()
    : forward_merged_(&database1_, &database2_),
      reverse_merged_(&database2_, &database1_) {}

  virtual void SetUp() {
    AddToDatabase(&database1_,
      "name: \"foo.proto\" "
      "message_type { name:\"Foo\" extension_range { start: 1 end: 100 } } "
      "extension { name:\"foo_ext\" extendee: \".Foo\" number:3 "
      "            label:LABEL_OPTIONAL type:TYPE_INT32 } ");
    AddToDatabase(&database2_,
      "name: \"bar.proto\" "
      "message_type { name:\"Bar\" extension_range { start: 1 end: 100 } } "
      "extension { name:\"bar_ext\" extendee: \".Bar\" number:5 "
      "            label:LABEL_OPTIONAL type:TYPE_INT32 } ");

    AddToDatabase(&database1_,
      "name: \"baz.proto\" "
      "message_type { name:\"Baz\" extension_range { start: 1 end: 100 } } "
      "message_type { name:\"FromPool1\" } "
      "extension { name:\"baz_ext\" extendee: \".Baz\" number:12 "
      "            label:LABEL_OPTIONAL type:TYPE_INT32 } "
      "extension { name:\"database1_only_ext\" extendee: \".Baz\" number:13 "
      "            label:LABEL_OPTIONAL type:TYPE_INT32 } ");
    AddToDatabase(&database2_,
      "name: \"baz.proto\" "
      "message_type { name:\"Baz\" extension_range { start: 1 end: 100 } } "
      "message_type { name:\"FromPool2\" } "
      "extension { name:\"baz_ext\" extendee: \".Baz\" number:12 "
      "            label:LABEL_OPTIONAL type:TYPE_INT32 } ");
  }

  SimpleDescriptorDatabase database1_;
  SimpleDescriptorDatabase database2_;

  MergedDescriptorDatabase forward_merged_;
  MergedDescriptorDatabase reverse_merged_;
};

TEST_F(MergedDescriptorDatabaseTest, FindFileByName) {
  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileByName("foo.proto", &file));
    EXPECT_EQ("foo.proto", file.name());
    ExpectContainsType(file, "Foo");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileByName("bar.proto", &file));
    EXPECT_EQ("bar.proto", file.name());
    ExpectContainsType(file, "Bar");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileByName("baz.proto", &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool1");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(reverse_merged_.FindFileByName("baz.proto", &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool2");
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(forward_merged_.FindFileByName("no_such.proto", &file));
  }
}

TEST_F(MergedDescriptorDatabaseTest, FindFileContainingSymbol) {
  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileContainingSymbol("Foo", &file));
    EXPECT_EQ("foo.proto", file.name());
    ExpectContainsType(file, "Foo");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileContainingSymbol("Bar", &file));
    EXPECT_EQ("bar.proto", file.name());
    ExpectContainsType(file, "Bar");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileContainingSymbol("Baz", &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool1");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(reverse_merged_.FindFileContainingSymbol("Baz", &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool2");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileContainingSymbol("FromPool1", &file));
    EXPECT_FALSE(reverse_merged_.FindFileContainingSymbol("FromPool1", &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(
      forward_merged_.FindFileContainingSymbol("NoSuchType", &file));
  }
}

TEST_F(MergedDescriptorDatabaseTest, FindFileContainingExtension) {
  {

    FileDescriptorProto file;
    EXPECT_TRUE(
      forward_merged_.FindFileContainingExtension("Foo", 3, &file));
    EXPECT_EQ("foo.proto", file.name());
    ExpectContainsType(file, "Foo");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(
      forward_merged_.FindFileContainingExtension("Bar", 5, &file));
    EXPECT_EQ("bar.proto", file.name());
    ExpectContainsType(file, "Bar");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(
      forward_merged_.FindFileContainingExtension("Baz", 12, &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool1");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(
      reverse_merged_.FindFileContainingExtension("Baz", 12, &file));
    EXPECT_EQ("baz.proto", file.name());
    ExpectContainsType(file, "FromPool2");
  }

  {

    FileDescriptorProto file;
    EXPECT_TRUE(forward_merged_.FindFileContainingExtension("Baz", 13, &file));
    EXPECT_FALSE(reverse_merged_.FindFileContainingExtension("Baz", 13, &file));
  }

  {

    FileDescriptorProto file;
    EXPECT_FALSE(
      forward_merged_.FindFileContainingExtension("Foo", 6, &file));
  }
}

TEST_F(MergedDescriptorDatabaseTest, FindAllExtensionNumbers) {
  {

    vector<int> numbers;
    EXPECT_TRUE(forward_merged_.FindAllExtensionNumbers("Foo", &numbers));
    ASSERT_EQ(1, numbers.size());
    EXPECT_EQ(3, numbers[0]);
  }

  {

    vector<int> numbers;
    EXPECT_TRUE(forward_merged_.FindAllExtensionNumbers("Bar", &numbers));
    ASSERT_EQ(1, numbers.size());
    EXPECT_EQ(5, numbers[0]);
  }

  {

    vector<int> numbers;
    EXPECT_TRUE(forward_merged_.FindAllExtensionNumbers("Baz", &numbers));
    ASSERT_EQ(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    EXPECT_EQ(12, numbers[0]);
    EXPECT_EQ(13, numbers[1]);
  }

  {
    vector<int> numbers;
    EXPECT_TRUE(reverse_merged_.FindAllExtensionNumbers("Baz", &numbers));
    ASSERT_EQ(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    EXPECT_EQ(12, numbers[0]);
    EXPECT_EQ(13, numbers[1]);
  }

  {

    vector<int> numbers;
    EXPECT_FALSE(reverse_merged_.FindAllExtensionNumbers("Blah", &numbers));
  }
}

}
}
}
