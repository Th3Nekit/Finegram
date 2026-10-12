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

// Author: jschorr@google.com (Joseph Schorr)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// Utilities for printing and parsing protocol messages in a human-readable,
// text-based format.

#ifndef GOOGLE_PROTOBUF_TEXT_FORMAT_H__
#define GOOGLE_PROTOBUF_TEXT_FORMAT_H__

#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {

namespace io {
  class ErrorCollector;
}

class LIBPROTOBUF_EXPORT TextFormat {
 public:

  static bool Print(const Message& message, io::ZeroCopyOutputStream* output);

  static bool PrintUnknownFields(const UnknownFieldSet& unknown_fields,
                                 io::ZeroCopyOutputStream* output);

  static bool PrintToString(const Message& message, string* output);

  static bool PrintUnknownFieldsToString(const UnknownFieldSet& unknown_fields,
                                         string* output);

  static void PrintFieldValueToString(const Message& message,
                                      const FieldDescriptor* field,
                                      int index,
                                      string* output);

  class LIBPROTOBUF_EXPORT Printer {
   public:
    Printer();
    ~Printer();

    bool Print(const Message& message, io::ZeroCopyOutputStream* output) const;

    bool PrintUnknownFields(const UnknownFieldSet& unknown_fields,
                            io::ZeroCopyOutputStream* output) const;

    bool PrintToString(const Message& message, string* output) const;

    bool PrintUnknownFieldsToString(const UnknownFieldSet& unknown_fields,
                                    string* output) const;

    void PrintFieldValueToString(const Message& message,
                                 const FieldDescriptor* field,
                                 int index,
                                 string* output) const;

    void SetInitialIndentLevel(int indent_level) {
      initial_indent_level_ = indent_level;
    }

    void SetSingleLineMode(bool single_line_mode) {
      single_line_mode_ = single_line_mode;
    }

    void SetUseShortRepeatedPrimitives(bool use_short_repeated_primitives) {
      use_short_repeated_primitives_ = use_short_repeated_primitives;
    }

    void SetUseUtf8StringEscaping(bool as_utf8) {
      utf8_string_escaping_ = as_utf8;
    }

   private:

    class TextGenerator;

    void Print(const Message& message,
               TextGenerator& generator) const;

    void PrintField(const Message& message,
                    const Reflection* reflection,
                    const FieldDescriptor* field,
                    TextGenerator& generator) const;

    void PrintShortRepeatedField(const Message& message,
                                 const Reflection* reflection,
                                 const FieldDescriptor* field,
                                 TextGenerator& generator) const;

    void PrintFieldName(const Message& message,
                        const Reflection* reflection,
                        const FieldDescriptor* field,
                        TextGenerator& generator) const;

    void PrintFieldValue(const Message& message,
                         const Reflection* reflection,
                         const FieldDescriptor* field,
                         int index,
                         TextGenerator& generator) const;

    void PrintUnknownFields(const UnknownFieldSet& unknown_fields,
                            TextGenerator& generator) const;

    int initial_indent_level_;

    bool single_line_mode_;

    bool use_short_repeated_primitives_;

    bool utf8_string_escaping_;
  };

  static bool Parse(io::ZeroCopyInputStream* input, Message* output);

  static bool ParseFromString(const string& input, Message* output);

  static bool Merge(io::ZeroCopyInputStream* input, Message* output);

  static bool MergeFromString(const string& input, Message* output);

  static bool ParseFieldValueFromString(const string& input,
                                        const FieldDescriptor* field,
                                        Message* message);

  class LIBPROTOBUF_EXPORT Finder {
   public:
    virtual ~Finder();

    virtual const FieldDescriptor* FindExtension(
        Message* message,
        const string& name) const = 0;
  };

  class LIBPROTOBUF_EXPORT Parser {
   public:
    Parser();
    ~Parser();

    bool Parse(io::ZeroCopyInputStream* input, Message* output);

    bool ParseFromString(const string& input, Message* output);

    bool Merge(io::ZeroCopyInputStream* input, Message* output);

    bool MergeFromString(const string& input, Message* output);

    void RecordErrorsTo(io::ErrorCollector* error_collector) {
      error_collector_ = error_collector;
    }

    void SetFinder(Finder* finder) {
      finder_ = finder;
    }

    void AllowPartialMessage(bool allow) {
      allow_partial_ = allow;
    }

    bool ParseFieldValueFromString(const string& input,
                                   const FieldDescriptor* field,
                                   Message* output);

   private:

    class ParserImpl;

    bool MergeUsingImpl(io::ZeroCopyInputStream* input,
                        Message* output,
                        ParserImpl* parser_impl);

    io::ErrorCollector* error_collector_;
    Finder* finder_;
    bool allow_partial_;
  };

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(TextFormat);
};

}

}
#endif
