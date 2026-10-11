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
// Contains classes used to keep track of unrecognized fields seen while
// parsing a protocol message.

#ifndef GOOGLE_PROTOBUF_UNKNOWN_FIELD_SET_H__
#define GOOGLE_PROTOBUF_UNKNOWN_FIELD_SET_H__

#include <string>
#include <vector>
#include <google/protobuf/repeated_field.h>

namespace google {
namespace protobuf {

class Message;
class UnknownField;

class LIBPROTOBUF_EXPORT UnknownFieldSet {
 public:
  UnknownFieldSet();
  ~UnknownFieldSet();

  inline void Clear();

  inline bool empty() const;

  void MergeFrom(const UnknownFieldSet& other);

  inline void Swap(UnknownFieldSet* x);

  int SpaceUsedExcludingSelf() const;

  int SpaceUsed() const;

  inline int field_count() const;

  inline const UnknownField& field(int index) const;

  inline UnknownField* mutable_field(int index);

  void AddVarint(int number, uint64 value);
  void AddFixed32(int number, uint32 value);
  void AddFixed64(int number, uint64 value);
  void AddLengthDelimited(int number, const string& value);
  string* AddLengthDelimited(int number);
  UnknownFieldSet* AddGroup(int number);

  void AddField(const UnknownField& field);

  bool MergeFromCodedStream(io::CodedInputStream* input);
  bool ParseFromCodedStream(io::CodedInputStream* input);
  bool ParseFromZeroCopyStream(io::ZeroCopyInputStream* input);
  bool ParseFromArray(const void* data, int size);
  inline bool ParseFromString(const string& data) {
    return ParseFromArray(data.data(), data.size());
  }

 private:
  void ClearFallback();

  vector<UnknownField>* fields_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(UnknownFieldSet);
};

class LIBPROTOBUF_EXPORT UnknownField {
 public:
  enum Type {
    TYPE_VARINT,
    TYPE_FIXED32,
    TYPE_FIXED64,
    TYPE_LENGTH_DELIMITED,
    TYPE_GROUP
  };

  inline int number() const;

  inline Type type() const;

  inline uint64 varint() const;
  inline uint32 fixed32() const;
  inline uint64 fixed64() const;
  inline const string& length_delimited() const;
  inline const UnknownFieldSet& group() const;

  inline void set_varint(uint64 value);
  inline void set_fixed32(uint32 value);
  inline void set_fixed64(uint64 value);
  inline void set_length_delimited(const string& value);
  inline string* mutable_length_delimited();
  inline UnknownFieldSet* mutable_group();

 private:
  friend class UnknownFieldSet;

  void Delete();

  void DeepCopy();

  unsigned int number_ : 29;
  unsigned int type_   : 3;
  union {
    uint64 varint_;
    uint32 fixed32_;
    uint64 fixed64_;
    string* length_delimited_;
    UnknownFieldSet* group_;
  };
};

inline void UnknownFieldSet::Clear() {
  if (fields_ != NULL) {
    ClearFallback();
  }
}

inline bool UnknownFieldSet::empty() const {
  return fields_ == NULL || fields_->empty();
}

inline void UnknownFieldSet::Swap(UnknownFieldSet* x) {
  std::swap(fields_, x->fields_);
}

inline int UnknownFieldSet::field_count() const {
  return (fields_ == NULL) ? 0 : fields_->size();
}
inline const UnknownField& UnknownFieldSet::field(int index) const {
  return (*fields_)[index];
}
inline UnknownField* UnknownFieldSet::mutable_field(int index) {
  return &(*fields_)[index];
}

inline void UnknownFieldSet::AddLengthDelimited(
    int number, const string& value) {
  AddLengthDelimited(number)->assign(value);
}

inline int UnknownField::number() const { return number_; }
inline UnknownField::Type UnknownField::type() const {
  return static_cast<Type>(type_);
}

inline uint64 UnknownField::varint () const {
  GOOGLE_DCHECK_EQ(type_, TYPE_VARINT);
  return varint_;
}
inline uint32 UnknownField::fixed32() const {
  GOOGLE_DCHECK_EQ(type_, TYPE_FIXED32);
  return fixed32_;
}
inline uint64 UnknownField::fixed64() const {
  GOOGLE_DCHECK_EQ(type_, TYPE_FIXED64);
  return fixed64_;
}
inline const string& UnknownField::length_delimited() const {
  GOOGLE_DCHECK_EQ(type_, TYPE_LENGTH_DELIMITED);
  return *length_delimited_;
}
inline const UnknownFieldSet& UnknownField::group() const {
  GOOGLE_DCHECK_EQ(type_, TYPE_GROUP);
  return *group_;
}

inline void UnknownField::set_varint(uint64 value) {
  GOOGLE_DCHECK_EQ(type_, TYPE_VARINT);
  varint_ = value;
}
inline void UnknownField::set_fixed32(uint32 value) {
  GOOGLE_DCHECK_EQ(type_, TYPE_FIXED32);
  fixed32_ = value;
}
inline void UnknownField::set_fixed64(uint64 value) {
  GOOGLE_DCHECK_EQ(type_, TYPE_FIXED64);
  fixed64_ = value;
}
inline void UnknownField::set_length_delimited(const string& value) {
  GOOGLE_DCHECK_EQ(type_, TYPE_LENGTH_DELIMITED);
  length_delimited_->assign(value);
}
inline string* UnknownField::mutable_length_delimited() {
  GOOGLE_DCHECK_EQ(type_, TYPE_LENGTH_DELIMITED);
  return length_delimited_;
}
inline UnknownFieldSet* UnknownField::mutable_group() {
  GOOGLE_DCHECK_EQ(type_, TYPE_GROUP);
  return group_;
}

}

}
#endif
