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

// memory_range.h: Define the google_breakpad::MemoryRange class, which
// is a lightweight wrapper with a pointer and a length to encapsulate
// a contiguous range of memory.

#ifndef COMMON_MEMORY_RANGE_H_
#define COMMON_MEMORY_RANGE_H_

#include <stddef.h>

#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

class MemoryRange {
 public:
  MemoryRange() : data_(NULL), length_(0) {}

  MemoryRange(const void* data, size_t length) {
    Set(data, length);
  }

  bool IsEmpty() const {

    return length_ == 0;
  }

  void Reset() {
    data_ = NULL;
    length_ = 0;
  }

  void Set(const void* data, size_t length) {
    data_ = reinterpret_cast<const uint8_t*>(data);

    length_ = data ? length : 0;
  }

  bool Covers(size_t sub_offset, size_t sub_length) const {

    return sub_offset < length_ &&
           sub_offset + sub_length >= sub_offset &&
           sub_offset + sub_length <= length_;
  }

  const void* GetData(size_t sub_offset, size_t sub_length) const {
    return Covers(sub_offset, sub_length) ? (data_ + sub_offset) : NULL;
  }

  template <typename DataType>
  const DataType* GetData(size_t sub_offset) const {
    return reinterpret_cast<const DataType*>(
        GetData(sub_offset, sizeof(DataType)));
  }

  const void* GetArrayElement(size_t element_offset,
                              size_t element_size,
                              unsigned element_index) const {
    size_t sub_offset = element_offset + element_index * element_size;
    return GetData(sub_offset, element_size);
  }

  template <typename ElementType>
  const ElementType* GetArrayElement(size_t element_offset,
                                     unsigned element_index) const {
    return reinterpret_cast<const ElementType*>(
        GetArrayElement(element_offset, sizeof(ElementType), element_index));
  }

  MemoryRange Subrange(size_t sub_offset, size_t sub_length) const {
    return Covers(sub_offset, sub_length) ?
        MemoryRange(data_ + sub_offset, sub_length) : MemoryRange();
  }

  const uint8_t* data() const { return data_; }

  size_t length() const { return length_; }

 private:

  const uint8_t* data_;

  size_t length_;
};

}

#endif
