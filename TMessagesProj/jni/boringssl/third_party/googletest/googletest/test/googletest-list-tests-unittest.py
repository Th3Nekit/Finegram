#!/usr/bin/env python
#
# Copyright 2006, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
from googletest.test import gtest_test_utils

LIST_TESTS_FLAG = 'gtest_list_tests'

EXE_PATH = gtest_test_utils.GetTestExecutablePath(
    'googletest-list-tests-unittest_'
)

EXPECTED_OUTPUT_NO_FILTER_RE = re.compile(
    r"""FooDeathTest\.
  Test1
Foo\.
  Bar1
  Bar2
  DISABLED_Bar3
Abc\.
  Xyz
  Def
FooBar\.
  Baz
FooTest\.
  Test1
  DISABLED_Test2
  Test3
TypedTest/0\.  # TypeParam = (VeryLo{245}|class VeryLo{239})\.\.\.
  TestA
  TestB
TypedTest/1\.  # TypeParam = int\s*\*( __ptr64)?
  TestA
  TestB
TypedTest/2\.  # TypeParam = .*MyArray<bool,\s*42>
  TestA
  TestB
My/TypeParamTest/0\.  # TypeParam = (VeryLo{245}|class VeryLo{239})\.\.\.
  TestA
  TestB
My/TypeParamTest/1\.  # TypeParam = int\s*\*( __ptr64)?
  TestA
  TestB
My/TypeParamTest/2\.  # TypeParam = .*MyArray<bool,\s*42>
  TestA
  TestB
MyInstantiation/ValueParamTest\.
  TestA/0  # GetParam\(\) = one line
  TestA/1  # GetParam\(\) = two\\nlines
  TestA/2  # GetParam\(\) = a very\\nlo{241}\.\.\.
  TestB/0  # GetParam\(\) = one line
  TestB/1  # GetParam\(\) = two\\nlines
  TestB/2  # GetParam\(\) = a very\\nlo{241}\.\.\.
"""
)

EXPECTED_OUTPUT_FILTER_FOO_RE = re.compile(
    r"""FooDeathTest\.
  Test1
Foo\.
  Bar1
  Bar2
  DISABLED_Bar3
FooBar\.
  Baz
FooTest\.
  Test1
  DISABLED_Test2
  Test3
"""
)

def Run(args):

  return gtest_test_utils.Subprocess(
      [EXE_PATH] + args, capture_stderr=False
  ).output

class GTestListTestsUnitTest(gtest_test_utils.TestCase):

  def RunAndVerify(self, flag_value, expected_output_re, other_flag):

    if flag_value is None:
      flag = ''
      flag_expression = 'not set'
    elif flag_value == '0':
      flag = '--%s=0' % LIST_TESTS_FLAG
      flag_expression = '0'
    else:
      flag = '--%s' % LIST_TESTS_FLAG
      flag_expression = '1'

    args = [flag]

    if other_flag is not None:
      args += [other_flag]

    output = Run(args)

    if expected_output_re:
      self.assertTrue(
          expected_output_re.match(output),
          'when %s is %s, the output of "%s" is "%s",\n'
          'which does not match regex "%s"'
          % (
              LIST_TESTS_FLAG,
              flag_expression,
              ' '.join(args),
              output,
              expected_output_re.pattern,
          ),
      )
    else:
      self.assertTrue(
          not EXPECTED_OUTPUT_NO_FILTER_RE.match(output),
          'when %s is %s, the output of "%s" is "%s"'
          % (LIST_TESTS_FLAG, flag_expression, ' '.join(args), output),
      )

  def testDefaultBehavior(self):

    self.RunAndVerify(flag_value=None, expected_output_re=None, other_flag=None)

  def testFlag(self):

    self.RunAndVerify(flag_value='0', expected_output_re=None, other_flag=None)
    self.RunAndVerify(
        flag_value='1',
        expected_output_re=EXPECTED_OUTPUT_NO_FILTER_RE,
        other_flag=None,
    )

  def testOverrideNonFilterFlags(self):

    self.RunAndVerify(
        flag_value='1',
        expected_output_re=EXPECTED_OUTPUT_NO_FILTER_RE,
        other_flag='--gtest_break_on_failure',
    )

  def testWithFilterFlags(self):

    self.RunAndVerify(
        flag_value='1',
        expected_output_re=EXPECTED_OUTPUT_FILTER_FOO_RE,
        other_flag='--gtest_filter=Foo*',
    )

if __name__ == '__main__':
  gtest_test_utils.Main()
