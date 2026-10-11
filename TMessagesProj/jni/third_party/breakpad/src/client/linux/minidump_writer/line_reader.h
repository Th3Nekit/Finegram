// Copyright (c) 2009, Google Inc.
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

#ifndef CLIENT_LINUX_MINIDUMP_WRITER_LINE_READER_H_
#define CLIENT_LINUX_MINIDUMP_WRITER_LINE_READER_H_

#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "common/linux/linux_libc_support.h"
#include "third_party/lss/linux_syscall_support.h"

namespace google_breakpad {

class LineReader {
 public:
  LineReader(int fd)
      : fd_(fd),
        hit_eof_(false),
        buf_used_(0) {
  }

  static const size_t kMaxLineLen = 512;

  bool GetNextLine(const char **line, unsigned *len) {
    for (;;) {
      if (buf_used_ == 0 && hit_eof_)
        return false;

      for (unsigned i = 0; i < buf_used_; ++i) {
        if (buf_[i] == '\n' || buf_[i] == 0) {
          buf_[i] = 0;
          *len = i;
          *line = buf_;
          return true;
        }
      }

      if (buf_used_ == sizeof(buf_)) {

        return false;
      }

      if (hit_eof_) {
        assert(buf_used_);

        buf_[buf_used_] = 0;
        *len = buf_used_;
        buf_used_ += 1;
        *line = buf_;
        return true;
      }

      const ssize_t n = sys_read(fd_, buf_ + buf_used_,
                                 sizeof(buf_) - buf_used_);
      if (n < 0) {
        return false;
      } else if (n == 0) {
        hit_eof_ = true;
      } else {
        buf_used_ += n;
      }

    }
  }

  void PopLine(unsigned len) {

    assert(buf_used_ >= len + 1);
    buf_used_ -= len + 1;
    my_memmove(buf_, buf_ + len + 1, buf_used_);
  }

 private:
  const int fd_;

  bool hit_eof_;
  unsigned buf_used_;
  char buf_[kMaxLineLen];
};

}

#endif
