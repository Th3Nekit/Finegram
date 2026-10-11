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

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_TEST_PART_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_TEST_PART_H_

#include <iosfwd>
#include <ostream>
#include <string>
#include <vector>

#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-string.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

namespace testing {

class GTEST_API_ TestPartResult {
 public:

  enum Type {
    kSuccess,
    kNonFatalFailure,
    kFatalFailure,
    kSkip
  };

  TestPartResult(Type a_type, const char* a_file_name, int a_line_number,
                 const char* a_message)
      : type_(a_type),
        file_name_(a_file_name == nullptr ? "" : a_file_name),
        line_number_(a_line_number),
        summary_(ExtractSummary(a_message)),
        message_(a_message) {}

  Type type() const { return type_; }

  const char* file_name() const {
    return file_name_.empty() ? nullptr : file_name_.c_str();
  }

  int line_number() const { return line_number_; }

  const char* summary() const { return summary_.c_str(); }

  const char* message() const { return message_.c_str(); }

  bool skipped() const { return type_ == kSkip; }

  bool passed() const { return type_ == kSuccess; }

  bool nonfatally_failed() const { return type_ == kNonFatalFailure; }

  bool fatally_failed() const { return type_ == kFatalFailure; }

  bool failed() const { return fatally_failed() || nonfatally_failed(); }

 private:
  Type type_;

  static std::string ExtractSummary(const char* message);

  std::string file_name_;

  int line_number_;
  std::string summary_;
  std::string message_;
};

std::ostream& operator<<(std::ostream& os, const TestPartResult& result);

class GTEST_API_ TestPartResultArray {
 public:
  TestPartResultArray() = default;

  void Append(const TestPartResult& result);

  const TestPartResult& GetTestPartResult(int index) const;

  int size() const;

 private:
  std::vector<TestPartResult> array_;

  TestPartResultArray(const TestPartResultArray&) = delete;
  TestPartResultArray& operator=(const TestPartResultArray&) = delete;
};

class GTEST_API_ TestPartResultReporterInterface {
 public:
  virtual ~TestPartResultReporterInterface() = default;

  virtual void ReportTestPartResult(const TestPartResult& result) = 0;
};

namespace internal {

class GTEST_API_ HasNewFatalFailureHelper
    : public TestPartResultReporterInterface {
 public:
  HasNewFatalFailureHelper();
  ~HasNewFatalFailureHelper() override;
  void ReportTestPartResult(const TestPartResult& result) override;
  bool has_new_fatal_failure() const { return has_new_fatal_failure_; }

 private:
  bool has_new_fatal_failure_;
  TestPartResultReporterInterface* original_reporter_;

  HasNewFatalFailureHelper(const HasNewFatalFailureHelper&) = delete;
  HasNewFatalFailureHelper& operator=(const HasNewFatalFailureHelper&) = delete;
};

}

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
