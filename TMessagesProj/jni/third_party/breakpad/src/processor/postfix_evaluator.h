// -*- mode: C++ -*-

// Copyright (c) 2010 Google Inc.
// All rights reserved.
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

// postfix_evaluator.h: Postfix (reverse Polish) notation expression evaluator.
//
// PostfixEvaluator evaluates an expression, using the expression itself
// in postfix (reverse Polish) notation and a dictionary mapping constants
// and variables to their values.  The evaluator supports standard
// arithmetic operations, assignment into variables, and when an optional
// MemoryRange is provided, dereferencing.  (Any unary key-to-value operation
// may be used with a MemoryRange implementation that returns the appropriate
// values, but PostfixEvaluator was written with dereferencing in mind.)
//
// The expression language is simple.  Expressions are supplied as strings,
// with operands and operators delimited by whitespace.  Operands may be
// either literal values suitable for ValueType, or constants or variables,
// which reference the dictionary.  The supported binary operators are +
// (addition), - (subtraction), * (multiplication), / (quotient of division),
// % (modulus of division), and @ (data alignment). The alignment operator (@)
// accepts a value and an alignment size, and produces a result that is a
// multiple of the alignment size by truncating the input value.
// The unary ^ (dereference) operator is also provided.  These operators
// allow any operand to be either a literal value, constant, or variable.
// Assignment (=) of any type of operand into a variable is also supported.
//
// The dictionary is provided as a map with string keys.  Keys beginning
// with the '$' character are treated as variables.  All other keys are
// treated as constants.  Any results must be assigned into variables in the
// dictionary.  These variables do not need to exist prior to calling
// Evaluate, unless used in an expression prior to being assigned to.  The
// internal stack state is not made available after evaluation, and any
// values remaining on the stack are treated as evidence of incomplete
// execution and cause the evaluator to indicate failure.
//
// PostfixEvaluator is intended to support evaluation of "program strings"
// obtained from MSVC frame data debugging information in pdb files as
// returned by the DIA APIs.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_POSTFIX_EVALUATOR_H__
#define PROCESSOR_POSTFIX_EVALUATOR_H__

#include <map>
#include <string>
#include <vector>

#include "common/using_std_string.h"

namespace google_breakpad {

using std::map;
using std::vector;

class MemoryRegion;

template<typename ValueType>
class PostfixEvaluator {
 public:
  typedef map<string, ValueType> DictionaryType;
  typedef map<string, bool> DictionaryValidityType;

  PostfixEvaluator(DictionaryType *dictionary, const MemoryRegion *memory)
      : dictionary_(dictionary), memory_(memory), stack_() {}

  bool Evaluate(const string &expression, DictionaryValidityType *assigned);

  bool EvaluateForValue(const string &expression, ValueType *result);

  DictionaryType* dictionary() const { return dictionary_; }

  void set_dictionary(DictionaryType *dictionary) {dictionary_ = dictionary; }

 private:

  enum PopResult {
    POP_RESULT_FAIL = 0,
    POP_RESULT_VALUE,
    POP_RESULT_IDENTIFIER
  };

  PopResult PopValueOrIdentifier(ValueType *value, string *identifier);

  bool PopValue(ValueType *value);

  bool PopValues(ValueType *value1, ValueType *value2);

  void PushValue(const ValueType &value);

  bool EvaluateInternal(const string &expression,
                        DictionaryValidityType *assigned);

  bool EvaluateToken(const string &token,
                     const string &expression,
                     DictionaryValidityType *assigned);

  DictionaryType *dictionary_;

  const MemoryRegion *memory_;

  vector<string> stack_;
};

}

#endif
