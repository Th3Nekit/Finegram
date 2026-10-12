// Copyright (c) 2013, Google Inc.
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

#ifndef CLIENT_LINUX_MINIDUMP_WRITER_CPU_SET_H_
#define CLIENT_LINUX_MINIDUMP_WRITER_CPU_SET_H_

#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "common/linux/linux_libc_support.h"
#include "third_party/lss/linux_syscall_support.h"

namespace google_breakpad {

class CpuSet {
public:

  static const size_t kMaxCpus = 1024;

  CpuSet() {
    my_memset(mask_, 0, sizeof(mask_));
  }

  bool ParseSysFile(int fd) {
    char buffer[512];
    int ret = sys_read(fd, buffer, sizeof(buffer)-1);
    if (ret < 0)
      return false;

    buffer[ret] = '\0';

    const char* p = buffer;
    const char* p_end = p + ret;
    while (p < p_end) {

      while (p < p_end && my_isspace(*p))
        p++;

      const char* item = p;
      size_t item_len = static_cast<size_t>(p_end - p);
      const char* item_next =
          static_cast<const char*>(my_memchr(p, ',', item_len));
      if (item_next != NULL) {
        p = item_next + 1;
        item_len = static_cast<size_t>(item_next - item);
      } else {
        p = p_end;
        item_next = p_end;
      }

      while (item_next > item && my_isspace(item_next[-1]))
        item_next--;

      if (item_next == item)
        continue;

      uintptr_t start = 0;
      const char* next = my_read_decimal_ptr(&start, item);
      uintptr_t end = start;
      if (*next == '-')
        my_read_decimal_ptr(&end, next+1);

      while (start <= end)
        SetBit(start++);
    }
    return true;
  }

  void IntersectWith(const CpuSet& other) {
    for (size_t nn = 0; nn < kMaskWordCount; ++nn)
      mask_[nn] &= other.mask_[nn];
  }

  int GetCount() {
    int result = 0;
    for (size_t nn = 0; nn < kMaskWordCount; ++nn) {
      result += __builtin_popcount(mask_[nn]);
    }
    return result;
  }

private:
  void SetBit(uintptr_t index) {
    size_t nn = static_cast<size_t>(index);
    if (nn < kMaxCpus)
      mask_[nn / kMaskWordBits] |= (1U << (nn % kMaskWordBits));
  }

  typedef uint32_t MaskWordType;
  static const size_t kMaskWordBits = 8*sizeof(MaskWordType);
  static const size_t kMaskWordCount =
      (kMaxCpus + kMaskWordBits - 1) / kMaskWordBits;

  MaskWordType mask_[kMaskWordCount];
};

}

#endif
