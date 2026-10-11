#!/usr/bin/env python
#
# Copyright 2009, Google Inc.
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
import re
import sys
from googletest.test import gtest_test_utils

FREEBSD = ('FreeBSD', 'GNU/kFreeBSD')
NETBSD = ('NetBSD',)
OPENBSD = ('OpenBSD',)

def is_bsd_based_os() -> bool:

  if os.name != 'posix':
    return False

  return os.uname()[0] in (FREEBSD + NETBSD + OPENBSD)

IS_DARWIN = os.name == 'posix' and os.uname()[0] == 'Darwin'
IS_LINUX = os.name == 'posix' and os.uname()[0] == 'Linux'
IS_GNUHURD = os.name == 'posix' and os.uname()[0] == 'GNU'
IS_WINDOWS = os.name == 'nt'

PROGRAM_PATH = gtest_test_utils.GetTestExecutablePath('gtest_help_test_')
FLAG_PREFIX = '--gtest_'
DEATH_TEST_STYLE_FLAG = FLAG_PREFIX + 'death_test_style'
STREAM_RESULT_TO_FLAG = FLAG_PREFIX + 'stream_result_to'
LIST_TESTS_FLAG = FLAG_PREFIX + 'list_tests'
INTERNAL_FLAG_FOR_TESTING = FLAG_PREFIX + 'internal_flag_for_testing'

SUPPORTS_DEATH_TESTS = (
    'DeathTest'
    in gtest_test_utils.Subprocess([PROGRAM_PATH, LIST_TESTS_FLAG]).output
)

HAS_ABSL_FLAGS = '--has_absl_flags' in sys.argv

HELP_REGEX = re.compile(
    FLAG_PREFIX
    + r'list_tests.*'
    + FLAG_PREFIX
    + r'filter=.*'
    + FLAG_PREFIX
    + r'also_run_disabled_tests.*'
    + FLAG_PREFIX
    + r'repeat=.*'
    + FLAG_PREFIX
    + r'shuffle.*'
    + FLAG_PREFIX
    + r'random_seed=.*'
    + FLAG_PREFIX
    + r'color=.*'
    + FLAG_PREFIX
    + r'brief.*'
    + FLAG_PREFIX
    + r'print_time.*'
    + FLAG_PREFIX
    + r'output=.*'
    + FLAG_PREFIX
    + r'break_on_failure.*'
    + FLAG_PREFIX
    + r'throw_on_failure.*'
    + FLAG_PREFIX
    + r'catch_exceptions=0.*',
    re.DOTALL,
)

def run_with_flag(flag):

  if flag is None:
    command = [PROGRAM_PATH]
  else:
    command = [PROGRAM_PATH, flag]
  child = gtest_test_utils.Subprocess(command)
  return child.exit_code, child.output

class GTestHelpTest(gtest_test_utils.TestCase):

  def test_prints_help_with_full_flag(self):

    exit_code, output = run_with_flag('--help')
    if HAS_ABSL_FLAGS:

      self.assertEqual(1, exit_code)
    else:
      self.assertEqual(0, exit_code)

    self.assertTrue(HELP_REGEX.search(output), output)

    if IS_DARWIN or IS_LINUX or IS_GNUHURD or is_bsd_based_os():
      self.assertIn(STREAM_RESULT_TO_FLAG, output)
    else:
      self.assertNotIn(STREAM_RESULT_TO_FLAG, output)

    if SUPPORTS_DEATH_TESTS and not IS_WINDOWS:
      self.assertIn(DEATH_TEST_STYLE_FLAG, output)
    else:
      self.assertNotIn(DEATH_TEST_STYLE_FLAG, output)

  def test_runs_tests_without_help_flag(self):

    exit_code, output = run_with_flag(None)
    self.assertNotEqual(exit_code, 0)
    self.assertFalse(HELP_REGEX.search(output), output)

  def test_runs_tests_with_gtest_internal_flag(self):

    exit_code, output = run_with_flag(INTERNAL_FLAG_FOR_TESTING)
    self.assertNotEqual(exit_code, 0)
    self.assertFalse(HELP_REGEX.search(output), output)

if __name__ == '__main__':
  if '--has_absl_flags' in sys.argv:
    sys.argv.remove('--has_absl_flags')
  gtest_test_utils.Main()
