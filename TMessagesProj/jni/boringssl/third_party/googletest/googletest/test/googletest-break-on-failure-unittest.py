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

import os
from googletest.test import gtest_test_utils

IS_WINDOWS = os.name == 'nt'

BREAK_ON_FAILURE_ENV_VAR = 'GTEST_BREAK_ON_FAILURE'

BREAK_ON_FAILURE_FLAG = 'gtest_break_on_failure'

THROW_ON_FAILURE_ENV_VAR = 'GTEST_THROW_ON_FAILURE'

CATCH_EXCEPTIONS_ENV_VAR = 'GTEST_CATCH_EXCEPTIONS'

EXE_PATH = gtest_test_utils.GetTestExecutablePath(
    'googletest-break-on-failure-unittest_'
)

environ = gtest_test_utils.environ
SetEnvVar = gtest_test_utils.SetEnvVar

SetEnvVar(gtest_test_utils.PREMATURE_EXIT_FILE_ENV_VAR, None)

def Run(command):

  p = gtest_test_utils.Subprocess(command, env=environ)
  if p.terminated_by_signal:
    return 1
  else:
    return 0

class GTestBreakOnFailureUnitTest(gtest_test_utils.TestCase):

  def RunAndVerify(self, env_var_value, flag_value, expect_seg_fault):

    SetEnvVar(BREAK_ON_FAILURE_ENV_VAR, env_var_value)

    if env_var_value is None:
      env_var_value_msg = ' is not set'
    else:
      env_var_value_msg = '=' + env_var_value

    if flag_value is None:
      flag = ''
    elif flag_value == '0':
      flag = '--%s=0' % BREAK_ON_FAILURE_FLAG
    else:
      flag = '--%s' % BREAK_ON_FAILURE_FLAG

    command = [EXE_PATH]
    if flag:
      command.append(flag)

    if expect_seg_fault:
      should_or_not = 'should'
    else:
      should_or_not = 'should not'

    has_seg_fault = Run(command)

    SetEnvVar(BREAK_ON_FAILURE_ENV_VAR, None)

    msg = 'when %s%s, an assertion failure in "%s" %s cause a seg-fault.' % (
        BREAK_ON_FAILURE_ENV_VAR,
        env_var_value_msg,
        ' '.join(command),
        should_or_not,
    )
    self.assertTrue(has_seg_fault == expect_seg_fault, msg)

  def testDefaultBehavior(self):

    self.RunAndVerify(env_var_value=None, flag_value=None, expect_seg_fault=0)

  def testEnvVar(self):

    self.RunAndVerify(env_var_value='0', flag_value=None, expect_seg_fault=0)
    self.RunAndVerify(env_var_value='1', flag_value=None, expect_seg_fault=1)

  def testFlag(self):

    self.RunAndVerify(env_var_value=None, flag_value='0', expect_seg_fault=0)
    self.RunAndVerify(env_var_value=None, flag_value='1', expect_seg_fault=1)

  def testFlagOverridesEnvVar(self):

    self.RunAndVerify(env_var_value='0', flag_value='0', expect_seg_fault=0)
    self.RunAndVerify(env_var_value='0', flag_value='1', expect_seg_fault=1)
    self.RunAndVerify(env_var_value='1', flag_value='0', expect_seg_fault=0)
    self.RunAndVerify(env_var_value='1', flag_value='1', expect_seg_fault=1)

  def testBreakOnFailureOverridesThrowOnFailure(self):

    SetEnvVar(THROW_ON_FAILURE_ENV_VAR, '1')
    try:
      self.RunAndVerify(env_var_value=None, flag_value='1', expect_seg_fault=1)
    finally:
      SetEnvVar(THROW_ON_FAILURE_ENV_VAR, None)

  if IS_WINDOWS:

    def testCatchExceptionsDoesNotInterfere(self):

      SetEnvVar(CATCH_EXCEPTIONS_ENV_VAR, '1')
      try:
        self.RunAndVerify(env_var_value='1', flag_value='1', expect_seg_fault=1)
      finally:
        SetEnvVar(CATCH_EXCEPTIONS_ENV_VAR, None)

if __name__ == '__main__':
  gtest_test_utils.Main()
