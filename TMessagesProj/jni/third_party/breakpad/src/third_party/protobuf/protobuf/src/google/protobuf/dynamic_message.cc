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
// DynamicMessage is implemented by constructing a data structure which
// has roughly the same memory layout as a generated message would have.
// Then, we use GeneratedMessageReflection to implement our reflection
// interface.  All the other operations we need to implement (e.g.
// parsing, copying, etc.) are already implemented in terms of
// Reflection, so the rest is easy.
//
// The up side of this strategy is that it's very efficient.  We don't
// need to use hash_maps or generic representations of fields.  The
// down side is that this is a low-level memory management hack which
// can be tricky to get right.
//
// As mentioned in the header, we only expose a DynamicMessageFactory
// publicly, not the DynamicMessage class itself.  This is because
// GenericMessageReflection wants to have a pointer to a "default"
// copy of the class, with all fields initialized to their default
// values.  We only want to construct one of these per message type,
// so DynamicMessageFactory stores a cache of default messages for
// each type it sees (each unique Descriptor pointer).  The code
// refers to the "default" copy of the class as the "prototype".
//
// Note on memory allocation:  This module often calls "operator new()"
// to allocate untyped memory, rather than calling something like
// "new uint8[]".  This is because "operator new()" means "Give me some
// space which I can use as I please." while "new uint8[]" means "Give
// me an array of 8-bit integers.".  In practice, the later may return
// a pointer that is not aligned correctly for general use.  I believe
// Item 8 of "More Effective C++" discusses this in more detail, though
// I don't have the book on me right now so I'm not sure.

#include <algorithm>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/stubs/common.h>

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format.h>

namespace google {
namespace protobuf {

using internal::WireFormat;
using internal::ExtensionSet;
using internal::GeneratedMessageReflection;

namespace {

int FieldSpaceUsed(const FieldDescriptor* field) {
  typedef FieldDescriptor FD;
  if (field->label() == FD::LABEL_REPEATED) {
    switch (field->cpp_type()) {
      case FD::CPPTYPE_INT32  : return sizeof(RepeatedField<int32   >);
      case FD::CPPTYPE_INT64  : return sizeof(RepeatedField<int64   >);
      case FD::CPPTYPE_UINT32 : return sizeof(RepeatedField<uint32  >);
      case FD::CPPTYPE_UINT64 : return sizeof(RepeatedField<uint64  >);
      case FD::CPPTYPE_DOUBLE : return sizeof(RepeatedField<double  >);
      case FD::CPPTYPE_FLOAT  : return sizeof(RepeatedField<float   >);
      case FD::CPPTYPE_BOOL   : return sizeof(RepeatedField<bool    >);
      case FD::CPPTYPE_ENUM   : return sizeof(RepeatedField<int     >);
      case FD::CPPTYPE_MESSAGE: return sizeof(RepeatedPtrField<Message>);

      case FD::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:
          case FieldOptions::STRING:
            return sizeof(RepeatedPtrField<string>);
        }
        break;
    }
  } else {
    switch (field->cpp_type()) {
      case FD::CPPTYPE_INT32  : return sizeof(int32   );
      case FD::CPPTYPE_INT64  : return sizeof(int64   );
      case FD::CPPTYPE_UINT32 : return sizeof(uint32  );
      case FD::CPPTYPE_UINT64 : return sizeof(uint64  );
      case FD::CPPTYPE_DOUBLE : return sizeof(double  );
      case FD::CPPTYPE_FLOAT  : return sizeof(float   );
      case FD::CPPTYPE_BOOL   : return sizeof(bool    );
      case FD::CPPTYPE_ENUM   : return sizeof(int     );
      case FD::CPPTYPE_MESSAGE: return sizeof(Message*);

      case FD::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:
          case FieldOptions::STRING:
            return sizeof(string*);
        }
        break;
    }
  }

  GOOGLE_LOG(DFATAL) << "Can't get here.";
  return 0;
}

inline int DivideRoundingUp(int i, int j) {
  return (i + (j - 1)) / j;
}

static const int kSafeAlignment = sizeof(uint64);

inline int AlignTo(int offset, int alignment) {
  return DivideRoundingUp(offset, alignment) * alignment;
}

inline int AlignOffset(int offset) {
  return AlignTo(offset, kSafeAlignment);
}

#define bitsizeof(T) (sizeof(T) * 8)

}

class DynamicMessage : public Message {
 public:
  struct TypeInfo {
    int size;
    int has_bits_offset;
    int unknown_fields_offset;
    int extensions_offset;

    DynamicMessageFactory* factory;
    const DescriptorPool* pool;
    const Descriptor* type;

    scoped_array<int> offsets;
    scoped_ptr<const GeneratedMessageReflection> reflection;
    scoped_ptr<const DynamicMessage> prototype;
  };

  DynamicMessage(const TypeInfo* type_info);
  ~DynamicMessage();

  void CrossLinkPrototypes();

  Message* New() const;

  int GetCachedSize() const;
  void SetCachedSize(int size) const;

  Metadata GetMetadata() const;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DynamicMessage);

  inline bool is_prototype() const {
    return type_info_->prototype == this ||

           type_info_->prototype == NULL;
  }

  inline void* OffsetToPointer(int offset) {
    return reinterpret_cast<uint8*>(this) + offset;
  }
  inline const void* OffsetToPointer(int offset) const {
    return reinterpret_cast<const uint8*>(this) + offset;
  }

  const TypeInfo* type_info_;

  mutable int cached_byte_size_;
};

DynamicMessage::DynamicMessage(const TypeInfo* type_info)
  : type_info_(type_info),
    cached_byte_size_(0) {

  const Descriptor* descriptor = type_info_->type;

  new(OffsetToPointer(type_info_->unknown_fields_offset)) UnknownFieldSet;

  if (type_info_->extensions_offset != -1) {
    new(OffsetToPointer(type_info_->extensions_offset)) ExtensionSet;
  }

  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);
    switch (field->cpp_type()) {
#define HANDLE_TYPE(CPPTYPE, TYPE)                                           \
      case FieldDescriptor::CPPTYPE_##CPPTYPE:                               \
        if (!field->is_repeated()) {                                         \
          new(field_ptr) TYPE(field->default_value_##TYPE());                \
        } else {                                                             \
          new(field_ptr) RepeatedField<TYPE>();                              \
        }                                                                    \
        break;

      HANDLE_TYPE(INT32 , int32 );
      HANDLE_TYPE(INT64 , int64 );
      HANDLE_TYPE(UINT32, uint32);
      HANDLE_TYPE(UINT64, uint64);
      HANDLE_TYPE(DOUBLE, double);
      HANDLE_TYPE(FLOAT , float );
      HANDLE_TYPE(BOOL  , bool  );
#undef HANDLE_TYPE

      case FieldDescriptor::CPPTYPE_ENUM:
        if (!field->is_repeated()) {
          new(field_ptr) int(field->default_value_enum()->number());
        } else {
          new(field_ptr) RepeatedField<int>();
        }
        break;

      case FieldDescriptor::CPPTYPE_STRING:
        switch (field->options().ctype()) {
          default:
          case FieldOptions::STRING:
            if (!field->is_repeated()) {
              if (is_prototype()) {
                new(field_ptr) const string*(&field->default_value_string());
              } else {
                string* default_value =
                  *reinterpret_cast<string* const*>(
                    type_info_->prototype->OffsetToPointer(
                      type_info_->offsets[i]));
                new(field_ptr) string*(default_value);
              }
            } else {
              new(field_ptr) RepeatedPtrField<string>();
            }
            break;
        }
        break;

      case FieldDescriptor::CPPTYPE_MESSAGE: {
        if (!field->is_repeated()) {
          new(field_ptr) Message*(NULL);
        } else {
          new(field_ptr) RepeatedPtrField<Message>();
        }
        break;
      }
    }
  }
}

DynamicMessage::~DynamicMessage() {
  const Descriptor* descriptor = type_info_->type;

  reinterpret_cast<UnknownFieldSet*>(
    OffsetToPointer(type_info_->unknown_fields_offset))->~UnknownFieldSet();

  if (type_info_->extensions_offset != -1) {
    reinterpret_cast<ExtensionSet*>(
      OffsetToPointer(type_info_->extensions_offset))->~ExtensionSet();
  }

  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);

    if (field->is_repeated()) {
      switch (field->cpp_type()) {
#define HANDLE_TYPE(UPPERCASE, LOWERCASE)                                     \
        case FieldDescriptor::CPPTYPE_##UPPERCASE :                           \
          reinterpret_cast<RepeatedField<LOWERCASE>*>(field_ptr)              \
              ->~RepeatedField<LOWERCASE>();                                  \
          break

        HANDLE_TYPE( INT32,  int32);
        HANDLE_TYPE( INT64,  int64);
        HANDLE_TYPE(UINT32, uint32);
        HANDLE_TYPE(UINT64, uint64);
        HANDLE_TYPE(DOUBLE, double);
        HANDLE_TYPE( FLOAT,  float);
        HANDLE_TYPE(  BOOL,   bool);
        HANDLE_TYPE(  ENUM,    int);
#undef HANDLE_TYPE

        case FieldDescriptor::CPPTYPE_STRING:
          switch (field->options().ctype()) {
            default:
            case FieldOptions::STRING:
              reinterpret_cast<RepeatedPtrField<string>*>(field_ptr)
                  ->~RepeatedPtrField<string>();
              break;
          }
          break;

        case FieldDescriptor::CPPTYPE_MESSAGE:
          reinterpret_cast<RepeatedPtrField<Message>*>(field_ptr)
              ->~RepeatedPtrField<Message>();
          break;
      }

    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
      switch (field->options().ctype()) {
        default:
        case FieldOptions::STRING: {
          string* ptr = *reinterpret_cast<string**>(field_ptr);
          if (ptr != &field->default_value_string()) {
            delete ptr;
          }
          break;
        }
      }
    } else if ((field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) &&
               !is_prototype()) {
      Message* message = *reinterpret_cast<Message**>(field_ptr);
      if (message != NULL) {
        delete message;
      }
    }
  }
}

void DynamicMessage::CrossLinkPrototypes() {

  GOOGLE_CHECK(is_prototype());

  DynamicMessageFactory* factory = type_info_->factory;
  const Descriptor* descriptor = type_info_->type;

  for (int i = 0; i < descriptor->field_count(); i++) {
    const FieldDescriptor* field = descriptor->field(i);
    void* field_ptr = OffsetToPointer(type_info_->offsets[i]);

    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
        !field->is_repeated()) {

      *reinterpret_cast<const Message**>(field_ptr) =
        factory->GetPrototypeNoLock(field->message_type());
    }
  }
}

Message* DynamicMessage::New() const {
  void* new_base = reinterpret_cast<uint8*>(operator new(type_info_->size));
  memset(new_base, 0, type_info_->size);
  return new(new_base) DynamicMessage(type_info_);
}

int DynamicMessage::GetCachedSize() const {
  return cached_byte_size_;
}

void DynamicMessage::SetCachedSize(int size) const {

  cached_byte_size_ = size;
}

Metadata DynamicMessage::GetMetadata() const {
  Metadata metadata;
  metadata.descriptor = type_info_->type;
  metadata.reflection = type_info_->reflection.get();
  return metadata;
}

struct DynamicMessageFactory::PrototypeMap {
  typedef hash_map<const Descriptor*, const DynamicMessage::TypeInfo*> Map;
  Map map_;
};

DynamicMessageFactory::DynamicMessageFactory()
  : pool_(NULL), delegate_to_generated_factory_(false),
    prototypes_(new PrototypeMap) {
}

DynamicMessageFactory::DynamicMessageFactory(const DescriptorPool* pool)
  : pool_(pool), delegate_to_generated_factory_(false),
    prototypes_(new PrototypeMap) {
}

DynamicMessageFactory::~DynamicMessageFactory() {
  for (PrototypeMap::Map::iterator iter = prototypes_->map_.begin();
       iter != prototypes_->map_.end(); ++iter) {
    delete iter->second;
  }
}

const Message* DynamicMessageFactory::GetPrototype(const Descriptor* type) {
  MutexLock lock(&prototypes_mutex_);
  return GetPrototypeNoLock(type);
}

const Message* DynamicMessageFactory::GetPrototypeNoLock(
    const Descriptor* type) {
  if (delegate_to_generated_factory_ &&
      type->file()->pool() == DescriptorPool::generated_pool()) {
    return MessageFactory::generated_factory()->GetPrototype(type);
  }

  const DynamicMessage::TypeInfo** target = &prototypes_->map_[type];
  if (*target != NULL) {

    return (*target)->prototype.get();
  }

  DynamicMessage::TypeInfo* type_info = new DynamicMessage::TypeInfo;
  *target = type_info;

  type_info->type = type;
  type_info->pool = (pool_ == NULL) ? type->file()->pool() : pool_;
  type_info->factory = this;

  int* offsets = new int[type->field_count()];
  type_info->offsets.reset(offsets);

  int size = sizeof(DynamicMessage);
  size = AlignOffset(size);

  type_info->has_bits_offset = size;
  int has_bits_array_size =
    DivideRoundingUp(type->field_count(), bitsizeof(uint32));
  size += has_bits_array_size * sizeof(uint32);
  size = AlignOffset(size);

  if (type->extension_range_count() > 0) {
    type_info->extensions_offset = size;
    size += sizeof(ExtensionSet);
    size = AlignOffset(size);
  } else {

    type_info->extensions_offset = -1;
  }

  for (int i = 0; i < type->field_count(); i++) {

    int field_size = FieldSpaceUsed(type->field(i));
    size = AlignTo(size, min(kSafeAlignment, field_size));
    offsets[i] = size;
    size += field_size;
  }

  size = AlignOffset(size);
  type_info->unknown_fields_offset = size;
  size += sizeof(UnknownFieldSet);

  size = AlignOffset(size);
  type_info->size = size;

  void* base = operator new(size);
  memset(base, 0, size);
  DynamicMessage* prototype = new(base) DynamicMessage(type_info);
  type_info->prototype.reset(prototype);

  type_info->reflection.reset(
    new GeneratedMessageReflection(
      type_info->type,
      type_info->prototype.get(),
      type_info->offsets.get(),
      type_info->has_bits_offset,
      type_info->unknown_fields_offset,
      type_info->extensions_offset,
      type_info->pool,
      this,
      type_info->size));

  prototype->CrossLinkPrototypes();

  return prototype;
}

}
}
