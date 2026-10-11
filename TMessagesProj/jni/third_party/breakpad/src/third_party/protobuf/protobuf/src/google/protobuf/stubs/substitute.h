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
// from google3/strings/substitute.h

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>

#ifndef GOOGLE_PROTOBUF_STUBS_SUBSTITUTE_H_
#define GOOGLE_PROTOBUF_STUBS_SUBSTITUTE_H_

namespace google {
namespace protobuf {
namespace strings {

namespace internal {

class SubstituteArg {
 public:
  inline SubstituteArg(const char* value)
    : text_(value), size_(strlen(text_)) {}
  inline SubstituteArg(const string& value)
    : text_(value.data()), size_(value.size()) {}

  inline explicit SubstituteArg()
    : text_(NULL), size_(-1) {}

  inline SubstituteArg(char value)
    : text_(scratch_), size_(1) { scratch_[0] = value; }
  inline SubstituteArg(short value)
    : text_(FastInt32ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(unsigned short value)
    : text_(FastUInt32ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(int value)
    : text_(FastInt32ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(unsigned int value)
    : text_(FastUInt32ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(long value)
    : text_(FastLongToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(unsigned long value)
    : text_(FastULongToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(long long value)
    : text_(FastInt64ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(unsigned long long value)
    : text_(FastUInt64ToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(float value)
    : text_(FloatToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(double value)
    : text_(DoubleToBuffer(value, scratch_)), size_(strlen(text_)) {}
  inline SubstituteArg(bool value)
    : text_(value ? "true" : "false"), size_(strlen(text_)) {}

  inline const char* data() const { return text_; }
  inline int size() const { return size_; }

 private:
  const char* text_;
  int size_;
  char scratch_[kFastToBufferSize];
};

}

LIBPROTOBUF_EXPORT string Substitute(
  const char* format,
  const internal::SubstituteArg& arg0 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg1 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg2 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg3 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg4 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg5 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg6 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg7 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg8 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg9 = internal::SubstituteArg());

LIBPROTOBUF_EXPORT void SubstituteAndAppend(
  string* output, const char* format,
  const internal::SubstituteArg& arg0 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg1 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg2 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg3 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg4 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg5 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg6 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg7 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg8 = internal::SubstituteArg(),
  const internal::SubstituteArg& arg9 = internal::SubstituteArg());

}
}
}

#endif
