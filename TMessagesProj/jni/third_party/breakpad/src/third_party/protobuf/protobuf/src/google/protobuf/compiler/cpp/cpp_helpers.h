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

#ifndef GOOGLE_PROTOBUF_COMPILER_CPP_HELPERS_H__
#define GOOGLE_PROTOBUF_COMPILER_CPP_HELPERS_H__

#include <string>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

extern const char kThickSeparator[];
extern const char kThinSeparator[];

string ClassName(const Descriptor* descriptor, bool qualified);
string ClassName(const EnumDescriptor* enum_descriptor, bool qualified);

string SuperClassName(const Descriptor* descriptor);

string FieldName(const FieldDescriptor* field);

string FieldConstantName(const FieldDescriptor *field);

inline const Descriptor* FieldScope(const FieldDescriptor* field) {
  return field->is_extension() ?
    field->extension_scope() : field->containing_type();
}

string FieldMessageTypeName(const FieldDescriptor* field);

string StripProto(const string& filename);

const char* PrimitiveTypeName(FieldDescriptor::CppType type);

const char* DeclaredTypeMethodName(FieldDescriptor::Type type);

string DefaultValue(const FieldDescriptor* field);

string FilenameIdentifier(const string& filename);

string GlobalAddDescriptorsName(const string& filename);

string GlobalAssignDescriptorsName(const string& filename);

string GlobalShutdownFileName(const string& filename);

string EscapeTrigraphs(const string& to_escape);

inline bool HasUnknownFields(const FileDescriptor *file) {
  return file->options().optimize_for() != FileOptions::LITE_RUNTIME;
}

inline bool HasGeneratedMethods(const FileDescriptor *file) {
  return file->options().optimize_for() != FileOptions::CODE_SIZE;
}

inline bool HasDescriptorMethods(const FileDescriptor *file) {
  return file->options().optimize_for() != FileOptions::LITE_RUNTIME;
}

inline bool HasGenericServices(const FileDescriptor *file) {
  return file->service_count() > 0 &&
         file->options().optimize_for() != FileOptions::LITE_RUNTIME &&
         file->options().cc_generic_services();
}

inline bool HasUtf8Verification(const FileDescriptor* file) {
  return file->options().optimize_for() != FileOptions::LITE_RUNTIME;
}

inline bool HasFastArraySerialization(const FileDescriptor* file) {
  return file->options().optimize_for() == FileOptions::SPEED;
}

}
}
}

}
#endif
