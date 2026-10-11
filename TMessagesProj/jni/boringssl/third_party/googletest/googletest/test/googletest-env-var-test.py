#!/usr/bin/env python
#
# Copyright 2008, Google Inc.
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

import os
from googletest.test import gtest_test_utils

IS_WINDOWS = os.name == 'nt'
IS_LINUX = os.name == 'posix' and os.uname()[0] == 'Linux'

COMMAND = gtest_test_utils.GetTestExecutablePath('googletest-env-var-test_')

environ = os.environ.copy()

def AssertEq(expected, actual):
  if expected != actual:
    print('Expected: %s' % (expected,))
    print('  Actual: %s' % (actual,))
    raise AssertionError

def SetEnvVar(env_var, value):

  if value is not None:
    environ[env_var] = value
  elif env_var in environ:
    del environ[env_var]

def GetFlag(flag):

  args = [COMMAND]
  if flag is not None:
    args += [flag]
  return gtest_test_utils.Subprocess(args, env=environ).output

def TestFlag(flag, test_val, default_val):

  env_var = 'GTEST_' + flag.upper()
  SetEnvVar(env_var, test_val)
  AssertEq(test_val, GetFlag(flag))
  SetEnvVar(env_var, None)
  AssertEq(default_val, GetFlag(flag))

class GTestEnvVarTest(gtest_test_utils.TestCase):

  def testEnvVarAffectsFlag(self):

    TestFlag('break_on_failure', '1', '0')
    TestFlag('color', 'yes', 'auto')
    SetEnvVar('TESTBRIDGE_TEST_RUNNER_FAIL_FAST', None)
    TestFlag('fail_fast', '1', '0')
    TestFlag('filter', 'FooTest.Bar', '*')
    SetEnvVar('XML_OUTPUT_FILE', None)
    TestFlag('output', 'xml:tmp/foo.xml', '')
    TestFlag('brief', '1', '0')
    TestFlag('print_time', '0', '1')
    TestFlag('repeat', '999', '1')
    TestFlag('throw_on_failure', '1', '0')
    TestFlag('death_test_style', 'threadsafe', 'fast')
    TestFlag('catch_exceptions', '0', '1')

    if IS_LINUX:
      TestFlag('death_test_use_fork', '1', '0')
      TestFlag('stack_trace_depth', '0', '100')

  def testXmlOutputFile(self):

    SetEnvVar('GTEST_OUTPUT', None)
    SetEnvVar('XML_OUTPUT_FILE', 'tmp/bar.xml')
    AssertEq('xml:tmp/bar.xml', GetFlag('output'))

  def testXmlOutputFileOverride(self):

    SetEnvVar('GTEST_OUTPUT', 'xml:tmp/foo.xml')
    SetEnvVar('XML_OUTPUT_FILE', 'tmp/bar.xml')
    AssertEq('xml:tmp/foo.xml', GetFlag('output'))

if __name__ == '__main__':
  gtest_test_utils.Main()
