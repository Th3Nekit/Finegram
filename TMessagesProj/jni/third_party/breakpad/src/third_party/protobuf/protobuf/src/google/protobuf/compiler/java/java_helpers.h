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

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_HELPERS_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_HELPERS_H__

#include <string>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

extern const char kThickSeparator[];
extern const char kThinSeparator[];

string UnderscoresToCamelCase(const FieldDescriptor* field);
string UnderscoresToCapitalizedCamelCase(const FieldDescriptor* field);

string UnderscoresToCamelCase(const MethodDescriptor* method);

string StripProto(const string& filename);

string FileClassName(const FileDescriptor* file);

string FileJavaPackage(const FileDescriptor* file);

string JavaPackageToDir(string package_name);

string ToJavaName(const string& full_name, const FileDescriptor* file);

inline string ClassName(const Descriptor* descriptor) {
  return ToJavaName(descriptor->full_name(), descriptor->file());
}
inline string ClassName(const EnumDescriptor* descriptor) {
  return ToJavaName(descriptor->full_name(), descriptor->file());
}
inline string ClassName(const ServiceDescriptor* descriptor) {
  return ToJavaName(descriptor->full_name(), descriptor->file());
}
inline string ExtensionIdentifierName(const FieldDescriptor* descriptor) {
  return ToJavaName(descriptor->full_name(), descriptor->file());
}
string ClassName(const FileDescriptor* descriptor);

string FieldConstantName(const FieldDescriptor *field);

FieldDescriptor::Type GetType(const FieldDescriptor* field);

enum JavaType {
  JAVATYPE_INT,
  JAVATYPE_LONG,
  JAVATYPE_FLOAT,
  JAVATYPE_DOUBLE,
  JAVATYPE_BOOLEAN,
  JAVATYPE_STRING,
  JAVATYPE_BYTES,
  JAVATYPE_ENUM,
  JAVATYPE_MESSAGE
};

JavaType GetJavaType(const FieldDescriptor* field);

const char* BoxedPrimitiveTypeName(JavaType type);

string DefaultValue(const FieldDescriptor* field);
bool IsDefaultValueJavaDefault(const FieldDescriptor* field);

inline bool HasUnknownFields(const Descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           FileOptions::LITE_RUNTIME;
}

inline bool HasGeneratedMethods(const Descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           FileOptions::CODE_SIZE;
}

inline bool HasEqualsAndHashCode(const Descriptor* descriptor) {
  return descriptor->file()->options().java_generate_equals_and_hash();
}

inline bool HasDescriptorMethods(const Descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           FileOptions::LITE_RUNTIME;
}
inline bool HasDescriptorMethods(const EnumDescriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           FileOptions::LITE_RUNTIME;
}
inline bool HasDescriptorMethods(const FileDescriptor* descriptor) {
  return descriptor->options().optimize_for() !=
           FileOptions::LITE_RUNTIME;
}

inline bool HasNestedBuilders(const Descriptor* descriptor) {

  return descriptor->file()->options().optimize_for() !=
           FileOptions::LITE_RUNTIME;
}

inline bool HasGenericServices(const FileDescriptor *file) {
  return file->service_count() > 0 &&
         file->options().optimize_for() != FileOptions::LITE_RUNTIME &&
         file->options().java_generic_services();
}

string GetBitFieldName(int index);

string GetBitFieldNameForBit(int bitIndex);

string GenerateGetBit(int bitIndex);

string GenerateSetBit(int bitIndex);

string GenerateClearBit(int bitIndex);

string GenerateGetBitFromLocal(int bitIndex);

string GenerateSetBitToLocal(int bitIndex);

}
}
}

}
#endif
