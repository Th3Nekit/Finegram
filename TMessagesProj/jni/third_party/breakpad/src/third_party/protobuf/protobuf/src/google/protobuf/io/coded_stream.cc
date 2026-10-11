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
// This implementation is heavily optimized to make reads and writes
// of small values (especially varints) as fast as possible.  In
// particular, we optimize for the common case that a read or a write
// will not cross the end of the buffer, since we can avoid a lot
// of branching in this case.

#include <google/protobuf/io/coded_stream_inl.h>
#include <algorithm>
#include <limits.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stl_util-inl.h>

namespace google {
namespace protobuf {
namespace io {

namespace {

static const int kMaxVarintBytes = 10;
static const int kMaxVarint32Bytes = 5;

inline bool NextNonEmpty(ZeroCopyInputStream* input,
                         const void** data, int* size) {
  bool success;
  do {
    success = input->Next(data, size);
  } while (success && *size == 0);
  return success;
}

}

void CodedInputStream::BackUpInputToCurrentPosition() {
  int backup_bytes = BufferSize() + buffer_size_after_limit_ + overflow_bytes_;
  if (backup_bytes > 0) {
    input_->BackUp(backup_bytes);

    total_bytes_read_ -= BufferSize() + buffer_size_after_limit_;
    buffer_end_ = buffer_;
    buffer_size_after_limit_ = 0;
    overflow_bytes_ = 0;
  }
}

inline void CodedInputStream::RecomputeBufferLimits() {
  buffer_end_ += buffer_size_after_limit_;
  int closest_limit = min(current_limit_, total_bytes_limit_);
  if (closest_limit < total_bytes_read_) {

    buffer_size_after_limit_ = total_bytes_read_ - closest_limit;
    buffer_end_ -= buffer_size_after_limit_;
  } else {
    buffer_size_after_limit_ = 0;
  }
}

CodedInputStream::Limit CodedInputStream::PushLimit(int byte_limit) {

  int current_position = total_bytes_read_ -
      (BufferSize() + buffer_size_after_limit_);

  Limit old_limit = current_limit_;

  if (byte_limit >= 0 &&
      byte_limit <= INT_MAX - current_position) {
    current_limit_ = current_position + byte_limit;
  } else {

    current_limit_ = INT_MAX;
  }

  current_limit_ = min(current_limit_, old_limit);

  RecomputeBufferLimits();
  return old_limit;
}

void CodedInputStream::PopLimit(Limit limit) {

  current_limit_ = limit;
  RecomputeBufferLimits();

  legitimate_message_end_ = false;
}

int CodedInputStream::BytesUntilLimit() {
  if (current_limit_ == INT_MAX) return -1;
  int current_position = total_bytes_read_ -
      (BufferSize() + buffer_size_after_limit_);

  return current_limit_ - current_position;
}

void CodedInputStream::SetTotalBytesLimit(
    int total_bytes_limit, int warning_threshold) {

  int current_position = total_bytes_read_ -
      (BufferSize() + buffer_size_after_limit_);
  total_bytes_limit_ = max(current_position, total_bytes_limit);
  total_bytes_warning_threshold_ = warning_threshold;
  RecomputeBufferLimits();
}

void CodedInputStream::PrintTotalBytesLimitError() {
  GOOGLE_LOG(ERROR) << "A protocol message was rejected because it was too "
                "big (more than " << total_bytes_limit_
             << " bytes).  To increase the limit (or to disable these "
                "warnings), see CodedInputStream::SetTotalBytesLimit() "
                "in google/protobuf/io/coded_stream.h.";
}

bool CodedInputStream::Skip(int count) {
  if (count < 0) return false;

  const int original_buffer_size = BufferSize();

  if (count <= original_buffer_size) {

    Advance(count);
    return true;
  }

  if (buffer_size_after_limit_ > 0) {

    Advance(original_buffer_size);
    return false;
  }

  count -= original_buffer_size;
  buffer_ = NULL;
  buffer_end_ = buffer_;

  int closest_limit = min(current_limit_, total_bytes_limit_);
  int bytes_until_limit = closest_limit - total_bytes_read_;
  if (bytes_until_limit < count) {

    if (bytes_until_limit > 0) {
      total_bytes_read_ = closest_limit;
      input_->Skip(bytes_until_limit);
    }
    return false;
  }

  total_bytes_read_ += count;
  return input_->Skip(count);
}

bool CodedInputStream::GetDirectBufferPointer(const void** data, int* size) {
  if (BufferSize() == 0 && !Refresh()) return false;

  *data = buffer_;
  *size = BufferSize();
  return true;
}

bool CodedInputStream::ReadRaw(void* buffer, int size) {
  int current_buffer_size;
  while ((current_buffer_size = BufferSize()) < size) {

    memcpy(buffer, buffer_, current_buffer_size);
    buffer = reinterpret_cast<uint8*>(buffer) + current_buffer_size;
    size -= current_buffer_size;
    Advance(current_buffer_size);
    if (!Refresh()) return false;
  }

  memcpy(buffer, buffer_, size);
  Advance(size);

  return true;
}

bool CodedInputStream::ReadString(string* buffer, int size) {
  if (size < 0) return false;
  return InternalReadStringInline(buffer, size);
}

bool CodedInputStream::ReadStringFallback(string* buffer, int size) {
  if (!buffer->empty()) {
    buffer->clear();
  }

  int current_buffer_size;
  while ((current_buffer_size = BufferSize()) < size) {

    if (current_buffer_size != 0) {

      buffer->append(reinterpret_cast<const char*>(buffer_),
                     current_buffer_size);
    }
    size -= current_buffer_size;
    Advance(current_buffer_size);
    if (!Refresh()) return false;
  }

  buffer->append(reinterpret_cast<const char*>(buffer_), size);
  Advance(size);

  return true;
}

bool CodedInputStream::ReadLittleEndian32Fallback(uint32* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (BufferSize() >= sizeof(*value)) {

    ptr = buffer_;
    Advance(sizeof(*value));
  } else {

    if (!ReadRaw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  ReadLittleEndian32FromArray(ptr, value);
  return true;
}

bool CodedInputStream::ReadLittleEndian64Fallback(uint64* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (BufferSize() >= sizeof(*value)) {

    ptr = buffer_;
    Advance(sizeof(*value));
  } else {

    if (!ReadRaw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  ReadLittleEndian64FromArray(ptr, value);
  return true;
}

namespace {

inline const uint8* ReadVarint32FromArray(
    const uint8* buffer, uint32* value) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;
inline const uint8* ReadVarint32FromArray(const uint8* buffer, uint32* value) {

  const uint8* ptr = buffer;
  uint32 b;
  uint32 result;

  b = *(ptr++); result  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

  for (int i = 0; i < kMaxVarintBytes - kMaxVarint32Bytes; i++) {
    b = *(ptr++); if (!(b & 0x80)) goto done;
  }

  return NULL;

 done:
  *value = result;
  return ptr;
}

}

bool CodedInputStream::ReadVarint32Slow(uint32* value) {
  uint64 result;

  if (!ReadVarint64Fallback(&result)) return false;
  *value = (uint32)result;
  return true;
}

bool CodedInputStream::ReadVarint32Fallback(uint32* value) {
  if (BufferSize() >= kMaxVarintBytes ||

      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    const uint8* end = ReadVarint32FromArray(buffer_, value);
    if (end == NULL) return false;
    buffer_ = end;
    return true;
  } else {

    return ReadVarint32Slow(value);
  }
}

uint32 CodedInputStream::ReadTagSlow() {
  if (buffer_ == buffer_end_) {

    if (!Refresh()) {

      int current_position = total_bytes_read_ - buffer_size_after_limit_;
      if (current_position >= total_bytes_limit_) {

        legitimate_message_end_ = current_limit_ == total_bytes_limit_;
      } else {
        legitimate_message_end_ = true;
      }
      return 0;
    }
  }

  uint64 result;
  if (!ReadVarint64(&result)) return 0;
  return static_cast<uint32>(result);
}

uint32 CodedInputStream::ReadTagFallback() {
  if (BufferSize() >= kMaxVarintBytes ||

      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    uint32 tag;
    const uint8* end = ReadVarint32FromArray(buffer_, &tag);
    if (end == NULL) {
      return 0;
    }
    buffer_ = end;
    return tag;
  } else {

    if (buffer_ == buffer_end_ && buffer_size_after_limit_ > 0 &&

        total_bytes_read_ - buffer_size_after_limit_ < total_bytes_limit_) {

      legitimate_message_end_ = true;
      return 0;
    }
    return ReadTagSlow();
  }
}

bool CodedInputStream::ReadVarint64Slow(uint64* value) {

  uint64 result = 0;
  int count = 0;
  uint32 b;

  do {
    if (count == kMaxVarintBytes) return false;
    while (buffer_ == buffer_end_) {
      if (!Refresh()) return false;
    }
    b = *buffer_;
    result |= static_cast<uint64>(b & 0x7F) << (7 * count);
    Advance(1);
    ++count;
  } while (b & 0x80);

  *value = result;
  return true;
}

bool CodedInputStream::ReadVarint64Fallback(uint64* value) {
  if (BufferSize() >= kMaxVarintBytes ||

      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {

    const uint8* ptr = buffer_;
    uint32 b;

    uint32 part0 = 0, part1 = 0, part2 = 0;

    b = *(ptr++); part0  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); part0 |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
    b = *(ptr++); part1 |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
    b = *(ptr++); part2  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
    b = *(ptr++); part2 |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;

    return NULL;

   done:
    Advance(ptr - buffer_);
    *value = (static_cast<uint64>(part0)      ) |
             (static_cast<uint64>(part1) << 28) |
             (static_cast<uint64>(part2) << 56);
    return true;
  } else {
    return ReadVarint64Slow(value);
  }
}

bool CodedInputStream::Refresh() {
  GOOGLE_DCHECK_EQ(0, BufferSize());

  if (buffer_size_after_limit_ > 0 || overflow_bytes_ > 0 ||
      total_bytes_read_ == current_limit_) {

    int current_position = total_bytes_read_ - buffer_size_after_limit_;

    if (current_position >= total_bytes_limit_ &&
        total_bytes_limit_ != current_limit_) {

      PrintTotalBytesLimitError();
    }

    return false;
  }

  if (total_bytes_warning_threshold_ >= 0 &&
      total_bytes_read_ >= total_bytes_warning_threshold_) {
      GOOGLE_LOG(WARNING) << "Reading dangerously large protocol message.  If the "
                      "message turns out to be larger than "
                   << total_bytes_limit_ << " bytes, parsing will be halted "
                      "for security reasons.  To increase the limit (or to "
                      "disable these warnings), see "
                      "CodedInputStream::SetTotalBytesLimit() in "
                      "google/protobuf/io/coded_stream.h.";

    total_bytes_warning_threshold_ = -1;
  }

  const void* void_buffer;
  int buffer_size;
  if (NextNonEmpty(input_, &void_buffer, &buffer_size)) {
    buffer_ = reinterpret_cast<const uint8*>(void_buffer);
    buffer_end_ = buffer_ + buffer_size;
    GOOGLE_CHECK_GE(buffer_size, 0);

    if (total_bytes_read_ <= INT_MAX - buffer_size) {
      total_bytes_read_ += buffer_size;
    } else {

      overflow_bytes_ = total_bytes_read_ - (INT_MAX - buffer_size);
      buffer_end_ -= overflow_bytes_;
      total_bytes_read_ = INT_MAX;
    }

    RecomputeBufferLimits();
    return true;
  } else {
    buffer_ = NULL;
    buffer_end_ = NULL;
    return false;
  }
}

CodedOutputStream::CodedOutputStream(ZeroCopyOutputStream* output)
  : output_(output),
    buffer_(NULL),
    buffer_size_(0),
    total_bytes_(0),
    had_error_(false) {

  Refresh();

  had_error_ = false;
}

CodedOutputStream::~CodedOutputStream() {
  if (buffer_size_ > 0) {
    output_->BackUp(buffer_size_);
  }
}

bool CodedOutputStream::Skip(int count) {
  if (count < 0) return false;

  while (count > buffer_size_) {
    count -= buffer_size_;
    if (!Refresh()) return false;
  }

  Advance(count);
  return true;
}

bool CodedOutputStream::GetDirectBufferPointer(void** data, int* size) {
  if (buffer_size_ == 0 && !Refresh()) return false;

  *data = buffer_;
  *size = buffer_size_;
  return true;
}

void CodedOutputStream::WriteRaw(const void* data, int size) {
  while (buffer_size_ < size) {
    memcpy(buffer_, data, buffer_size_);
    size -= buffer_size_;
    data = reinterpret_cast<const uint8*>(data) + buffer_size_;
    if (!Refresh()) return;
  }

  memcpy(buffer_, data, size);
  Advance(size);
}

uint8* CodedOutputStream::WriteRawToArray(
    const void* data, int size, uint8* target) {
  memcpy(target, data, size);
  return target + size;
}

void CodedOutputStream::WriteLittleEndian32(uint32 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  WriteLittleEndian32ToArray(value, ptr);

  if (use_fast) {
    Advance(sizeof(value));
  } else {
    WriteRaw(bytes, sizeof(value));
  }
}

void CodedOutputStream::WriteLittleEndian64(uint64 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  WriteLittleEndian64ToArray(value, ptr);

  if (use_fast) {
    Advance(sizeof(value));
  } else {
    WriteRaw(bytes, sizeof(value));
  }
}

inline uint8* CodedOutputStream::WriteVarint32FallbackToArrayInline(
    uint32 value, uint8* target) {
  target[0] = static_cast<uint8>(value | 0x80);
  if (value >= (1 << 7)) {
    target[1] = static_cast<uint8>((value >>  7) | 0x80);
    if (value >= (1 << 14)) {
      target[2] = static_cast<uint8>((value >> 14) | 0x80);
      if (value >= (1 << 21)) {
        target[3] = static_cast<uint8>((value >> 21) | 0x80);
        if (value >= (1 << 28)) {
          target[4] = static_cast<uint8>(value >> 28);
          return target + 5;
        } else {
          target[3] &= 0x7F;
          return target + 4;
        }
      } else {
        target[2] &= 0x7F;
        return target + 3;
      }
    } else {
      target[1] &= 0x7F;
      return target + 2;
    }
  } else {
    target[0] &= 0x7F;
    return target + 1;
  }
}

void CodedOutputStream::WriteVarint32(uint32 value) {
  if (buffer_size_ >= kMaxVarint32Bytes) {

    uint8* target = buffer_;
    uint8* end = WriteVarint32FallbackToArrayInline(value, target);
    int size = end - target;
    Advance(size);
  } else {

    uint8 bytes[kMaxVarint32Bytes];
    int size = 0;
    while (value > 0x7F) {
      bytes[size++] = (static_cast<uint8>(value) & 0x7F) | 0x80;
      value >>= 7;
    }
    bytes[size++] = static_cast<uint8>(value) & 0x7F;
    WriteRaw(bytes, size);
  }
}

uint8* CodedOutputStream::WriteVarint32FallbackToArray(
    uint32 value, uint8* target) {
  return WriteVarint32FallbackToArrayInline(value, target);
}

inline uint8* CodedOutputStream::WriteVarint64ToArrayInline(
    uint64 value, uint8* target) {

  uint32 part0 = static_cast<uint32>(value      );
  uint32 part1 = static_cast<uint32>(value >> 28);
  uint32 part2 = static_cast<uint32>(value >> 56);

  int size;

  if (part2 == 0) {
    if (part1 == 0) {
      if (part0 < (1 << 14)) {
        if (part0 < (1 << 7)) {
          size = 1; goto size1;
        } else {
          size = 2; goto size2;
        }
      } else {
        if (part0 < (1 << 21)) {
          size = 3; goto size3;
        } else {
          size = 4; goto size4;
        }
      }
    } else {
      if (part1 < (1 << 14)) {
        if (part1 < (1 << 7)) {
          size = 5; goto size5;
        } else {
          size = 6; goto size6;
        }
      } else {
        if (part1 < (1 << 21)) {
          size = 7; goto size7;
        } else {
          size = 8; goto size8;
        }
      }
    }
  } else {
    if (part2 < (1 << 7)) {
      size = 9; goto size9;
    } else {
      size = 10; goto size10;
    }
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";

  size10: target[9] = static_cast<uint8>((part2 >>  7) | 0x80);
  size9 : target[8] = static_cast<uint8>((part2      ) | 0x80);
  size8 : target[7] = static_cast<uint8>((part1 >> 21) | 0x80);
  size7 : target[6] = static_cast<uint8>((part1 >> 14) | 0x80);
  size6 : target[5] = static_cast<uint8>((part1 >>  7) | 0x80);
  size5 : target[4] = static_cast<uint8>((part1      ) | 0x80);
  size4 : target[3] = static_cast<uint8>((part0 >> 21) | 0x80);
  size3 : target[2] = static_cast<uint8>((part0 >> 14) | 0x80);
  size2 : target[1] = static_cast<uint8>((part0 >>  7) | 0x80);
  size1 : target[0] = static_cast<uint8>((part0      ) | 0x80);

  target[size-1] &= 0x7F;
  return target + size;
}

void CodedOutputStream::WriteVarint64(uint64 value) {
  if (buffer_size_ >= kMaxVarintBytes) {

    uint8* target = buffer_;

    uint8* end = WriteVarint64ToArrayInline(value, target);
    int size = end - target;
    Advance(size);
  } else {

    uint8 bytes[kMaxVarintBytes];
    int size = 0;
    while (value > 0x7F) {
      bytes[size++] = (static_cast<uint8>(value) & 0x7F) | 0x80;
      value >>= 7;
    }
    bytes[size++] = static_cast<uint8>(value) & 0x7F;
    WriteRaw(bytes, size);
  }
}

uint8* CodedOutputStream::WriteVarint64ToArray(
    uint64 value, uint8* target) {
  return WriteVarint64ToArrayInline(value, target);
}

bool CodedOutputStream::Refresh() {
  void* void_buffer;
  if (output_->Next(&void_buffer, &buffer_size_)) {
    buffer_ = reinterpret_cast<uint8*>(void_buffer);
    total_bytes_ += buffer_size_;
    return true;
  } else {
    buffer_ = NULL;
    buffer_size_ = 0;
    had_error_ = true;
    return false;
  }
}

int CodedOutputStream::VarintSize32Fallback(uint32 value) {
  if (value < (1 << 7)) {
    return 1;
  } else if (value < (1 << 14)) {
    return 2;
  } else if (value < (1 << 21)) {
    return 3;
  } else if (value < (1 << 28)) {
    return 4;
  } else {
    return 5;
  }
}

int CodedOutputStream::VarintSize64(uint64 value) {
  if (value < (1ull << 35)) {
    if (value < (1ull << 7)) {
      return 1;
    } else if (value < (1ull << 14)) {
      return 2;
    } else if (value < (1ull << 21)) {
      return 3;
    } else if (value < (1ull << 28)) {
      return 4;
    } else {
      return 5;
    }
  } else {
    if (value < (1ull << 42)) {
      return 6;
    } else if (value < (1ull << 49)) {
      return 7;
    } else if (value < (1ull << 56)) {
      return 8;
    } else if (value < (1ull << 63)) {
      return 9;
    } else {
      return 10;
    }
  }
}

}
}
}
