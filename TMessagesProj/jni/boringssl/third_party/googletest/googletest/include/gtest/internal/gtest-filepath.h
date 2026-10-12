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

// Google Test filepath utilities
//
// This header file declares classes and functions used internally by
// Google Test.  They are subject to change without notice.
//
// This file is #included in gtest/internal/gtest-internal.h.
// Do not include this header file separately!

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_FILEPATH_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_FILEPATH_H_

#include <string>
#include <utility>

#include "gtest/internal/gtest-port.h"
#include "gtest/internal/gtest-string.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

#if GTEST_HAS_FILE_SYSTEM

namespace testing {
namespace internal {

class GTEST_API_ FilePath {
 public:
  FilePath() : pathname_("") {}
  FilePath(const FilePath& rhs) : pathname_(rhs.pathname_) {}
  FilePath(FilePath&& rhs) noexcept : pathname_(std::move(rhs.pathname_)) {}

  explicit FilePath(std::string pathname) : pathname_(std::move(pathname)) {
    Normalize();
  }

  FilePath& operator=(const FilePath& rhs) {
    Set(rhs);
    return *this;
  }
  FilePath& operator=(FilePath&& rhs) noexcept {
    pathname_ = std::move(rhs.pathname_);
    return *this;
  }

  void Set(const FilePath& rhs) { pathname_ = rhs.pathname_; }

  const std::string& string() const { return pathname_; }
  const char* c_str() const { return pathname_.c_str(); }

  static FilePath GetCurrentDir();

  static FilePath MakeFileName(const FilePath& directory,
                               const FilePath& base_name, int number,
                               const char* extension);

  static FilePath ConcatPaths(const FilePath& directory,
                              const FilePath& relative_path);

  static FilePath GenerateUniqueFileName(const FilePath& directory,
                                         const FilePath& base_name,
                                         const char* extension);

  bool IsEmpty() const { return pathname_.empty(); }

  FilePath RemoveTrailingPathSeparator() const;

  FilePath RemoveDirectoryName() const;

  FilePath RemoveFileName() const;

  FilePath RemoveExtension(const char* extension) const;

  bool CreateDirectoriesRecursively() const;

  bool CreateFolder() const;

  bool FileOrDirectoryExists() const;

  bool DirectoryExists() const;

  bool IsDirectory() const;

  bool IsRootDirectory() const;

  bool IsAbsolutePath() const;

 private:

  void Normalize();

  const char* FindLastPathSeparator() const;

  size_t CalculateRootLength() const;

  std::string pathname_;
};

}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif

#endif
