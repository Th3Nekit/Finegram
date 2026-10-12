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
//         atenasio@google.com (Chris Atenasio) (ZigZag transform)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef GOOGLE_PROTOBUF_WIRE_FORMAT_H__
#define GOOGLE_PROTOBUF_WIRE_FORMAT_H__

#include <string>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/wire_format_lite.h>

#ifndef NDEBUG
#define GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
#endif

namespace google {
namespace protobuf {
  namespace io {
    class CodedInputStream;
    class CodedOutputStream;
  }
  class UnknownFieldSet;
}

namespace protobuf {
namespace internal {

class LIBPROTOBUF_EXPORT WireFormat {
 public:

  static inline WireFormatLite::WireType WireTypeForField(
      const FieldDescriptor* field);

  static inline WireFormatLite::WireType WireTypeForFieldType(
      FieldDescriptor::Type type);

  static inline int TagSize(int field_number, FieldDescriptor::Type type);

  static bool ParseAndMergePartial(io::CodedInputStream* input,
                                   Message* message);

  static void SerializeWithCachedSizes(
      const Message& message,
      int size, io::CodedOutputStream* output);

  static int ByteSize(const Message& message);

  static bool SkipField(io::CodedInputStream* input, uint32 tag,
                        UnknownFieldSet* unknown_fields);

  static bool SkipMessage(io::CodedInputStream* input,
                          UnknownFieldSet* unknown_fields);

  static void SerializeUnknownFields(const UnknownFieldSet& unknown_fields,
                                     io::CodedOutputStream* output);

  static uint8* SerializeUnknownFieldsToArray(
      const UnknownFieldSet& unknown_fields,
      uint8* target);

  static void SerializeUnknownMessageSetItems(
      const UnknownFieldSet& unknown_fields,
      io::CodedOutputStream* output);

  static uint8* SerializeUnknownMessageSetItemsToArray(
      const UnknownFieldSet& unknown_fields,
      uint8* target);

  static int ComputeUnknownFieldsSize(const UnknownFieldSet& unknown_fields);

  static int ComputeUnknownMessageSetItemsSize(
      const UnknownFieldSet& unknown_fields);

  static uint32 MakeTag(const FieldDescriptor* field);

  static bool ParseAndMergeField(
      uint32 tag,
      const FieldDescriptor* field,
      Message* message,
      io::CodedInputStream* input);

  static void SerializeFieldWithCachedSizes(
      const FieldDescriptor* field,
      const Message& message,
      io::CodedOutputStream* output);

  static int FieldByteSize(
      const FieldDescriptor* field,
      const Message& message);

  static bool ParseAndMergeMessageSetItem(
      io::CodedInputStream* input,
      Message* message);
  static void SerializeMessageSetItemWithCachedSizes(
      const FieldDescriptor* field,
      const Message& message,
      io::CodedOutputStream* output);
  static int MessageSetItemByteSize(
      const FieldDescriptor* field,
      const Message& message);

  static int FieldDataOnlyByteSize(
      const FieldDescriptor* field,
      const Message& message);

  enum Operation {
    PARSE,
    SERIALIZE,
  };

  static void VerifyUTF8String(const char* data, int size, Operation op);

 private:

  static void VerifyUTF8StringFallback(
      const char* data,
      int size,
      Operation op);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(WireFormat);
};

class LIBPROTOBUF_EXPORT UnknownFieldSetFieldSkipper : public FieldSkipper {
 public:
  UnknownFieldSetFieldSkipper(UnknownFieldSet* unknown_fields)
      : unknown_fields_(unknown_fields) {}
  virtual ~UnknownFieldSetFieldSkipper() {}

  virtual bool SkipField(io::CodedInputStream* input, uint32 tag);
  virtual bool SkipMessage(io::CodedInputStream* input);
  virtual void SkipUnknownEnum(int field_number, int value);

 private:
  UnknownFieldSet* unknown_fields_;
};

inline WireFormatLite::WireType WireFormat::WireTypeForField(
    const FieldDescriptor* field) {
  if (field->options().packed()) {
    return WireFormatLite::WIRETYPE_LENGTH_DELIMITED;
  } else {
    return WireTypeForFieldType(field->type());
  }
}

inline WireFormatLite::WireType WireFormat::WireTypeForFieldType(
    FieldDescriptor::Type type) {

  return WireFormatLite::WireTypeForFieldType(
      static_cast<WireFormatLite::FieldType>(
        implicit_cast<int>(type)));
}

inline uint32 WireFormat::MakeTag(const FieldDescriptor* field) {
  return WireFormatLite::MakeTag(field->number(), WireTypeForField(field));
}

inline int WireFormat::TagSize(int field_number, FieldDescriptor::Type type) {

  return WireFormatLite::TagSize(field_number,
      static_cast<WireFormatLite::FieldType>(
        implicit_cast<int>(type)));
}

inline void WireFormat::VerifyUTF8String(const char* data, int size,
    WireFormat::Operation op) {
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  WireFormat::VerifyUTF8StringFallback(data, size, op);
#endif
}

}
}

}
#endif
