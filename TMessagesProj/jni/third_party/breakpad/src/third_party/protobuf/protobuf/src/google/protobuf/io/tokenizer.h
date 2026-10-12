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
// Class for parsing tokenized text from a ZeroCopyInputStream.

#ifndef GOOGLE_PROTOBUF_IO_TOKENIZER_H__
#define GOOGLE_PROTOBUF_IO_TOKENIZER_H__

#include <string>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

class ZeroCopyInputStream;

class ErrorCollector;
class Tokenizer;

class LIBPROTOBUF_EXPORT ErrorCollector {
 public:
  inline ErrorCollector() {}
  virtual ~ErrorCollector();

  virtual void AddError(int line, int column, const string& message) = 0;

  virtual void AddWarning(int line, int column, const string& message) { }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ErrorCollector);
};

class LIBPROTOBUF_EXPORT Tokenizer {
 public:

  Tokenizer(ZeroCopyInputStream* input, ErrorCollector* error_collector);
  ~Tokenizer();

  enum TokenType {
    TYPE_START,
    TYPE_END,

    TYPE_IDENTIFIER,

    TYPE_INTEGER,

    TYPE_FLOAT,

    TYPE_STRING,

    TYPE_SYMBOL,

  };

  struct Token {
    TokenType type;
    string text;

    int line;
    int column;
    int end_column;
  };

  const Token& current();

  const Token& previous();

  bool Next();

  static double ParseFloat(const string& text);

  static void ParseString(const string& text, string* output);

  static void ParseStringAppend(const string& text, string* output);

  static bool ParseInteger(const string& text, uint64 max_value,
                           uint64* output);

  void set_allow_f_after_float(bool value) { allow_f_after_float_ = value; }

  enum CommentStyle {

    CPP_COMMENT_STYLE,

    SH_COMMENT_STYLE
  };

  void set_comment_style(CommentStyle style) { comment_style_ = style; }

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Tokenizer);

  Token current_;
  Token previous_;

  ZeroCopyInputStream* input_;
  ErrorCollector* error_collector_;

  char current_char_;
  const char* buffer_;
  int buffer_size_;
  int buffer_pos_;
  bool read_error_;

  int line_;
  int column_;

  int token_start_;

  bool allow_f_after_float_;
  CommentStyle comment_style_;

  static const int kTabWidth = 8;

  void NextChar();

  void Refresh();

  inline void StartToken();

  inline void EndToken();

  void AddError(const string& message) {
    error_collector_->AddError(line_, column_, message);
  }

  void ConsumeString(char delimiter);

  TokenType ConsumeNumber(bool started_with_zero, bool started_with_dot);

  void ConsumeLineComment();

  void ConsumeBlockComment();

  template<typename CharacterClass>
  inline bool LookingAt();

  template<typename CharacterClass>
  inline bool TryConsumeOne();

  inline bool TryConsume(char c);

  template<typename CharacterClass>
  inline void ConsumeZeroOrMore();

  template<typename CharacterClass>
  inline void ConsumeOneOrMore(const char* error);
};

inline const Tokenizer::Token& Tokenizer::current() {
  return current_;
}

inline const Tokenizer::Token& Tokenizer::previous() {
  return previous_;
}

inline void Tokenizer::ParseString(const string& text, string* output) {
  output->clear();
  ParseStringAppend(text, output);
}

}
}

}
#endif
