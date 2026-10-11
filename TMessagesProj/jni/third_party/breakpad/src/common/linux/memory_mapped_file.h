// Copyright (c) 2011, Google Inc.
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

// memory_mapped_file.h: Define the google_breakpad::MemoryMappedFile
// class, which maps a file into memory for read-only access.

#ifndef COMMON_LINUX_MEMORY_MAPPED_FILE_H_
#define COMMON_LINUX_MEMORY_MAPPED_FILE_H_

#include <stddef.h>
#include "common/basictypes.h"
#include "common/memory_range.h"

namespace google_breakpad {

class MemoryMappedFile {
 public:
  MemoryMappedFile();

  MemoryMappedFile(const char* path, size_t offset);

  ~MemoryMappedFile();

  bool Map(const char* path, size_t offset);

  void Unmap();

  const MemoryRange& content() const { return content_; }

  const void* data() const { return content_.data(); }

  size_t size() const { return content_.length(); }

 private:

  MemoryRange content_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedFile);
};

}

#endif
