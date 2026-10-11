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

from io import open
import os
import re
import sys
from googlemock.test import gmock_test_utils

GENGOLDEN_FLAG = '--gengolden'

PROGRAM_PATH = gmock_test_utils.GetTestExecutablePath('gmock_output_test_')
COMMAND = [PROGRAM_PATH, '--gtest_stack_trace_depth=0', '--gtest_print_time=0']
GOLDEN_NAME = 'gmock_output_test_golden.txt'
GOLDEN_PATH = os.path.join(gmock_test_utils.GetSourceDir(), GOLDEN_NAME)

def ToUnixLineEnding(s):

  return s.replace('\r\n', '\n').replace('\r', '\n')

def RemoveReportHeaderAndFooter(output):

  output = re.sub(r'.*gtest_main.*\n', '', output)
  output = re.sub(r'\[.*\d+ tests.*\n', '', output)
  output = re.sub(r'\[.* test environment .*\n', '', output)
  output = re.sub(r'\[=+\] \d+ tests .* ran.*', '', output)
  output = re.sub(r'.* FAILED TESTS\n', '', output)
  return output

def RemoveLocations(output):

  return re.sub(r'.*[/\\](.+)(\:\d+|\(\d+\))\:', 'FILE:#:', output)

def NormalizeErrorMarker(output):

  return re.sub(r' error: ', ' Failure\n', output)

def RemoveMemoryAddresses(output):

  return re.sub(r'@\w+', '@0x#', output)

def RemoveTestNamesOfLeakedMocks(output):

  return re.sub(r'\(used in test .+\) ', '', output)

def GetLeakyTests(output):

  return re.findall(r'\(used in test (.+)\)', output)

def GetNormalizedOutputAndLeakyTests(output):

  output = ToUnixLineEnding(output)
  output = RemoveReportHeaderAndFooter(output)
  output = NormalizeErrorMarker(output)
  output = RemoveLocations(output)
  output = RemoveMemoryAddresses(output)
  return (RemoveTestNamesOfLeakedMocks(output), GetLeakyTests(output))

def GetShellCommandOutput(cmd):

  return gmock_test_utils.Subprocess(cmd, capture_stderr=False).output

def GetNormalizedCommandOutputAndLeakyTests(cmd):

  os.environ['GTEST_CATCH_EXCEPTIONS'] = '1'
  return GetNormalizedOutputAndLeakyTests(GetShellCommandOutput(cmd))

class GMockOutputTest(gmock_test_utils.TestCase):

  def testOutput(self):
    (output, leaky_tests) = GetNormalizedCommandOutputAndLeakyTests(COMMAND)
    golden_file = open(GOLDEN_PATH, 'rb')
    golden = golden_file.read().decode('utf-8')
    golden_file.close()

    golden = ToUnixLineEnding(golden)

    self.assertEqual(golden, output)

    self.assertEqual(
        [
            'GMockOutputTest.CatchesLeakedMocks',
            'GMockOutputTest.CatchesLeakedMocks',
        ],
        leaky_tests,
    )

if __name__ == '__main__':
  if sys.argv[1:] == [GENGOLDEN_FLAG]:
    (output, _) = GetNormalizedCommandOutputAndLeakyTests(COMMAND)
    golden_file = open(GOLDEN_PATH, 'wb')
    golden_file.write(output)
    golden_file.close()

    os._exit(0)
  else:
    gmock_test_utils.Main()
