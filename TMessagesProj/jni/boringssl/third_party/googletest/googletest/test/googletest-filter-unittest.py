#!/usr/bin/env python
#
# Copyright 2005 Google Inc. All Rights Reserved.
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

try:
  from sets import Set as set
except ImportError:
  pass
import sys
from googletest.test import gtest_test_utils

CAN_PASS_EMPTY_ENV = False
if sys.executable:
  os.environ['EMPTY_VAR'] = ''
  child = gtest_test_utils.Subprocess(
      [sys.executable, '-c', "import os; print('EMPTY_VAR' in os.environ)"]
  )
  CAN_PASS_EMPTY_ENV = eval(child.output)

CAN_UNSET_ENV = False
if sys.executable:
  os.environ['UNSET_VAR'] = 'X'
  del os.environ['UNSET_VAR']
  child = gtest_test_utils.Subprocess(
      [sys.executable, '-c', "import os; print('UNSET_VAR' not in os.environ)"]
  )
  CAN_UNSET_ENV = eval(child.output)

CAN_TEST_EMPTY_FILTER = CAN_PASS_EMPTY_ENV and CAN_UNSET_ENV

FILTER_ENV_VAR = 'GTEST_FILTER'

TOTAL_SHARDS_ENV_VAR = 'GTEST_TOTAL_SHARDS'
SHARD_INDEX_ENV_VAR = 'GTEST_SHARD_INDEX'
SHARD_STATUS_FILE_ENV_VAR = 'GTEST_SHARD_STATUS_FILE'

FILTER_FLAG = 'gtest_filter'

ALSO_RUN_DISABLED_TESTS_FLAG = 'gtest_also_run_disabled_tests'

COMMAND = gtest_test_utils.GetTestExecutablePath('googletest-filter-unittest_')

PARAM_TEST_REGEX = re.compile(r'/ParamTest')

TEST_CASE_REGEX = re.compile(r'^\[\-+\] \d+ tests? from (\w+(/\w+)?)')

TEST_REGEX = re.compile(r'^\[\s*RUN\s*\].*\.(\w+(/\w+)?)')

DISABLED_BANNER_REGEX = re.compile(r'^\[\s*DISABLED\s*\] (.*)')

LIST_TESTS_FLAG = '--gtest_list_tests'

SUPPORTS_DEATH_TESTS = (
    'HasDeathTest'
    in gtest_test_utils.Subprocess([COMMAND, LIST_TESTS_FLAG]).output
)

PARAM_TESTS = [
    'SeqP/ParamTest.TestX/0',
    'SeqP/ParamTest.TestX/1',
    'SeqP/ParamTest.TestY/0',
    'SeqP/ParamTest.TestY/1',
    'SeqQ/ParamTest.TestX/0',
    'SeqQ/ParamTest.TestX/1',
    'SeqQ/ParamTest.TestY/0',
    'SeqQ/ParamTest.TestY/1',
]

DISABLED_TESTS = [
    'BarTest.DISABLED_TestFour',
    'BarTest.DISABLED_TestFive',
    'BazTest.DISABLED_TestC',
    'DISABLED_FoobarTest.Test1',
    'DISABLED_FoobarTest.DISABLED_Test2',
    'DISABLED_FoobarbazTest.TestA',
]

if SUPPORTS_DEATH_TESTS:
  DEATH_TESTS = [
      'HasDeathTest.Test1',
      'HasDeathTest.Test2',
  ]
else:
  DEATH_TESTS = []

ACTIVE_TESTS = (
    [
        'FooTest.Abc',
        'FooTest.Xyz',
        'BarTest.TestOne',
        'BarTest.TestTwo',
        'BarTest.TestThree',
        'BazTest.TestOne',
        'BazTest.TestA',
        'BazTest.TestB',
    ]
    + DEATH_TESTS
    + PARAM_TESTS
)

param_tests_present = None

environ = os.environ.copy()

def SetEnvVar(env_var, value):

  if value is not None:
    environ[env_var] = value
  elif env_var in environ:
    del environ[env_var]

def RunAndReturnOutput(args=None):

  return gtest_test_utils.Subprocess(
      [COMMAND] + (args or []), env=environ
  ).output

def RunAndExtractTestList(args=None):

  p = gtest_test_utils.Subprocess([COMMAND] + (args or []), env=environ)
  tests_run = []
  test_case = ''
  test = ''
  for line in p.output.split('\n'):
    match = TEST_CASE_REGEX.match(line)
    if match is not None:
      test_case = match.group(1)
    else:
      match = TEST_REGEX.match(line)
      if match is not None:
        test = match.group(1)
        tests_run.append(test_case + '.' + test)
  return (tests_run, p.exit_code)

def RunAndExtractDisabledBannerList(args=None):

  p = gtest_test_utils.Subprocess([COMMAND] + (args or []), env=environ)
  banners_printed = []
  for line in p.output.split('\n'):
    match = DISABLED_BANNER_REGEX.match(line)
    if match is not None:
      banners_printed.append(match.group(1))
  return banners_printed

def InvokeWithModifiedEnv(extra_env, function, *args, **kwargs):

  try:
    original_env = environ.copy()
    environ.update(extra_env)
    return function(*args, **kwargs)
  finally:
    environ.clear()
    environ.update(original_env)

def RunWithSharding(total_shards, shard_index, command):

  extra_env = {
      SHARD_INDEX_ENV_VAR: str(shard_index),
      TOTAL_SHARDS_ENV_VAR: str(total_shards),
  }
  return InvokeWithModifiedEnv(extra_env, RunAndExtractTestList, command)

class GTestFilterUnitTest(gtest_test_utils.TestCase):

  def AssertSetEqual(self, lhs, rhs):

    for elem in lhs:
      self.assertTrue(elem in rhs, '%s in %s' % (elem, rhs))

    for elem in rhs:
      self.assertTrue(elem in lhs, '%s in %s' % (elem, lhs))

  def AssertPartitionIsValid(self, set_var, list_of_sets):

    full_partition = []
    for slice_var in list_of_sets:
      full_partition.extend(slice_var)
    self.assertEqual(len(set_var), len(full_partition))
    self.assertEqual(set(set_var), set(full_partition))

  def AdjustForParameterizedTests(self, tests_to_run):

    global param_tests_present
    if not param_tests_present:
      return list(set(tests_to_run) - set(PARAM_TESTS))
    else:
      return tests_to_run

  def RunAndVerify(self, gtest_filter, tests_to_run):

    tests_to_run = self.AdjustForParameterizedTests(tests_to_run)

    if CAN_TEST_EMPTY_FILTER or gtest_filter != '':
      SetEnvVar(FILTER_ENV_VAR, gtest_filter)
      tests_run = RunAndExtractTestList()[0]
      SetEnvVar(FILTER_ENV_VAR, None)
      self.AssertSetEqual(tests_run, tests_to_run)

    if gtest_filter is None:
      args = []
    else:
      args = ['--%s=%s' % (FILTER_FLAG, gtest_filter)]

    tests_run = RunAndExtractTestList(args)[0]
    self.AssertSetEqual(tests_run, tests_to_run)

  def RunAndVerifyWithSharding(
      self,
      gtest_filter,
      total_shards,
      tests_to_run,
      args=None,
      check_exit_0=False,
  ):

    tests_to_run = self.AdjustForParameterizedTests(tests_to_run)

    if CAN_TEST_EMPTY_FILTER or gtest_filter != '':
      SetEnvVar(FILTER_ENV_VAR, gtest_filter)
      partition = []
      for i in range(0, total_shards):
        (tests_run, exit_code) = RunWithSharding(total_shards, i, args)
        if check_exit_0:
          self.assertEqual(0, exit_code)
        partition.append(tests_run)

      self.AssertPartitionIsValid(tests_to_run, partition)
      SetEnvVar(FILTER_ENV_VAR, None)

  def RunAndVerifyAllowingDisabled(self, gtest_filter, tests_to_run):

    tests_to_run = self.AdjustForParameterizedTests(tests_to_run)

    args = ['--%s' % ALSO_RUN_DISABLED_TESTS_FLAG]
    if gtest_filter is not None:
      args.append('--%s=%s' % (FILTER_FLAG, gtest_filter))

    tests_run = RunAndExtractTestList(args)[0]
    self.AssertSetEqual(tests_run, tests_to_run)

  def setUp(self):

    global param_tests_present
    if param_tests_present is None:
      param_tests_present = (
          PARAM_TEST_REGEX.search(RunAndReturnOutput()) is not None
      )

  def testDefaultBehavior(self):

    self.RunAndVerify(None, ACTIVE_TESTS)

  def testDefaultBehaviorWithShards(self):

    self.RunAndVerifyWithSharding(None, 1, ACTIVE_TESTS)
    self.RunAndVerifyWithSharding(None, 2, ACTIVE_TESTS)
    self.RunAndVerifyWithSharding(None, len(ACTIVE_TESTS) - 1, ACTIVE_TESTS)
    self.RunAndVerifyWithSharding(None, len(ACTIVE_TESTS), ACTIVE_TESTS)
    self.RunAndVerifyWithSharding(None, len(ACTIVE_TESTS) + 1, ACTIVE_TESTS)

  def testEmptyFilter(self):

    self.RunAndVerify('', [])
    self.RunAndVerifyWithSharding('', 1, [])
    self.RunAndVerifyWithSharding('', 2, [])

  def testBadFilter(self):

    self.RunAndVerify('BadFilter', [])
    self.RunAndVerifyAllowingDisabled('BadFilter', [])

  def testFullName(self):

    self.RunAndVerify('FooTest.Xyz', ['FooTest.Xyz'])
    self.RunAndVerifyAllowingDisabled('FooTest.Xyz', ['FooTest.Xyz'])
    self.RunAndVerifyWithSharding('FooTest.Xyz', 5, ['FooTest.Xyz'])

  def testUniversalFilters(self):

    self.RunAndVerify('*', ACTIVE_TESTS)
    self.RunAndVerify('*.*', ACTIVE_TESTS)
    self.RunAndVerifyWithSharding('*.*', len(ACTIVE_TESTS) - 3, ACTIVE_TESTS)
    self.RunAndVerifyAllowingDisabled('*', ACTIVE_TESTS + DISABLED_TESTS)
    self.RunAndVerifyAllowingDisabled('*.*', ACTIVE_TESTS + DISABLED_TESTS)

  def testFilterByTestCase(self):

    self.RunAndVerify('FooTest.*', ['FooTest.Abc', 'FooTest.Xyz'])

    BAZ_TESTS = ['BazTest.TestOne', 'BazTest.TestA', 'BazTest.TestB']
    self.RunAndVerify('BazTest.*', BAZ_TESTS)
    self.RunAndVerifyAllowingDisabled(
        'BazTest.*', BAZ_TESTS + ['BazTest.DISABLED_TestC']
    )

  def testFilterByTest(self):

    self.RunAndVerify('*.TestOne', ['BarTest.TestOne', 'BazTest.TestOne'])

  def testFilterDisabledTests(self):

    self.RunAndVerify('DISABLED_FoobarTest.Test1', [])
    self.RunAndVerifyAllowingDisabled(
        'DISABLED_FoobarTest.Test1', ['DISABLED_FoobarTest.Test1']
    )

    self.RunAndVerify('*DISABLED_*', [])
    self.RunAndVerifyAllowingDisabled('*DISABLED_*', DISABLED_TESTS)

    self.RunAndVerify('*.DISABLED_*', [])
    self.RunAndVerifyAllowingDisabled(
        '*.DISABLED_*',
        [
            'BarTest.DISABLED_TestFour',
            'BarTest.DISABLED_TestFive',
            'BazTest.DISABLED_TestC',
            'DISABLED_FoobarTest.DISABLED_Test2',
        ],
    )

    self.RunAndVerify('DISABLED_*', [])
    self.RunAndVerifyAllowingDisabled(
        'DISABLED_*',
        [
            'DISABLED_FoobarTest.Test1',
            'DISABLED_FoobarTest.DISABLED_Test2',
            'DISABLED_FoobarbazTest.TestA',
        ],
    )

  def testWildcardInTestCaseName(self):

    self.RunAndVerify(
        '*a*.*',
        [
            'BarTest.TestOne',
            'BarTest.TestTwo',
            'BarTest.TestThree',
            'BazTest.TestOne',
            'BazTest.TestA',
            'BazTest.TestB',
        ]
        + DEATH_TESTS
        + PARAM_TESTS,
    )

  def testWildcardInTestName(self):

    self.RunAndVerify('*.*A*', ['FooTest.Abc', 'BazTest.TestA'])

  def testFilterWithoutDot(self):

    self.RunAndVerify(
        '*z*',
        [
            'FooTest.Xyz',
            'BazTest.TestOne',
            'BazTest.TestA',
            'BazTest.TestB',
        ],
    )

  def testTwoPatterns(self):

    self.RunAndVerify(
        'Foo*.*:*A*',
        [
            'FooTest.Abc',
            'FooTest.Xyz',
            'BazTest.TestA',
        ],
    )

    self.RunAndVerify(':*A*', ['FooTest.Abc', 'BazTest.TestA'])

  def testThreePatterns(self):

    self.RunAndVerify(
        '*oo*:*A*:*One',
        [
            'FooTest.Abc',
            'FooTest.Xyz',
            'BarTest.TestOne',
            'BazTest.TestOne',
            'BazTest.TestA',
        ],
    )

    self.RunAndVerify(
        '*oo*::*One',
        [
            'FooTest.Abc',
            'FooTest.Xyz',
            'BarTest.TestOne',
            'BazTest.TestOne',
        ],
    )

    self.RunAndVerify(
        '*oo*::',
        [
            'FooTest.Abc',
            'FooTest.Xyz',
        ],
    )

  def testNegativeFilters(self):
    self.RunAndVerify(
        '*-BazTest.TestOne',
        [
            'FooTest.Abc',
            'FooTest.Xyz',
            'BarTest.TestOne',
            'BarTest.TestTwo',
            'BarTest.TestThree',
            'BazTest.TestA',
            'BazTest.TestB',
        ]
        + DEATH_TESTS
        + PARAM_TESTS,
    )

    self.RunAndVerify(
        '*-FooTest.Abc:BazTest.*',
        [
            'FooTest.Xyz',
            'BarTest.TestOne',
            'BarTest.TestTwo',
            'BarTest.TestThree',
        ]
        + DEATH_TESTS
        + PARAM_TESTS,
    )

    self.RunAndVerify(
        'BarTest.*-BarTest.TestOne',
        [
            'BarTest.TestTwo',
            'BarTest.TestThree',
        ],
    )

    self.RunAndVerify(
        '-FooTest.Abc:FooTest.Xyz:BazTest.*',
        [
            'BarTest.TestOne',
            'BarTest.TestTwo',
            'BarTest.TestThree',
        ]
        + DEATH_TESTS
        + PARAM_TESTS,
    )

    self.RunAndVerify('*/*', PARAM_TESTS)

    self.RunAndVerify(
        'SeqP/*',
        [
            'SeqP/ParamTest.TestX/0',
            'SeqP/ParamTest.TestX/1',
            'SeqP/ParamTest.TestY/0',
            'SeqP/ParamTest.TestY/1',
        ],
    )

    self.RunAndVerify(
        '*/0',
        [
            'SeqP/ParamTest.TestX/0',
            'SeqP/ParamTest.TestY/0',
            'SeqQ/ParamTest.TestX/0',
            'SeqQ/ParamTest.TestY/0',
        ],
    )

  def testFlagOverridesEnvVar(self):

    SetEnvVar(FILTER_ENV_VAR, 'Foo*')
    args = ['--%s=%s' % (FILTER_FLAG, '*One')]
    tests_run = RunAndExtractTestList(args)[0]
    SetEnvVar(FILTER_ENV_VAR, None)

    self.AssertSetEqual(tests_run, ['BarTest.TestOne', 'BazTest.TestOne'])

  def testShardStatusFileIsCreated(self):

    shard_status_file = os.path.join(
        gtest_test_utils.GetTempDir(), 'shard_status_file'
    )
    self.assertTrue(not os.path.exists(shard_status_file))

    extra_env = {SHARD_STATUS_FILE_ENV_VAR: shard_status_file}
    try:
      InvokeWithModifiedEnv(extra_env, RunAndReturnOutput)
    finally:
      self.assertTrue(os.path.exists(shard_status_file))
      os.remove(shard_status_file)

  def testShardStatusFileIsCreatedWithListTests(self):

    shard_status_file = os.path.join(
        gtest_test_utils.GetTempDir(), 'shard_status_file2'
    )
    self.assertTrue(not os.path.exists(shard_status_file))

    extra_env = {SHARD_STATUS_FILE_ENV_VAR: shard_status_file}
    try:
      output = InvokeWithModifiedEnv(
          extra_env, RunAndReturnOutput, [LIST_TESTS_FLAG]
      )
    finally:

      self.assertTrue(
          '[==========]' not in output,
          (
              'Unexpected output during test enumeration.\n'
              'Please ensure that LIST_TESTS_FLAG is assigned the\n'
              'correct flag value for listing Google Test tests.'
          ),
      )

      self.assertTrue(os.path.exists(shard_status_file))
      os.remove(shard_status_file)

  def testDisabledBanner(self):

    make_filter = lambda s: ['--%s=%s' % (FILTER_FLAG, s)]

    banners = RunAndExtractDisabledBannerList(make_filter('*'))
    self.AssertSetEqual(
        banners,
        [
            'BarTest.DISABLED_TestFour',
            'BarTest.DISABLED_TestFive',
            'BazTest.DISABLED_TestC',
        ],
    )

    banners = RunAndExtractDisabledBannerList(make_filter('Bar*'))
    self.AssertSetEqual(
        banners, ['BarTest.DISABLED_TestFour', 'BarTest.DISABLED_TestFive']
    )

    banners = RunAndExtractDisabledBannerList(make_filter('*-Bar*'))
    self.AssertSetEqual(banners, ['BazTest.DISABLED_TestC'])

  if SUPPORTS_DEATH_TESTS:

    def testShardingWorksWithDeathTests(self):

      gtest_filter = 'HasDeathTest.*:SeqP/*'
      expected_tests = [
          'HasDeathTest.Test1',
          'HasDeathTest.Test2',
          'SeqP/ParamTest.TestX/0',
          'SeqP/ParamTest.TestX/1',
          'SeqP/ParamTest.TestY/0',
          'SeqP/ParamTest.TestY/1',
      ]

      for flag in [
          '--gtest_death_test_style=threadsafe',
          '--gtest_death_test_style=fast',
      ]:
        self.RunAndVerifyWithSharding(
            gtest_filter, 3, expected_tests, check_exit_0=True, args=[flag]
        )
        self.RunAndVerifyWithSharding(
            gtest_filter, 5, expected_tests, check_exit_0=True, args=[flag]
        )

if __name__ == '__main__':
  gtest_test_utils.Main()
