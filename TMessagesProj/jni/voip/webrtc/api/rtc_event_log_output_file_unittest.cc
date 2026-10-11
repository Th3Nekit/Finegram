/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_event_log_output_file.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

class RtcEventLogOutputFileTest : public ::testing::Test {
 public:
  RtcEventLogOutputFileTest() : output_file_name_(GetOutputFilePath()) {

    remove(output_file_name_.c_str());
  }

  ~RtcEventLogOutputFileTest() override { remove(output_file_name_.c_str()); }

 protected:
  std::string GetOutputFilePath() const {
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    return test::OutputPathWithRandomDirectory() + test_info->test_case_name() +
           test_info->name();
  }

  std::string GetOutputFileContents() const {
    std::ifstream file(output_file_name_,
                       std::ios_base::in | std::ios_base::binary);
    RTC_CHECK(file.is_open());
    RTC_CHECK(file.good());
    std::string file_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return file_str;
  }

  const std::string output_file_name_;
};

TEST_F(RtcEventLogOutputFileTest, NonDefectiveOutputsStartOutActive) {
  auto output_file = std::make_unique<RtcEventLogOutputFile>(output_file_name_);
  EXPECT_TRUE(output_file->IsActive());
}

TEST_F(RtcEventLogOutputFileTest, DefectiveOutputsStartOutInactive) {
  const std::string illegal_filename = "/////////";
  auto output_file = std::make_unique<RtcEventLogOutputFile>(illegal_filename);
  EXPECT_FALSE(output_file->IsActive());
}

TEST_F(RtcEventLogOutputFileTest, UnlimitedOutputFile) {
  const std::string output_str = "one two three";

  auto output_file = std::make_unique<RtcEventLogOutputFile>(output_file_name_);
  output_file->Write(output_str);
  output_file.reset();

  EXPECT_EQ(GetOutputFileContents(), output_str);
}

TEST_F(RtcEventLogOutputFileTest, LimitedOutputFileCappedToCapacity) {

  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, 2);

  output_file->Write("1");
  output_file->Write("2");
  output_file->Write("3");

  EXPECT_EQ(GetOutputFileContents(), "12");
}

TEST_F(RtcEventLogOutputFileTest, DoNotWritePartialLines) {
  const std::string output_str_1 = "0123456789";
  const std::string output_str_2 = "abcdefghij";

  const size_t size_limit = output_str_1.length() + output_str_2.length() - 1;
  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, size_limit);

  output_file->Write(output_str_1);
  output_file->Write(output_str_2);

  EXPECT_EQ(GetOutputFileContents(), output_str_1);
}

TEST_F(RtcEventLogOutputFileTest, UnsuccessfulWriteReturnsFalse) {
  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, 2);
  EXPECT_FALSE(output_file->Write("abc"));
}

TEST_F(RtcEventLogOutputFileTest, SuccessfulWriteReturnsTrue) {
  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, 3);
  EXPECT_TRUE(output_file->Write("abc"));
}

TEST_F(RtcEventLogOutputFileTest, FileStillActiveAfterSuccessfulWrite) {
  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, 3);
  ASSERT_TRUE(output_file->Write("abc"));
  EXPECT_TRUE(output_file->IsActive());
}

TEST_F(RtcEventLogOutputFileTest, FileInactiveAfterUnsuccessfulWrite) {
  auto output_file =
      std::make_unique<RtcEventLogOutputFile>(output_file_name_, 2);
  ASSERT_FALSE(output_file->Write("abc"));
  EXPECT_FALSE(output_file->IsActive());
}

TEST_F(RtcEventLogOutputFileTest, AllowReasonableFileSizeLimits) {
  auto output_file = std::make_unique<RtcEventLogOutputFile>(
      output_file_name_, RtcEventLogOutputFile::kMaxReasonableFileSize);
  EXPECT_TRUE(output_file->IsActive());
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
class RtcEventLogOutputFileDeathTest : public RtcEventLogOutputFileTest {};

TEST_F(RtcEventLogOutputFileDeathTest, WritingToInactiveFileForbidden) {
  RtcEventLogOutputFile output_file(output_file_name_, 2);
  ASSERT_FALSE(output_file.Write("abc"));
  ASSERT_FALSE(output_file.IsActive());
  EXPECT_DEATH(output_file.Write("abc"), "");
}

TEST_F(RtcEventLogOutputFileDeathTest, DisallowUnreasonableFileSizeLimits) {

  std::unique_ptr<RtcEventLogOutputFile> output_file;
  auto create_output_file = [&] {
    const size_t unreasonable_size =
        RtcEventLogOutputFile::kMaxReasonableFileSize + 1;
    output_file = std::make_unique<RtcEventLogOutputFile>(output_file_name_,
                                                          unreasonable_size);
  };
  EXPECT_DEATH(create_output_file(), "");
}
#endif

}
