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

#ifndef GOOGLE_PROTOBUF_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_TEST_UTIL_H__

#include <stack>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/unittest.pb.h>

namespace google {
namespace protobuf {

namespace unittest = protobuf_unittest;
namespace unittest_import = protobuf_unittest_import;

class TestUtil {
 public:

  static void SetAllFields(unittest::TestAllTypes* message);
  static void SetAllExtensions(unittest::TestAllExtensions* message);
  static void SetAllFieldsAndExtensions(unittest::TestFieldOrderings* message);
  static void SetPackedFields(unittest::TestPackedTypes* message);
  static void SetPackedExtensions(unittest::TestPackedExtensions* message);
  static void SetUnpackedFields(unittest::TestUnpackedTypes* message);

  static void ModifyRepeatedFields(unittest::TestAllTypes* message);
  static void ModifyRepeatedExtensions(unittest::TestAllExtensions* message);
  static void ModifyPackedFields(unittest::TestPackedTypes* message);
  static void ModifyPackedExtensions(unittest::TestPackedExtensions* message);

  static void ExpectAllFieldsSet(const unittest::TestAllTypes& message);
  static void ExpectAllExtensionsSet(
      const unittest::TestAllExtensions& message);
  static void ExpectPackedFieldsSet(const unittest::TestPackedTypes& message);
  static void ExpectPackedExtensionsSet(
      const unittest::TestPackedExtensions& message);
  static void ExpectUnpackedFieldsSet(
      const unittest::TestUnpackedTypes& message);

  static void ExpectRepeatedFieldsModified(
      const unittest::TestAllTypes& message);
  static void ExpectRepeatedExtensionsModified(
      const unittest::TestAllExtensions& message);
  static void ExpectPackedFieldsModified(
      const unittest::TestPackedTypes& message);
  static void ExpectPackedExtensionsModified(
      const unittest::TestPackedExtensions& message);

  static void ExpectClear(const unittest::TestAllTypes& message);
  static void ExpectExtensionsClear(const unittest::TestAllExtensions& message);
  static void ExpectPackedClear(const unittest::TestPackedTypes& message);
  static void ExpectPackedExtensionsClear(
      const unittest::TestPackedExtensions& message);

  static void ExpectAllFieldsAndExtensionsInOrder(const string& serialized);

  static void ExpectLastRepeatedsRemoved(
      const unittest::TestAllTypes& message);
  static void ExpectLastRepeatedExtensionsRemoved(
      const unittest::TestAllExtensions& message);

  static void ExpectRepeatedsSwapped(const unittest::TestAllTypes& message);
  static void ExpectRepeatedExtensionsSwapped(
      const unittest::TestAllExtensions& message);

  class ReflectionTester {
   public:

    explicit ReflectionTester(const Descriptor* base_descriptor);

    void SetAllFieldsViaReflection(Message* message);
    void ModifyRepeatedFieldsViaReflection(Message* message);
    void ExpectAllFieldsSetViaReflection(const Message& message);
    void ExpectClearViaReflection(const Message& message);

    void SetPackedFieldsViaReflection(Message* message);
    void ModifyPackedFieldsViaReflection(Message* message);
    void ExpectPackedFieldsSetViaReflection(const Message& message);
    void ExpectPackedClearViaReflection(const Message& message);

    void RemoveLastRepeatedsViaReflection(Message* message);
    void SwapRepeatedsViaReflection(Message* message);

   private:
    const FieldDescriptor* F(const string& name);

    const Descriptor* base_descriptor_;

    const FieldDescriptor* group_a_;
    const FieldDescriptor* repeated_group_a_;
    const FieldDescriptor* nested_b_;
    const FieldDescriptor* foreign_c_;
    const FieldDescriptor* import_d_;

    const EnumValueDescriptor* nested_foo_;
    const EnumValueDescriptor* nested_bar_;
    const EnumValueDescriptor* nested_baz_;
    const EnumValueDescriptor* foreign_foo_;
    const EnumValueDescriptor* foreign_bar_;
    const EnumValueDescriptor* foreign_baz_;
    const EnumValueDescriptor* import_foo_;
    const EnumValueDescriptor* import_bar_;
    const EnumValueDescriptor* import_baz_;

    void ExpectAllFieldsSetViaReflection1(const Message& message);
    void ExpectAllFieldsSetViaReflection2(const Message& message);
    void ExpectAllFieldsSetViaReflection3(const Message& message);

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ReflectionTester);
  };

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TestUtil);
};

}

}
#endif
