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
// emulates google3/testing/base/public/googletest.h

#ifndef GOOGLE_PROTOBUF_GOOGLETEST_H__
#define GOOGLE_PROTOBUF_GOOGLETEST_H__

#include <vector>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

string TestSourceDir();

string TestTempDir();

void CaptureTestStdout();
void CaptureTestStderr();

string GetCapturedTestStdout();
string GetCapturedTestStderr();

#undef ERROR  // defend against promiscuous windows.h
static const LogLevel ERROR = LOGLEVEL_ERROR;

class ScopedMemoryLog {
 public:
  ScopedMemoryLog();
  virtual ~ScopedMemoryLog();

  const vector<string>& GetMessages(LogLevel dummy) const;

 private:
  vector<string> messages_;
  LogHandler* old_handler_;

  static void HandleLog(LogLevel level, const char* filename, int line,
                        const string& message);

  static ScopedMemoryLog* active_log_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ScopedMemoryLog);
};

}
}

#endif
