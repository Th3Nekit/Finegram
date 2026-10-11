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

// postfix_evaluator_unittest.cc: Unit tests for PostfixEvaluator.
//
// Author: Mark Mentovai

#include <assert.h>
#include <stdio.h>

#include <map>
#include <string>

#include "processor/postfix_evaluator-inl.h"

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/memory_region.h"
#include "processor/logging.h"

namespace {

using std::map;
using google_breakpad::MemoryRegion;
using google_breakpad::PostfixEvaluator;

class FakeMemoryRegion : public MemoryRegion {
 public:
  virtual uint64_t GetBase() const { return 0; }
  virtual uint32_t GetSize() const { return 0; }
  virtual bool GetMemoryAtAddress(uint64_t address, uint8_t  *value) const {
    *value = address + 1;
    return true;
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint16_t *value) const {
    *value = address + 1;
    return true;
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint32_t *value) const {
    *value = address + 1;
    return true;
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint64_t *value) const {
    *value = address + 1;
    return true;
  }
  virtual void Print() const {
    assert(false);
  }
};

struct EvaluateTest {

  const string expression;

  bool evaluable;
};

struct EvaluateTestSet {

  PostfixEvaluator<unsigned int>::DictionaryType *dictionary;

  const EvaluateTest *evaluate_tests;

  unsigned int evaluate_test_count;

  map<string, unsigned int> *validate_data;
};

struct EvaluateForValueTest {

  const string expression;

  bool evaluable;

  unsigned int value;
};

static bool RunTests() {

  PostfixEvaluator<unsigned int>::DictionaryType dictionary_0;
  const EvaluateTest evaluate_tests_0[] = {
    { "$rAdd 2 2 + =",     true },
    { "$rAdd $rAdd 2 + =", true },
    { "$rAdd 2 $rAdd + =", true },
    { "99",                false },
    { "$rAdd2 2 2 + =",    true },
    { "$rAdd2\t2\n2 + =",  true },
    { "$rAdd2 2 2 + = ",   true },
    { " $rAdd2 2 2 + =",   true },
    { "$rAdd2  2 2 +   =", true },
    { "$T0 2 = +",         false },
    { "2 + =",             false },
    { "2 +",               false },
    { "+",                 false },
    { "^",                 false },
    { "=",                 false },
    { "2 =",               false },
    { "2 2 + =",           false },
    { "2 2 =",             false },
    { "k 2 =",             false },
    { "2",                 false },
    { "2 2 +",             false },
    { "$rAdd",             false },
    { "0 $T1 0 0 + =",     false },
    { "$T2 $T2 2 + =",     false },
    { "$rMul 9 6 * =",     true },
    { "$rSub 9 6 - =",     true },
    { "$rDivQ 9 6 / =",    true },
    { "$rDivM 9 6 % =",    true },
    { "$rDeref 9 ^ =",     true },
    { "$rAlign 36 8 @ =",  true },
    { "$rAdd3 2 2 + =$rMul2 9 6 * =", true }
  };
  map<string, unsigned int> validate_data_0;
  validate_data_0["$rAdd"]   = 8;
  validate_data_0["$rAdd2"]  = 4;
  validate_data_0["$rSub"]   = 3;
  validate_data_0["$rMul"]   = 54;
  validate_data_0["$rDivQ"]  = 1;
  validate_data_0["$rDivM"]  = 3;
  validate_data_0["$rDeref"] = 10;
  validate_data_0["$rAlign"] = 32;
  validate_data_0["$rAdd3"]  = 4;
  validate_data_0["$rMul2"]  = 54;

  PostfixEvaluator<unsigned int>::DictionaryType dictionary_1;
  dictionary_1["$ebp"] = 0xbfff0010;
  dictionary_1["$eip"] = 0x10000000;
  dictionary_1["$esp"] = 0xbfff0000;
  dictionary_1[".cbSavedRegs"] = 4;
  dictionary_1[".cbParams"] = 4;
  dictionary_1[".raSearchStart"] = 0xbfff0020;
  const EvaluateTest evaluate_tests_1[] = {
    { "$T0 $ebp = $eip $T0 4 + ^ = $ebp $T0 ^ = $esp $T0 8 + = "
      "$L $T0 .cbSavedRegs - = $P $T0 8 + .cbParams + =", true },

    { "$T0 $ebp = $eip $T0 4 + ^ = $ebp $T0 ^ = $esp $T0 8 + = "
      "$L $T0 .cbSavedRegs - = $P $T0 8 + .cbParams + = $ebx $T0 28 - ^ =",
      true },

    { "$T0 $ebp = $T2 $esp = $T1 .raSearchStart = $eip $T1 ^ = $ebp $T0 = "
      "$esp $T1 4 + = $L $T0 .cbSavedRegs - = $P $T1 4 + .cbParams + = "
      "$ebx $T0 28 - ^ =",
      true }
  };
  map<string, unsigned int> validate_data_1;
  validate_data_1["$T0"]  = 0xbfff0012;
  validate_data_1["$T1"]  = 0xbfff0020;
  validate_data_1["$T2"]  = 0xbfff0019;
  validate_data_1["$eip"] = 0xbfff0021;
  validate_data_1["$ebp"] = 0xbfff0012;
  validate_data_1["$esp"] = 0xbfff0024;
  validate_data_1["$L"]   = 0xbfff000e;
  validate_data_1["$P"]   = 0xbfff0028;
  validate_data_1["$ebx"] = 0xbffefff7;
  validate_data_1[".cbSavedRegs"] = 4;
  validate_data_1[".cbParams"] = 4;

  EvaluateTestSet evaluate_test_sets[] = {
    { &dictionary_0, evaluate_tests_0,
          sizeof(evaluate_tests_0) / sizeof(EvaluateTest), &validate_data_0 },
    { &dictionary_1, evaluate_tests_1,
          sizeof(evaluate_tests_1) / sizeof(EvaluateTest), &validate_data_1 },
  };

  unsigned int evaluate_test_set_count = sizeof(evaluate_test_sets) /
                                         sizeof(EvaluateTestSet);

  FakeMemoryRegion fake_memory;
  PostfixEvaluator<unsigned int> postfix_evaluator =
      PostfixEvaluator<unsigned int>(NULL, &fake_memory);

  for (unsigned int evaluate_test_set_index = 0;
       evaluate_test_set_index < evaluate_test_set_count;
       ++evaluate_test_set_index) {
    EvaluateTestSet *evaluate_test_set =
        &evaluate_test_sets[evaluate_test_set_index];
    const EvaluateTest *evaluate_tests = evaluate_test_set->evaluate_tests;
    unsigned int evaluate_test_count = evaluate_test_set->evaluate_test_count;

    postfix_evaluator.set_dictionary(evaluate_test_set->dictionary);

    PostfixEvaluator<unsigned int>::DictionaryValidityType assigned;

    for (unsigned int evaluate_test_index = 0;
         evaluate_test_index < evaluate_test_count;
         ++evaluate_test_index) {
      const EvaluateTest *evaluate_test = &evaluate_tests[evaluate_test_index];

      bool result = postfix_evaluator.Evaluate(evaluate_test->expression,
                                               &assigned);
      if (result != evaluate_test->evaluable) {
        fprintf(stderr, "FAIL: evaluate set %d/%d, test %d/%d, "
                        "expression \"%s\", expected %s, observed %s\n",
                evaluate_test_set_index, evaluate_test_set_count,
                evaluate_test_index, evaluate_test_count,
                evaluate_test->expression.c_str(),
                evaluate_test->evaluable ? "evaluable" : "not evaluable",
                result ? "evaluted" : "not evaluated");
        return false;
      }
    }

    for (map<string, unsigned int>::const_iterator validate_iterator =
            evaluate_test_set->validate_data->begin();
        validate_iterator != evaluate_test_set->validate_data->end();
        ++validate_iterator) {
      const string identifier = validate_iterator->first;
      unsigned int expected_value = validate_iterator->second;

      map<string, unsigned int>::const_iterator dictionary_iterator =
          evaluate_test_set->dictionary->find(identifier);

      if (dictionary_iterator == evaluate_test_set->dictionary->end()) {
        fprintf(stderr, "FAIL: evaluate test set %d/%d, "
                        "validate identifier \"%s\", "
                        "expected %d, observed not found\n",
                evaluate_test_set_index, evaluate_test_set_count,
                identifier.c_str(), expected_value);
        return false;
      }

      unsigned int observed_value = dictionary_iterator->second;
      if (expected_value != observed_value) {
        fprintf(stderr, "FAIL: evaluate test set %d/%d, "
                        "validate identifier \"%s\", "
                        "expected %d, observed %d\n",
                evaluate_test_set_index, evaluate_test_set_count,
                identifier.c_str(), expected_value, observed_value);
        return false;
      }

      bool expected_assigned = identifier[0] == '$';
      bool observed_assigned = false;
      PostfixEvaluator<unsigned int>::DictionaryValidityType::const_iterator
          iterator_assigned = assigned.find(identifier);
      if (iterator_assigned != assigned.end()) {
        observed_assigned = iterator_assigned->second;
      }
      if (expected_assigned != observed_assigned) {
        fprintf(stderr, "FAIL: evaluate test set %d/%d, "
                        "validate assignment of \"%s\", "
                        "expected %d, observed %d\n",
                evaluate_test_set_index, evaluate_test_set_count,
                identifier.c_str(), expected_assigned, observed_assigned);
        return false;
      }
    }
  }

  PostfixEvaluator<unsigned int>::DictionaryType dictionary_2;
  dictionary_2["$ebp"] = 0xbfff0010;
  dictionary_2["$eip"] = 0x10000000;
  dictionary_2["$esp"] = 0xbfff0000;
  dictionary_2[".cbSavedRegs"] = 4;
  dictionary_2[".cbParams"] = 4;
  dictionary_2[".raSearchStart"] = 0xbfff0020;
  const EvaluateForValueTest evaluate_for_value_tests_2[] = {
    { "28907223",               true,  28907223 },
    { "89854293 40010015 +",    true,  89854293 + 40010015 },
    { "-870245 8769343 +",      true,  7899098 },
    { "$ebp $esp - $eip +",     true,  0x10000010 },
    { "18929794 34015074",      false, 0 },
    { "$ebp $ebp 4 - =",        false, 0 },
    { "$new $eip = $new",       true,  0x10000000 },
    { "$new 4 +",               true,  0x10000004 },
    { ".cfa 42 = 10",           false, 0 }
  };
  const int evaluate_for_value_tests_2_size
      = (sizeof (evaluate_for_value_tests_2)
         / sizeof (evaluate_for_value_tests_2[0]));
  map<string, unsigned int> validate_data_2;
  validate_data_2["$eip"] = 0x10000000;
  validate_data_2["$ebp"] = 0xbfff000c;
  validate_data_2["$esp"] = 0xbfff0000;
  validate_data_2["$new"] = 0x10000000;
  validate_data_2[".cbSavedRegs"] = 4;
  validate_data_2[".cbParams"] = 4;
  validate_data_2[".raSearchStart"] = 0xbfff0020;

  postfix_evaluator.set_dictionary(&dictionary_2);
  for (int i = 0; i < evaluate_for_value_tests_2_size; i++) {
    const EvaluateForValueTest *test = &evaluate_for_value_tests_2[i];
    unsigned int result;
    if (postfix_evaluator.EvaluateForValue(test->expression, &result)
        != test->evaluable) {
      fprintf(stderr, "FAIL: evaluate for value test %d, "
              "expected evaluation to %s, but it %s\n",
              i, test->evaluable ? "succeed" : "fail",
              test->evaluable ? "failed" : "succeeded");
      return false;
    }
    if (test->evaluable && result != test->value) {
      fprintf(stderr, "FAIL: evaluate for value test %d, "
              "expected value to be 0x%x, but it was 0x%x\n",
              i, test->value, result);
      return false;
    }
  }

  for (map<string, unsigned int>::iterator v = validate_data_2.begin();
       v != validate_data_2.end(); v++) {
    map<string, unsigned int>::iterator a = dictionary_2.find(v->first);
    if (a == dictionary_2.end()) {
      fprintf(stderr, "FAIL: evaluate for value dictionary check: "
              "expected dict[\"%s\"] to be 0x%x, but it was unset\n",
              v->first.c_str(), v->second);
      return false;
    } else if (a->second != v->second) {
      fprintf(stderr, "FAIL: evaluate for value dictionary check: "
              "expected dict[\"%s\"] to be 0x%x, but it was 0x%x\n",
              v->first.c_str(), v->second, a->second);
      return false;
    }
    dictionary_2.erase(a);
  }

  map<string, unsigned int>::iterator remaining = dictionary_2.begin();
  if (remaining != dictionary_2.end()) {
    fprintf(stderr, "FAIL: evaluation of test expressions put unexpected "
            "values in dictionary:\n");
    for (; remaining != dictionary_2.end(); remaining++)
      fprintf(stderr, "    dict[\"%s\"] == 0x%x\n",
              remaining->first.c_str(), remaining->second);
    return false;
  }

  return true;
}

}

int main(int argc, char **argv) {
  BPLOG_INIT(&argc, &argv);

  return RunTests() ? 0 : 1;
}
