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

#ifndef GOOGLE_PROTOBUF_COMPILER_CPP_MESSAGE_H__
#define GOOGLE_PROTOBUF_COMPILER_CPP_MESSAGE_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>

namespace google {
namespace protobuf {
  namespace io {
    class Printer;
  }
}

namespace protobuf {
namespace compiler {
namespace cpp {

class EnumGenerator;
class ExtensionGenerator;

class MessageGenerator {
 public:

  explicit MessageGenerator(const Descriptor* descriptor,
                            const string& dllexport_decl);
  ~MessageGenerator();

  void GenerateForwardDeclaration(io::Printer* printer);

  void GenerateEnumDefinitions(io::Printer* printer);

  void GenerateGetEnumDescriptorSpecializations(io::Printer* printer);

  void GenerateClassDefinition(io::Printer* printer);

  void GenerateInlineMethods(io::Printer* printer);

  void GenerateDescriptorDeclarations(io::Printer* printer);

  void GenerateDescriptorInitializer(io::Printer* printer, int index);

  void GenerateTypeRegistrations(io::Printer* printer);

  void GenerateDefaultInstanceAllocator(io::Printer* printer);

  void GenerateDefaultInstanceInitializer(io::Printer* printer);

  void GenerateShutdownCode(io::Printer* printer);

  void GenerateClassMethods(io::Printer* printer);

 private:

  void GenerateFieldAccessorDeclarations(io::Printer* printer);
  void GenerateFieldAccessorDefinitions(io::Printer* printer);

  void GenerateOffsets(io::Printer* printer);

  void GenerateStructors(io::Printer* printer);

  void GenerateSharedConstructorCode(io::Printer* printer);

  void GenerateSharedDestructorCode(io::Printer* printer);

  void GenerateClear(io::Printer* printer);
  void GenerateMergeFromCodedStream(io::Printer* printer);
  void GenerateSerializeWithCachedSizes(io::Printer* printer);
  void GenerateSerializeWithCachedSizesToArray(io::Printer* printer);
  void GenerateSerializeWithCachedSizesBody(io::Printer* printer,
                                            bool to_array);
  void GenerateByteSize(io::Printer* printer);
  void GenerateMergeFrom(io::Printer* printer);
  void GenerateCopyFrom(io::Printer* printer);
  void GenerateSwap(io::Printer* printer);
  void GenerateIsInitialized(io::Printer* printer);

  void GenerateSerializeOneField(io::Printer* printer,
                                 const FieldDescriptor* field,
                                 bool unbounded);
  void GenerateSerializeOneExtensionRange(
      io::Printer* printer, const Descriptor::ExtensionRange* range,
      bool unbounded);

  const Descriptor* descriptor_;
  string classname_;
  string dllexport_decl_;
  FieldGeneratorMap field_generators_;
  scoped_array<scoped_ptr<MessageGenerator> > nested_generators_;
  scoped_array<scoped_ptr<EnumGenerator> > enum_generators_;
  scoped_array<scoped_ptr<ExtensionGenerator> > extension_generators_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MessageGenerator);
};

}
}
}

}
#endif
