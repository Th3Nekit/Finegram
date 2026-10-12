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
import subprocess
import sys

IS_WINDOWS = os.name == 'nt'
IS_CYGWIN = os.name == 'posix' and 'CYGWIN' in os.uname()[0]
IS_OS2 = os.name == 'os2'

import atexit
import shutil
import tempfile
import unittest as _test_module

GTEST_OUTPUT_VAR_NAME = 'GTEST_OUTPUT'

PREMATURE_EXIT_FILE_ENV_VAR = 'TEST_PREMATURE_EXIT_FILE'

environ = os.environ.copy()

def SetEnvVar(env_var, value):

  if value is not None:
    environ[env_var] = value
  elif env_var in environ:
    del environ[env_var]

TestCase = _test_module.TestCase

_flag_map = {
    'source_dir': os.path.dirname(sys.argv[0]),
    'build_dir': os.path.dirname(sys.argv[0]),
}
_gtest_flags_are_parsed = False

def _ParseAndStripGTestFlags(argv):

  global _gtest_flags_are_parsed
  if _gtest_flags_are_parsed:
    return

  _gtest_flags_are_parsed = True
  for flag in _flag_map:

    if flag.upper() in os.environ:
      _flag_map[flag] = os.environ[flag.upper()]

    i = 1
    while i < len(argv):
      prefix = '--' + flag + '='
      if argv[i].startswith(prefix):
        _flag_map[flag] = argv[i][len(prefix) :]
        del argv[i]
        break
      else:

        i += 1

def GetFlag(flag):

  _ParseAndStripGTestFlags(sys.argv)

  return _flag_map[flag]

def GetSourceDir():

  return os.path.abspath(GetFlag('source_dir'))

def GetBuildDir():

  return os.path.abspath(GetFlag('build_dir'))

_temp_dir = None

def _RemoveTempDir():
  if _temp_dir:
    shutil.rmtree(_temp_dir, ignore_errors=True)

atexit.register(_RemoveTempDir)

def GetTempDir():
  global _temp_dir
  if not _temp_dir:
    _temp_dir = tempfile.mkdtemp()
  return _temp_dir

def GetTestExecutablePath(executable_name, build_dir=None):

  path = os.path.abspath(
      os.path.join(build_dir or GetBuildDir(), executable_name)
  )
  if (IS_WINDOWS or IS_CYGWIN or IS_OS2) and not path.endswith('.exe'):
    path += '.exe'

  if not os.path.exists(path):
    message = (
        'Unable to find the test binary "%s". Please make sure to provide\n'
        'a path to the binary via the --build_dir flag or the BUILD_DIR\n'
        'environment variable.' % path
    )
    print(message, file=sys.stderr)
    sys.exit(1)

  return path

def GetExitStatus(exit_code):

  if os.name == 'nt':

    return exit_code
  else:

    if os.WIFEXITED(exit_code):
      return os.WEXITSTATUS(exit_code)
    else:
      return -1

class Subprocess:

  def __init__(self, command, working_dir=None, capture_stderr=True, env=None):

    if capture_stderr:
      stderr = subprocess.STDOUT
    else:
      stderr = subprocess.PIPE

    p = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=stderr,
        cwd=working_dir,
        universal_newlines=True,
        env=env,
    )

    self.output = p.communicate()[0]
    self._return_code = p.returncode

    if bool(self._return_code & 0x80000000):
      self.terminated_by_signal = True
      self.exited = False
    else:
      self.terminated_by_signal = False
      self.exited = True
      self.exit_code = self._return_code

def Main():

  _ParseAndStripGTestFlags(sys.argv)

  if GTEST_OUTPUT_VAR_NAME in os.environ:
    del os.environ[GTEST_OUTPUT_VAR_NAME]

  _test_module.main()
