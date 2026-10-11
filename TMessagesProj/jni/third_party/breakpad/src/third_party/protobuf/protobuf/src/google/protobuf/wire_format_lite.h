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
//         wink@google.com (Wink Saville) (refactored from wire_format.h)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This header is logically internal, but is made public because it is used
// from protocol-compiler-generated code, which may reside in other components.

#ifndef GOOGLE_PROTOBUF_WIRE_FORMAT_LITE_H__
#define GOOGLE_PROTOBUF_WIRE_FORMAT_LITE_H__

#include <string>
#include <google/protobuf/message_lite.h>

namespace google {

namespace protobuf {
  template <typename T> class RepeatedField;
  namespace io {
    class CodedInputStream;
    class CodedOutputStream;
  }
}

namespace protobuf {
namespace internal {

class StringPieceField;

class LIBPROTOBUF_EXPORT WireFormatLite {
 public:

  enum WireType {
    WIRETYPE_VARINT           = 0,
    WIRETYPE_FIXED64          = 1,
    WIRETYPE_LENGTH_DELIMITED = 2,
    WIRETYPE_START_GROUP      = 3,
    WIRETYPE_END_GROUP        = 4,
    WIRETYPE_FIXED32          = 5,
  };

  enum FieldType {
    TYPE_DOUBLE         = 1,
    TYPE_FLOAT          = 2,
    TYPE_INT64          = 3,
    TYPE_UINT64         = 4,
    TYPE_INT32          = 5,
    TYPE_FIXED64        = 6,
    TYPE_FIXED32        = 7,
    TYPE_BOOL           = 8,
    TYPE_STRING         = 9,
    TYPE_GROUP          = 10,
    TYPE_MESSAGE        = 11,
    TYPE_BYTES          = 12,
    TYPE_UINT32         = 13,
    TYPE_ENUM           = 14,
    TYPE_SFIXED32       = 15,
    TYPE_SFIXED64       = 16,
    TYPE_SINT32         = 17,
    TYPE_SINT64         = 18,
    MAX_FIELD_TYPE      = 18,
  };

  enum CppType {
    CPPTYPE_INT32       = 1,
    CPPTYPE_INT64       = 2,
    CPPTYPE_UINT32      = 3,
    CPPTYPE_UINT64      = 4,
    CPPTYPE_DOUBLE      = 5,
    CPPTYPE_FLOAT       = 6,
    CPPTYPE_BOOL        = 7,
    CPPTYPE_ENUM        = 8,
    CPPTYPE_STRING      = 9,
    CPPTYPE_MESSAGE     = 10,
    MAX_CPPTYPE         = 10,
  };

  static CppType FieldTypeToCppType(FieldType type);

  static inline WireFormatLite::WireType WireTypeForFieldType(
      WireFormatLite::FieldType type) {
    return kWireTypeForFieldType[type];
  }

  static const int kTagTypeBits = 3;

  static const uint32 kTagTypeMask = (1 << kTagTypeBits) - 1;

  static uint32 MakeTag(int field_number, WireType type);
  static WireType GetTagWireType(uint32 tag);
  static int GetTagFieldNumber(uint32 tag);

  static inline int TagSize(int field_number, WireFormatLite::FieldType type);

  static bool SkipField(io::CodedInputStream* input, uint32 tag);

  static bool SkipMessage(io::CodedInputStream* input);

#define GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(FIELD_NUMBER, TYPE)                  \
  static_cast<uint32>(                                                   \
    ((FIELD_NUMBER) << ::google::protobuf::internal::WireFormatLite::kTagTypeBits) \
      | (TYPE))

  static const int kMessageSetItemNumber = 1;
  static const int kMessageSetTypeIdNumber = 2;
  static const int kMessageSetMessageNumber = 3;
  static const int kMessageSetItemStartTag =
    GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(kMessageSetItemNumber,
                                WireFormatLite::WIRETYPE_START_GROUP);
  static const int kMessageSetItemEndTag =
    GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(kMessageSetItemNumber,
                                WireFormatLite::WIRETYPE_END_GROUP);
  static const int kMessageSetTypeIdTag =
    GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(kMessageSetTypeIdNumber,
                                WireFormatLite::WIRETYPE_VARINT);
  static const int kMessageSetMessageTag =
    GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(kMessageSetMessageNumber,
                                WireFormatLite::WIRETYPE_LENGTH_DELIMITED);

  static const int kMessageSetItemTagsSize;

  static uint32 EncodeFloat(float value);
  static float DecodeFloat(uint32 value);
  static uint64 EncodeDouble(double value);
  static double DecodeDouble(uint64 value);

  static uint32 ZigZagEncode32(int32 n);
  static int32  ZigZagDecode32(uint32 n);
  static uint64 ZigZagEncode64(int64 n);
  static int64  ZigZagDecode64(uint64 n);

#define input  io::CodedInputStream*  input
#define output io::CodedOutputStream* output
#define field_number int field_number
#define INL GOOGLE_ATTRIBUTE_ALWAYS_INLINE

  template <typename CType, enum FieldType DeclaredType>
  static inline bool ReadPrimitive(input, CType* value) INL;

  template <typename CType, enum FieldType DeclaredType>
  static inline bool ReadRepeatedPrimitive(int tag_size,
                                           uint32 tag,
                                           input,
                                           RepeatedField<CType>* value) INL;

  template <typename CType, enum FieldType DeclaredType>
  static bool ReadRepeatedPrimitiveNoInline(int tag_size,
                                            uint32 tag,
                                            input,
                                            RepeatedField<CType>* value);

  template <typename CType, enum FieldType DeclaredType>
  static inline const uint8* ReadPrimitiveFromArray(const uint8* buffer,
                                                    CType* value) INL;

  template <typename CType, enum FieldType DeclaredType>
  static inline bool ReadPackedPrimitive(input,
                                         RepeatedField<CType>* value) INL;

  template <typename CType, enum FieldType DeclaredType>
  static bool ReadPackedPrimitiveNoInline(input, RepeatedField<CType>* value);

  static bool ReadPackedEnumNoInline(input,
                                     bool (*is_valid)(int),
                                     RepeatedField<int>* value);

  static bool ReadString(input, string* value);
  static bool ReadBytes (input, string* value);

  static inline bool ReadGroup  (field_number, input, MessageLite* value);
  static inline bool ReadMessage(input, MessageLite* value);

  template<typename MessageType>
  static inline bool ReadGroupNoVirtual(field_number, input,
                                        MessageType* value);
  template<typename MessageType>
  static inline bool ReadMessageNoVirtual(input, MessageType* value);

  static inline void WriteTag(field_number, WireType type, output) INL;

  static inline void WriteInt32NoTag   (int32 value, output) INL;
  static inline void WriteInt64NoTag   (int64 value, output) INL;
  static inline void WriteUInt32NoTag  (uint32 value, output) INL;
  static inline void WriteUInt64NoTag  (uint64 value, output) INL;
  static inline void WriteSInt32NoTag  (int32 value, output) INL;
  static inline void WriteSInt64NoTag  (int64 value, output) INL;
  static inline void WriteFixed32NoTag (uint32 value, output) INL;
  static inline void WriteFixed64NoTag (uint64 value, output) INL;
  static inline void WriteSFixed32NoTag(int32 value, output) INL;
  static inline void WriteSFixed64NoTag(int64 value, output) INL;
  static inline void WriteFloatNoTag   (float value, output) INL;
  static inline void WriteDoubleNoTag  (double value, output) INL;
  static inline void WriteBoolNoTag    (bool value, output) INL;
  static inline void WriteEnumNoTag    (int value, output) INL;

  static void WriteInt32   (field_number,  int32 value, output);
  static void WriteInt64   (field_number,  int64 value, output);
  static void WriteUInt32  (field_number, uint32 value, output);
  static void WriteUInt64  (field_number, uint64 value, output);
  static void WriteSInt32  (field_number,  int32 value, output);
  static void WriteSInt64  (field_number,  int64 value, output);
  static void WriteFixed32 (field_number, uint32 value, output);
  static void WriteFixed64 (field_number, uint64 value, output);
  static void WriteSFixed32(field_number,  int32 value, output);
  static void WriteSFixed64(field_number,  int64 value, output);
  static void WriteFloat   (field_number,  float value, output);
  static void WriteDouble  (field_number, double value, output);
  static void WriteBool    (field_number,   bool value, output);
  static void WriteEnum    (field_number,    int value, output);

  static void WriteString(field_number, const string& value, output);
  static void WriteBytes (field_number, const string& value, output);

  static void WriteGroup(
    field_number, const MessageLite& value, output);
  static void WriteMessage(
    field_number, const MessageLite& value, output);

  static void WriteGroupMaybeToArray(
    field_number, const MessageLite& value, output);
  static void WriteMessageMaybeToArray(
    field_number, const MessageLite& value, output);

  template<typename MessageType>
  static inline void WriteGroupNoVirtual(
    field_number, const MessageType& value, output);
  template<typename MessageType>
  static inline void WriteMessageNoVirtual(
    field_number, const MessageType& value, output);

#undef output
#define output uint8* target

  static inline uint8* WriteTagToArray(field_number, WireType type, output) INL;

  static inline uint8* WriteInt32NoTagToArray   (int32 value, output) INL;
  static inline uint8* WriteInt64NoTagToArray   (int64 value, output) INL;
  static inline uint8* WriteUInt32NoTagToArray  (uint32 value, output) INL;
  static inline uint8* WriteUInt64NoTagToArray  (uint64 value, output) INL;
  static inline uint8* WriteSInt32NoTagToArray  (int32 value, output) INL;
  static inline uint8* WriteSInt64NoTagToArray  (int64 value, output) INL;
  static inline uint8* WriteFixed32NoTagToArray (uint32 value, output) INL;
  static inline uint8* WriteFixed64NoTagToArray (uint64 value, output) INL;
  static inline uint8* WriteSFixed32NoTagToArray(int32 value, output) INL;
  static inline uint8* WriteSFixed64NoTagToArray(int64 value, output) INL;
  static inline uint8* WriteFloatNoTagToArray   (float value, output) INL;
  static inline uint8* WriteDoubleNoTagToArray  (double value, output) INL;
  static inline uint8* WriteBoolNoTagToArray    (bool value, output) INL;
  static inline uint8* WriteEnumNoTagToArray    (int value, output) INL;

  static inline uint8* WriteInt32ToArray(
    field_number, int32 value, output) INL;
  static inline uint8* WriteInt64ToArray(
    field_number, int64 value, output) INL;
  static inline uint8* WriteUInt32ToArray(
    field_number, uint32 value, output) INL;
  static inline uint8* WriteUInt64ToArray(
    field_number, uint64 value, output) INL;
  static inline uint8* WriteSInt32ToArray(
    field_number, int32 value, output) INL;
  static inline uint8* WriteSInt64ToArray(
    field_number, int64 value, output) INL;
  static inline uint8* WriteFixed32ToArray(
    field_number, uint32 value, output) INL;
  static inline uint8* WriteFixed64ToArray(
    field_number, uint64 value, output) INL;
  static inline uint8* WriteSFixed32ToArray(
    field_number, int32 value, output) INL;
  static inline uint8* WriteSFixed64ToArray(
    field_number, int64 value, output) INL;
  static inline uint8* WriteFloatToArray(
    field_number, float value, output) INL;
  static inline uint8* WriteDoubleToArray(
    field_number, double value, output) INL;
  static inline uint8* WriteBoolToArray(
    field_number, bool value, output) INL;
  static inline uint8* WriteEnumToArray(
    field_number, int value, output) INL;

  static inline uint8* WriteStringToArray(
    field_number, const string& value, output) INL;
  static inline uint8* WriteBytesToArray(
    field_number, const string& value, output) INL;

  static inline uint8* WriteGroupToArray(
      field_number, const MessageLite& value, output) INL;
  static inline uint8* WriteMessageToArray(
      field_number, const MessageLite& value, output) INL;

  template<typename MessageType>
  static inline uint8* WriteGroupNoVirtualToArray(
    field_number, const MessageType& value, output) INL;
  template<typename MessageType>
  static inline uint8* WriteMessageNoVirtualToArray(
    field_number, const MessageType& value, output) INL;

#undef output
#undef input
#undef INL

#undef field_number

  static inline int Int32Size   ( int32 value);
  static inline int Int64Size   ( int64 value);
  static inline int UInt32Size  (uint32 value);
  static inline int UInt64Size  (uint64 value);
  static inline int SInt32Size  ( int32 value);
  static inline int SInt64Size  ( int64 value);
  static inline int EnumSize    (   int value);

  static const int kFixed32Size  = 4;
  static const int kFixed64Size  = 8;
  static const int kSFixed32Size = 4;
  static const int kSFixed64Size = 8;
  static const int kFloatSize    = 4;
  static const int kDoubleSize   = 8;
  static const int kBoolSize     = 1;

  static inline int StringSize(const string& value);
  static inline int BytesSize (const string& value);

  static inline int GroupSize  (const MessageLite& value);
  static inline int MessageSize(const MessageLite& value);

  template<typename MessageType>
  static inline int GroupSizeNoVirtual  (const MessageType& value);
  template<typename MessageType>
  static inline int MessageSizeNoVirtual(const MessageType& value);

 private:

  template <typename CType, enum FieldType DeclaredType>
  static inline bool ReadRepeatedFixedSizePrimitive(
      int tag_size,
      uint32 tag,
      google::protobuf::io::CodedInputStream* input,
      RepeatedField<CType>* value) GOOGLE_ATTRIBUTE_ALWAYS_INLINE;

  static const CppType kFieldTypeToCppTypeMap[];
  static const WireFormatLite::WireType kWireTypeForFieldType[];

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(WireFormatLite);
};

class LIBPROTOBUF_EXPORT FieldSkipper {
 public:
  FieldSkipper() {}
  virtual ~FieldSkipper() {}

  virtual bool SkipField(io::CodedInputStream* input, uint32 tag);

  virtual bool SkipMessage(io::CodedInputStream* input);

  virtual void SkipUnknownEnum(int field_number, int value);
};

inline WireFormatLite::CppType
WireFormatLite::FieldTypeToCppType(FieldType type) {
  return kFieldTypeToCppTypeMap[type];
}

inline uint32 WireFormatLite::MakeTag(int field_number, WireType type) {
  return GOOGLE_PROTOBUF_WIRE_FORMAT_MAKE_TAG(field_number, type);
}

inline WireFormatLite::WireType WireFormatLite::GetTagWireType(uint32 tag) {
  return static_cast<WireType>(tag & kTagTypeMask);
}

inline int WireFormatLite::GetTagFieldNumber(uint32 tag) {
  return static_cast<int>(tag >> kTagTypeBits);
}

inline int WireFormatLite::TagSize(int field_number,
                                   WireFormatLite::FieldType type) {
  int result = io::CodedOutputStream::VarintSize32(
    field_number << kTagTypeBits);
  if (type == TYPE_GROUP) {

    return result * 2;
  } else {
    return result;
  }
}

inline uint32 WireFormatLite::EncodeFloat(float value) {
  union {float f; uint32 i;};
  f = value;
  return i;
}

inline float WireFormatLite::DecodeFloat(uint32 value) {
  union {float f; uint32 i;};
  i = value;
  return f;
}

inline uint64 WireFormatLite::EncodeDouble(double value) {
  union {double f; uint64 i;};
  f = value;
  return i;
}

inline double WireFormatLite::DecodeDouble(uint64 value) {
  union {double f; uint64 i;};
  i = value;
  return f;
}

inline uint32 WireFormatLite::ZigZagEncode32(int32 n) {

  return (n << 1) ^ (n >> 31);
}

inline int32 WireFormatLite::ZigZagDecode32(uint32 n) {
  return (n >> 1) ^ -static_cast<int32>(n & 1);
}

inline uint64 WireFormatLite::ZigZagEncode64(int64 n) {

  return (n << 1) ^ (n >> 63);
}

inline int64 WireFormatLite::ZigZagDecode64(uint64 n) {
  return (n >> 1) ^ -static_cast<int64>(n & 1);
}

}
}

}
#endif
