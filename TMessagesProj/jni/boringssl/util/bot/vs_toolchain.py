# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import json
import os
import os.path
import subprocess
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
toolchain_dir = os.path.join(script_dir, 'win_toolchain')
json_data_file = os.path.join(script_dir, 'win_toolchain.json')

def SetEnvironmentForCPU(cpu):

  with open(json_data_file, 'r') as tempf:
    toolchain_data = json.load(tempf)
  sdk_dir = toolchain_data['win_sdk']
  os.environ['WINDOWSSDKDIR'] = sdk_dir
  os.environ['WDK_DIR'] = toolchain_data['wdk']

  vs_runtime_dll_dirs = toolchain_data['runtime_dirs']
  runtime_path = os.pathsep.join(vs_runtime_dll_dirs)
  os.environ['PATH'] = runtime_path + os.pathsep + os.environ['PATH']

  assert cpu in ('x86', 'x64', 'arm', 'arm64')
  with open(os.path.join(sdk_dir, 'bin', 'SetEnv.%s.json' % cpu)) as f:
    env = json.load(f)['env']
  if env['VSINSTALLDIR'] == [["..", "..\\"]]:

    json_relative_dir = os.path.join(sdk_dir, 'bin')
  else:

    json_relative_dir = toolchain_data['path']
  for k in env:
    entries = [os.path.join(*([json_relative_dir] + e)) for e in env[k]]

    sep = os.pathsep if k == 'PATH' else ';'
    env[k] = sep.join(entries)

  env['PATH'] = env['PATH'] + os.pathsep + os.environ['PATH']

  for k, v in env.items():
    os.environ[k] = v

def FindDepotTools():

  for path in os.environ['PATH'].split(os.pathsep):
    if os.path.isfile(os.path.join(path, 'gclient.py')):
      return path
  raise Exception("depot_tools not found!")

def _GetDesiredVsToolchainHashes(version):

  if version == '2022':

    return ['7393122652']
  raise Exception('Unsupported VS version %s' % version)

def Update(version):

  depot_tools_path = FindDepotTools()
  get_toolchain_args = [
      sys.executable,
      os.path.join(depot_tools_path,
                  'win_toolchain',
                  'get_toolchain_if_necessary.py'),
      '--output-json', json_data_file,
      '--toolchain-dir', toolchain_dir,
    ] + _GetDesiredVsToolchainHashes(version)
  subprocess.check_call(get_toolchain_args)
  return 0

def main():
  if not sys.platform.startswith(('win32', 'cygwin')):
    return 0
  commands = {
      'update': Update,
  }
  if len(sys.argv) < 2 or sys.argv[1] not in commands:
    print('Expected one of: %s' % ', '.join(commands), file=sys.stderr)
    return 1
  return commands[sys.argv[1]](*sys.argv[2:])

if __name__ == '__main__':
  sys.exit(main())
