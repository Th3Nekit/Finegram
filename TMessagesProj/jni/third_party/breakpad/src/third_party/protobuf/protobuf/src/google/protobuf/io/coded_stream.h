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
// This file contains the CodedInputStream and CodedOutputStream classes,
// which wrap a ZeroCopyInputStream or ZeroCopyOutputStream, respectively,
// and allow you to read or write individual pieces of data in various
// formats.  In particular, these implement the varint encoding for
// integers, a simple variable-length encoding in which smaller numbers
// take fewer bytes.
//
// Typically these classes will only be used internally by the protocol
// buffer library in order to encode and decode protocol buffers.  Clients
// of the library only need to know about this class if they wish to write
// custom message parsing or serialization procedures.
//
// CodedOutputStream example:
//   // Write some data to "myfile".  First we write a 4-byte "magic number"
//   // to identify the file type, then write a length-delimited string.  The
//   // string is composed of a varint giving the length followed by the raw
//   // bytes.
//   int fd = open("myfile", O_WRONLY);
//   ZeroCopyOutputStream* raw_output = new FileOutputStream(fd);
//   CodedOutputStream* coded_output = new CodedOutputStream(raw_output);
//
//   int magic_number = 1234;
//   char text[] = "Hello world!";
//   coded_output->WriteLittleEndian32(magic_number);
//   coded_output->WriteVarint32(strlen(text));
//   coded_output->WriteRaw(text, strlen(text));
//
//   delete coded_output;
//   delete raw_output;
//   close(fd);
//
// CodedInputStream example:
//   // Read a file created by the above code.
//   int fd = open("myfile", O_RDONLY);
//   ZeroCopyInputStream* raw_input = new FileInputStream(fd);
//   CodedInputStream coded_input = new CodedInputStream(raw_input);
//
//   coded_input->ReadLittleEndian32(&magic_number);
//   if (magic_number != 1234) {
//     cerr << "File not in expected format." << endl;
//     return;
//   }
//
//   uint32 size;
//   coded_input->ReadVarint32(&size);
//
//   char* text = new char[size + 1];
//   coded_input->ReadRaw(buffer, size);
//   text[size] = '\0';
//
//   delete coded_input;
//   delete raw_input;
//   close(fd);
//
//   cout << "Text is: " << text << endl;
//   delete [] text;
//
// For those who are interested, varint encoding is defined as follows:
//
// The encoding operates on unsigned integers of up to 64 bits in length.
// Each byte of the encoded value has the format:
// * bits 0-6: Seven bits of the number being encoded.
// * bit 7: Zero if this is the last byte in the encoding (in which
//   case all remaining bits of the number are zero) or 1 if
//   more bytes follow.
// The first byte contains the least-significant 7 bits of the number, the
// second byte (if present) contains the next-least-significant 7 bits,
// and so on.  So, the binary number 1011000101011 would be encoded in two
// bytes as "10101011 00101100".
//
// In theory, varint could be used to encode integers of any length.
// However, for practicality we set a limit at 64 bits.  The maximum encoded
// length of a number is thus 10 bytes.

#ifndef GOOGLE_PROTOBUF_IO_CODED_STREAM_H__
#define GOOGLE_PROTOBUF_IO_CODED_STREAM_H__

#include <string>
#ifdef _MSC_VER
  #if defined(_M_IX86) && \
      !defined(PROTOBUF_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
    #define PROTOBUF_LITTLE_ENDIAN 1
  #endif
  #if _MSC_VER >= 1300

    #pragma runtime_checks("c", off)
  #endif
#else
  #include <sys/param.h>
  #if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN && \
      !defined(PROTOBUF_DISABLE_LITTLE_ENDIAN_OPT_FOR_TEST)
    #define PROTOBUF_LITTLE_ENDIAN 1
  #endif
#endif
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

class DescriptorPool;
class MessageFactory;

namespace io {

class CodedInputStream;
class CodedOutputStream;

class ZeroCopyInputStream;
class ZeroCopyOutputStream;

class LIBPROTOBUF_EXPORT CodedInputStream {
 public:

  explicit CodedInputStream(ZeroCopyInputStream* input);

  explicit CodedInputStream(const uint8* buffer, int size);

  ~CodedInputStream();

  bool Skip(int count);

  bool GetDirectBufferPointer(const void** data, int* size);

  inline void GetDirectBufferPointerInline(const void** data,
                                           int* size) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  bool ReadRaw(void* buffer, int size);

  bool ReadString(string* buffer, int size);

  inline bool InternalReadStringInline(string* buffer,
                                       int size) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  bool ReadLittleEndian32(uint32* value);

  bool ReadLittleEndian64(uint64* value);

  static const uint8* ReadLittleEndian32FromArray(const uint8* buffer,
                                                   uint32* value);

  static const uint8* ReadLittleEndian64FromArray(const uint8* buffer,
                                                   uint64* value);

  bool ReadVarint32(uint32* value);

  bool ReadVarint64(uint64* value);

  uint32 ReadTag() GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  bool ExpectTag(uint32 expected) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  static const uint8* ExpectTagFromArray(
      const uint8* buffer,
      uint32 expected) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  bool ExpectAtEnd();

  bool LastTagWas(uint32 expected);

  bool ConsumedEntireMessage();

  typedef int Limit;

  Limit PushLimit(int byte_limit);

  void PopLimit(Limit limit);

  int BytesUntilLimit();

  void SetTotalBytesLimit(int total_bytes_limit, int warning_threshold);

  void SetRecursionLimit(int limit);

  bool IncrementRecursionDepth();

  void DecrementRecursionDepth();

  void SetExtensionRegistry(DescriptorPool* pool, MessageFactory* factory);

  const DescriptorPool* GetExtensionPool();

  MessageFactory* GetExtensionFactory();

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CodedInputStream);

  ZeroCopyInputStream* input_;
  const uint8* buffer_;
  const uint8* buffer_end_;
  int total_bytes_read_;

  int overflow_bytes_;

  uint32 last_tag_;

  bool legitimate_message_end_;

  bool aliasing_enabled_;

  Limit current_limit_;

  int buffer_size_after_limit_;

  int total_bytes_limit_;
  int total_bytes_warning_threshold_;

  int recursion_depth_;

  int recursion_limit_;

  const DescriptorPool* extension_pool_;
  MessageFactory* extension_factory_;

  void Advance(int amount);

  void BackUpInputToCurrentPosition();

  void RecomputeBufferLimits();

  void PrintTotalBytesLimitError();

  bool Refresh();

  bool ReadVarint32Fallback(uint32* value);
  bool ReadVarint64Fallback(uint64* value);
  bool ReadVarint32Slow(uint32* value);
  bool ReadVarint64Slow(uint64* value);
  bool ReadLittleEndian32Fallback(uint32* value);
  bool ReadLittleEndian64Fallback(uint64* value);

  uint32 ReadTagFallback();
  uint32 ReadTagSlow();
  bool ReadStringFallback(string* buffer, int size);

  int BufferSize() const;

  static const int kDefaultTotalBytesLimit = 64 << 20;

  static const int kDefaultTotalBytesWarningThreshold = 32 << 20;
  static const int kDefaultRecursionLimit = 64;
};

class LIBPROTOBUF_EXPORT CodedOutputStream {
 public:

  explicit CodedOutputStream(ZeroCopyOutputStream* output);

  ~CodedOutputStream();

  bool Skip(int count);

  bool GetDirectBufferPointer(void** data, int* size);

  inline uint8* GetDirectBufferForNBytesAndAdvance(int size);

  void WriteRaw(const void* buffer, int size);

  static uint8* WriteRawToArray(const void* buffer, int size, uint8* target);

  void WriteString(const string& str);

  static uint8* WriteStringToArray(const string& str, uint8* target);

  void WriteLittleEndian32(uint32 value);

  static uint8* WriteLittleEndian32ToArray(uint32 value, uint8* target);

  void WriteLittleEndian64(uint64 value);

  static uint8* WriteLittleEndian64ToArray(uint64 value, uint8* target);

  void WriteVarint32(uint32 value);

  static uint8* WriteVarint32ToArray(uint32 value, uint8* target);

  void WriteVarint64(uint64 value);

  static uint8* WriteVarint64ToArray(uint64 value, uint8* target);

  void WriteVarint32SignExtended(int32 value);

  static uint8* WriteVarint32SignExtendedToArray(int32 value, uint8* target);

  void WriteTag(uint32 value);

  static uint8* WriteTagToArray(
      uint32 value, uint8* target) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  static int VarintSize32(uint32 value);

  static int VarintSize64(uint64 value);

  static int VarintSize32SignExtended(int32 value);

  inline int ByteCount() const;

  bool HadError() const { return had_error_; }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CodedOutputStream);

  ZeroCopyOutputStream* output_;
  uint8* buffer_;
  int buffer_size_;
  int total_bytes_;
  bool had_error_;

  void Advance(int amount);

  bool Refresh();

  static uint8* WriteVarint32FallbackToArray(uint32 value, uint8* target);

  static uint8* WriteVarint32FallbackToArrayInline(
      uint32 value, uint8* target) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;
  static uint8* WriteVarint64ToArrayInline(
      uint64 value, uint8* target) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  static int VarintSize32Fallback(uint32 value);
};

inline bool CodedInputStream::ReadVarint32(uint32* value) {
  if (GOOGLE_PREDICT_TRUE(buffer_ < buffer_end_) && *buffer_ < 0x80) {
    *value = *buffer_;
    Advance(1);
    return true;
  } else {
    return ReadVarint32Fallback(value);
  }
}

inline bool CodedInputStream::ReadVarint64(uint64* value) {
  if (GOOGLE_PREDICT_TRUE(buffer_ < buffer_end_) && *buffer_ < 0x80) {
    *value = *buffer_;
    Advance(1);
    return true;
  } else {
    return ReadVarint64Fallback(value);
  }
}

inline const uint8* CodedInputStream::ReadLittleEndian32FromArray(
    const uint8* buffer,
    uint32* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  *value = (static_cast<uint32>(buffer[0])      ) |
           (static_cast<uint32>(buffer[1]) <<  8) |
           (static_cast<uint32>(buffer[2]) << 16) |
           (static_cast<uint32>(buffer[3]) << 24);
  return buffer + sizeof(*value);
#endif
}

inline const uint8* CodedInputStream::ReadLittleEndian64FromArray(
    const uint8* buffer,
    uint64* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(value, buffer, sizeof(*value));
  return buffer + sizeof(*value);
#else
  uint32 part0 = (static_cast<uint32>(buffer[0])      ) |
                 (static_cast<uint32>(buffer[1]) <<  8) |
                 (static_cast<uint32>(buffer[2]) << 16) |
                 (static_cast<uint32>(buffer[3]) << 24);
  uint32 part1 = (static_cast<uint32>(buffer[4])      ) |
                 (static_cast<uint32>(buffer[5]) <<  8) |
                 (static_cast<uint32>(buffer[6]) << 16) |
                 (static_cast<uint32>(buffer[7]) << 24);
  *value = static_cast<uint64>(part0) |
          (static_cast<uint64>(part1) << 32);
  return buffer + sizeof(*value);
#endif
}

inline bool CodedInputStream::ReadLittleEndian32(uint32* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  if (GOOGLE_PREDICT_TRUE(BufferSize() >= static_cast<int>(sizeof(*value)))) {
    memcpy(value, buffer_, sizeof(*value));
    Advance(sizeof(*value));
    return true;
  } else {
    return ReadLittleEndian32Fallback(value);
  }
#else
  return ReadLittleEndian32Fallback(value);
#endif
}

inline bool CodedInputStream::ReadLittleEndian64(uint64* value) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  if (GOOGLE_PREDICT_TRUE(BufferSize() >= static_cast<int>(sizeof(*value)))) {
    memcpy(value, buffer_, sizeof(*value));
    Advance(sizeof(*value));
    return true;
  } else {
    return ReadLittleEndian64Fallback(value);
  }
#else
  return ReadLittleEndian64Fallback(value);
#endif
}

inline uint32 CodedInputStream::ReadTag() {
  if (GOOGLE_PREDICT_TRUE(buffer_ < buffer_end_) && buffer_[0] < 0x80) {
    last_tag_ = buffer_[0];
    Advance(1);
    return last_tag_;
  } else {
    last_tag_ = ReadTagFallback();
    return last_tag_;
  }
}

inline bool CodedInputStream::LastTagWas(uint32 expected) {
  return last_tag_ == expected;
}

inline bool CodedInputStream::ConsumedEntireMessage() {
  return legitimate_message_end_;
}

inline bool CodedInputStream::ExpectTag(uint32 expected) {
  if (expected < (1 << 7)) {
    if (GOOGLE_PREDICT_TRUE(buffer_ < buffer_end_) && buffer_[0] == expected) {
      Advance(1);
      return true;
    } else {
      return false;
    }
  } else if (expected < (1 << 14)) {
    if (GOOGLE_PREDICT_TRUE(BufferSize() >= 2) &&
        buffer_[0] == static_cast<uint8>(expected | 0x80) &&
        buffer_[1] == static_cast<uint8>(expected >> 7)) {
      Advance(2);
      return true;
    } else {
      return false;
    }
  } else {

    return false;
  }
}

inline const uint8* CodedInputStream::ExpectTagFromArray(
    const uint8* buffer, uint32 expected) {
  if (expected < (1 << 7)) {
    if (buffer[0] == expected) {
      return buffer + 1;
    }
  } else if (expected < (1 << 14)) {
    if (buffer[0] == static_cast<uint8>(expected | 0x80) &&
        buffer[1] == static_cast<uint8>(expected >> 7)) {
      return buffer + 2;
    }
  }
  return NULL;
}

inline void CodedInputStream::GetDirectBufferPointerInline(const void** data,
                                                           int* size) {
  *data = buffer_;
  *size = buffer_end_ - buffer_;
}

inline bool CodedInputStream::ExpectAtEnd() {

  if (buffer_ == buffer_end_ && buffer_size_after_limit_ != 0) {
    last_tag_ = 0;
    legitimate_message_end_ = true;
    return true;
  } else {
    return false;
  }
}

inline uint8* CodedOutputStream::GetDirectBufferForNBytesAndAdvance(int size) {
  if (buffer_size_ < size) {
    return NULL;
  } else {
    uint8* result = buffer_;
    Advance(size);
    return result;
  }
}

inline uint8* CodedOutputStream::WriteVarint32ToArray(uint32 value,
                                                        uint8* target) {
  if (value < 0x80) {
    *target = value;
    return target + 1;
  } else {
    return WriteVarint32FallbackToArray(value, target);
  }
}

inline void CodedOutputStream::WriteVarint32SignExtended(int32 value) {
  if (value < 0) {
    WriteVarint64(static_cast<uint64>(value));
  } else {
    WriteVarint32(static_cast<uint32>(value));
  }
}

inline uint8* CodedOutputStream::WriteVarint32SignExtendedToArray(
    int32 value, uint8* target) {
  if (value < 0) {
    return WriteVarint64ToArray(static_cast<uint64>(value), target);
  } else {
    return WriteVarint32ToArray(static_cast<uint32>(value), target);
  }
}

inline uint8* CodedOutputStream::WriteLittleEndian32ToArray(uint32 value,
                                                            uint8* target) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(target, &value, sizeof(value));
#else
  target[0] = static_cast<uint8>(value);
  target[1] = static_cast<uint8>(value >>  8);
  target[2] = static_cast<uint8>(value >> 16);
  target[3] = static_cast<uint8>(value >> 24);
#endif
  return target + sizeof(value);
}

inline uint8* CodedOutputStream::WriteLittleEndian64ToArray(uint64 value,
                                                            uint8* target) {
#if defined(PROTOBUF_LITTLE_ENDIAN)
  memcpy(target, &value, sizeof(value));
#else
  uint32 part0 = static_cast<uint32>(value);
  uint32 part1 = static_cast<uint32>(value >> 32);

  target[0] = static_cast<uint8>(part0);
  target[1] = static_cast<uint8>(part0 >>  8);
  target[2] = static_cast<uint8>(part0 >> 16);
  target[3] = static_cast<uint8>(part0 >> 24);
  target[4] = static_cast<uint8>(part1);
  target[5] = static_cast<uint8>(part1 >>  8);
  target[6] = static_cast<uint8>(part1 >> 16);
  target[7] = static_cast<uint8>(part1 >> 24);
#endif
  return target + sizeof(value);
}

inline void CodedOutputStream::WriteTag(uint32 value) {
  WriteVarint32(value);
}

inline uint8* CodedOutputStream::WriteTagToArray(
    uint32 value, uint8* target) {
  if (value < (1 << 7)) {
    target[0] = value;
    return target + 1;
  } else if (value < (1 << 14)) {
    target[0] = static_cast<uint8>(value | 0x80);
    target[1] = static_cast<uint8>(value >> 7);
    return target + 2;
  } else {
    return WriteVarint32FallbackToArray(value, target);
  }
}

inline int CodedOutputStream::VarintSize32(uint32 value) {
  if (value < (1 << 7)) {
    return 1;
  } else  {
    return VarintSize32Fallback(value);
  }
}

inline int CodedOutputStream::VarintSize32SignExtended(int32 value) {
  if (value < 0) {
    return 10;
  } else {
    return VarintSize32(static_cast<uint32>(value));
  }
}

inline void CodedOutputStream::WriteString(const string& str) {
  WriteRaw(str.data(), static_cast<int>(str.size()));
}

inline uint8* CodedOutputStream::WriteStringToArray(
    const string& str, uint8* target) {
  return WriteRawToArray(str.data(), static_cast<int>(str.size()), target);
}

inline int CodedOutputStream::ByteCount() const {
  return total_bytes_ - buffer_size_;
}

inline void CodedInputStream::Advance(int amount) {
  buffer_ += amount;
}

inline void CodedOutputStream::Advance(int amount) {
  buffer_ += amount;
  buffer_size_ -= amount;
}

inline void CodedInputStream::SetRecursionLimit(int limit) {
  recursion_limit_ = limit;
}

inline bool CodedInputStream::IncrementRecursionDepth() {
  ++recursion_depth_;
  return recursion_depth_ <= recursion_limit_;
}

inline void CodedInputStream::DecrementRecursionDepth() {
  if (recursion_depth_ > 0) --recursion_depth_;
}

inline void CodedInputStream::SetExtensionRegistry(DescriptorPool* pool,
                                                   MessageFactory* factory) {
  extension_pool_ = pool;
  extension_factory_ = factory;
}

inline const DescriptorPool* CodedInputStream::GetExtensionPool() {
  return extension_pool_;
}

inline MessageFactory* CodedInputStream::GetExtensionFactory() {
  return extension_factory_;
}

inline int CodedInputStream::BufferSize() const {
  return buffer_end_ - buffer_;
}

inline CodedInputStream::CodedInputStream(ZeroCopyInputStream* input)
  : input_(input),
    buffer_(NULL),
    buffer_end_(NULL),
    total_bytes_read_(0),
    overflow_bytes_(0),
    last_tag_(0),
    legitimate_message_end_(false),
    aliasing_enabled_(false),
    current_limit_(kint32max),
    buffer_size_after_limit_(0),
    total_bytes_limit_(kDefaultTotalBytesLimit),
    total_bytes_warning_threshold_(kDefaultTotalBytesWarningThreshold),
    recursion_depth_(0),
    recursion_limit_(kDefaultRecursionLimit),
    extension_pool_(NULL),
    extension_factory_(NULL) {

  Refresh();
}

inline CodedInputStream::CodedInputStream(const uint8* buffer, int size)
  : input_(NULL),
    buffer_(buffer),
    buffer_end_(buffer + size),
    total_bytes_read_(size),
    overflow_bytes_(0),
    last_tag_(0),
    legitimate_message_end_(false),
    aliasing_enabled_(false),
    current_limit_(size),
    buffer_size_after_limit_(0),
    total_bytes_limit_(kDefaultTotalBytesLimit),
    total_bytes_warning_threshold_(kDefaultTotalBytesWarningThreshold),
    recursion_depth_(0),
    recursion_limit_(kDefaultRecursionLimit),
    extension_pool_(NULL),
    extension_factory_(NULL) {

}

inline CodedInputStream::~CodedInputStream() {
  if (input_ != NULL) {
    BackUpInputToCurrentPosition();
  }
}

}
}

#if defined(_MSC_VER) && _MSC_VER >= 1300
  #pragma runtime_checks("c", restore)
#endif

}
#endif
