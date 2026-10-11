#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import base64
import os
import re
import sys

failed_test_regex = re.compile(r"""
Cert errors don't match expectations \((.+?)\)

EXPECTED:

(?:.|\n)*?
ACTUAL:

((?:.|\n)*?)
===> Use pki/testdata/parse_certificate_unittest/rebase-errors.py to rebaseline.
""", re.MULTILINE)

errors_block_regex = re.compile(r""".*
-----END .*?-----
(.*?
-----BEGIN ERRORS-----
.*?
-----END ERRORS-----)""", re.MULTILINE | re.DOTALL)

def read_file_to_string(path):

  with open(path, 'r') as f:
    return f.read()

def write_string_to_file(data, path):

  print("Writing file %s ..." % (path))
  with open(path, "w") as f:
    f.write(data)

def replace_string(original, start, end, replacement):

  return original[0:start] + replacement + original[end:]

def text_data_to_pem(block_header, text_data):

  pem_data = base64.b64encode(text_data.encode('utf8')).decode('utf8')
  return '%s\n-----BEGIN %s-----\n%s\n-----END %s-----\n' % (
      text_data, block_header, pem_data, block_header)

def fixup_pem_file(path, actual_errors):

  contents = read_file_to_string(path)

  errors_block_text = '\n' + text_data_to_pem('ERRORS', actual_errors)

  errors_block_text = errors_block_text[:-1]

  m = errors_block_regex.search(contents)

  if not m:
    contents += errors_block_text
  else:
    contents = replace_string(contents, m.start(1), m.end(1),
                              errors_block_text)

  write_string_to_file(contents, path)

def get_src_root():

  cur_dir = os.path.dirname(os.path.realpath(__file__))

  while True:

    if os.path.isdir(os.path.join(cur_dir, "crypto")) and \
       os.path.isdir(os.path.join(cur_dir, "pki")) and \
       os.path.isdir(os.path.join(cur_dir, "ssl")):
      return cur_dir
    parent_dir, _ = os.path.split(cur_dir)
    if not parent_dir or parent_dir == cur_dir:
      break
    cur_dir = parent_dir

  print("Couldn't find src dir")
  sys.exit(1)

def get_abs_path(rel_path):

  return os.path.join(get_src_root(), rel_path)

def main():
  if len(sys.argv) > 2:
    print('Usage: %s [path-to-unittest-stdout]' % (sys.argv[0]))
    sys.exit(1)

  test_stdout = None
  if len(sys.argv) == 2:
    test_stdout = read_file_to_string(sys.argv[1])
  else:
    print('Reading input from stdin...')
    test_stdout = sys.stdin.read()

  for m in failed_test_regex.finditer(test_stdout):
    src_relative_errors_path = "pki/" + m.group(1)
    errors_path = get_abs_path(src_relative_errors_path)
    actual_errors = m.group(2)

    fixup_pem_file(errors_path, actual_errors)

if __name__ == "__main__":
  main()
