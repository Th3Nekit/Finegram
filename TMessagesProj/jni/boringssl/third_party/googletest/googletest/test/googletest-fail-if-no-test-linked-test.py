#!/usr/bin/env python3  # pylint: disable=g-interpreter-mismatch
#
# Copyright 2025, Google Inc.
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

from googletest.test import gtest_test_utils

FAIL_IF_NO_TEST_LINKED_FLAG = "gtest_fail_if_no_test_linked"

class GTestFailIfNoTestLinkedTest(gtest_test_utils.TestCase):

  def Run(self, program_name, flag=None):

    exe_path = gtest_test_utils.GetTestExecutablePath(program_name)
    args = [exe_path]
    if flag is not None:
      args += [flag]
    process = gtest_test_utils.Subprocess(args, capture_stderr=False)
    return process.exited and process.exit_code == 0

  def testSucceedsIfNoTestLinkedAndFlagNotSpecified(self):

    self.assertTrue(
        self.Run("googletest-fail-if-no-test-linked-test-without-test_")
    )

  def testFailsIfNoTestLinkedAndFlagSpecified(self):

    self.assertFalse(
        self.Run(
            "googletest-fail-if-no-test-linked-test-without-test_",
            f"--{FAIL_IF_NO_TEST_LINKED_FLAG}",
        )
    )

  def testSucceedsIfEnabledTestLinkedAndFlagNotSpecified(self):

    self.assertTrue(
        self.Run("googletest-fail-if-no-test-linked-test-with-enabled-test_")
    )

  def testSucceedsIfEnabledTestLinkedAndFlagSpecified(self):

    self.assertTrue(
        self.Run(
            "googletest-fail-if-no-test-linked-test-with-enabled-test_",
            f"--{FAIL_IF_NO_TEST_LINKED_FLAG}",
        )
    )

  def testSucceedsIfDisabledTestLinkedAndFlagNotSpecified(self):

    self.assertTrue(
        self.Run("googletest-fail-if-no-test-linked-test-with-disabled-test_")
    )

  def testSucceedsIfDisabledTestLinkedAndFlagSpecified(self):

    self.assertTrue(
        self.Run(
            "googletest-fail-if-no-test-linked-test-with-disabled-test_",
            f"--{FAIL_IF_NO_TEST_LINKED_FLAG}",
        )
    )

if __name__ == "__main__":
  gtest_test_utils.Main()
