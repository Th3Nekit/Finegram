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

import os
import re
import subprocess
import sys
import tempfile

def sanitize_name(s):
  return s.translate(str.maketrans('', '', ' -'))

def finalize_test_case(test_case_name, sanitized_test_names, output):
  output.write('\nWRAPPED_REGISTER_TYPED_TEST_SUITE_P(%s' % test_case_name)
  for name in sanitized_test_names:
    output.write(',\n    %s' % name)
  output.write(');\n')

def bool_to_str(b):
  return "true" if b else "false"

def make_policies_string(policies):
  return '"' + ','.join(policies) + '"'

def output_test(test_case_name, test_number, raw_test_name, subpart_number,
                info, certs, crls, sanitized_test_names, output):

  sanitized_test_name = 'Section%s%s' % (test_number.split('.')[1],
                                         sanitize_name(raw_test_name))

  subpart_comment = ''
  if subpart_number is not None:
    sanitized_test_name += "Subpart%d" % (subpart_number)
    subpart_comment = ' (Subpart %d)' % (subpart_number)

  sanitized_test_names.append(sanitized_test_name)

  certs_formatted = ', '.join('"%s"' % n for n in certs)
  crls_formatted = ', '.join('"%s"' % n for n in crls)

  output.write('''
// %(test_number)s %(raw_test_name)s%(subpart_comment)s
WRAPPED_TYPED_TEST_P(%(test_case_name)s, %(sanitized_test_name)s) {
  const char* const certs[] = {
    %(certs_formatted)s
  };
  const char* const crls[] = {
    %(crls_formatted)s
  };
''' % vars())

  default_info = TestInfo(None)

  if info.include_subpart_in_test_number:
    test_number = "%s.%d" % (test_number, subpart_number)

  output.write('''PkitsTestInfo info;
  info.test_number = "%s";
  info.should_validate = %s;
''' % (test_number, bool_to_str(info.should_validate)))

  if info.initial_policy_set != default_info.initial_policy_set:
    output.write('''  info.SetInitialPolicySet(%s);
''' % make_policies_string(info.initial_policy_set))

  if info.initial_explicit_policy != default_info.initial_explicit_policy:
    output.write('''  info.SetInitialExplicitPolicy(%s);
''' % bool_to_str(info.initial_explicit_policy))

  if (info.initial_policy_mapping_inhibit !=
          default_info.initial_policy_mapping_inhibit):
    output.write('''  info.SetInitialPolicyMappingInhibit(%s);
''' % bool_to_str(info.initial_policy_mapping_inhibit))

  if (info.initial_inhibit_any_policy !=
          default_info.initial_inhibit_any_policy):
    output.write('''  info.SetInitialInhibitAnyPolicy(%s);
''' % bool_to_str(info.initial_inhibit_any_policy))

  if (info.user_constrained_policy_set !=
          default_info.user_constrained_policy_set):
    output.write('''  info.SetUserConstrainedPolicySet(%s);
''' % make_policies_string(info.user_constrained_policy_set))

  output.write('''
  this->RunTest(certs, crls, info);
}
''' % vars())

SECTION_MATCHER = re.compile('^\s*(\d+\.\d+)\s+(.+?)\s*\ufffd?$')

TEST_MATCHER = re.compile('^\s*(\d+\.\d+.\d+)\s+(.+?)\s*\ufffd?$')

EXPECTED_HEADER_MATCHER = re.compile('^\s*Expected Result:')
PROCEDURE_HEADER_MATCHER = re.compile('^\s*Procedure:')
PATH_HEADER_MATCHER = re.compile('^\s*Certification Path:')

USING_DEFAULT_SETTINGS_MATCHER = re.compile(
    '^.*using the \s*default settings.*')

CUSTOM_SETTINGS_MATCHER = re.compile(
    '.*this\s+test\s+be\s+validated\s+using\s+the\s+following\s+inputs:.*')

TEST_RESULT_MATCHER = re.compile(
    '^.*path (should validate|should not validate|not should validate)')

PATH_MATCHER = re.compile('^\s*\u2022\s*(.+)\s*$')

PAGE_NUMBER_MATCHER = re.compile('^\s*\d+\s*$')

CRL_MATCHER = re.compile('^.*CRL\d*$')

class TestSections(object):
  def __init__(self):
    self.description_lines = []
    self.procedure_lines = []
    self.expected_result_lines = []
    self.cert_path_lines = []

def parse_main_test_sections(lines, i):
  result = TestSections()

  result.description_lines = []
  while i < len(lines):
    if PROCEDURE_HEADER_MATCHER.match(lines[i]):
      break
    result.description_lines.append(lines[i])
    i += 1

  result.procedure_lines = []
  while i < len(lines):
    if EXPECTED_HEADER_MATCHER.match(lines[i]):
      break
    result.procedure_lines.append(lines[i])
    i += 1

  result.expected_result_lines = []
  while i < len(lines):
    if PATH_HEADER_MATCHER.match(lines[i]):
      break
    result.expected_result_lines.append(lines[i])
    i += 1

  result.cert_path_lines = []
  while i < len(lines):
    if TEST_MATCHER.match(lines[i]) or SECTION_MATCHER.match(lines[i]):
      break
    result.cert_path_lines.append(lines[i])
    i += 1

  return i, result

def parse_cert_path_lines(lines):
  path_lines = []
  crls = []
  certs = []

  for line in lines[1:]:
    line = line.strip()

    if "is composed of the following objects:" in line:
      continue
    if "See the introduction to Section 4.4 for more information." in line:
      continue

    if not line or PAGE_NUMBER_MATCHER.match(line):
      continue
    path_match = PATH_MATCHER.match(line)
    if path_match:
      path_lines.append(path_match.group(1))
      continue

    path_lines[-1] += ' ' + line

  for path_line in path_lines:
    for path in path_line.split(','):
      path = sanitize_name(path.strip())
      if CRL_MATCHER.match(path):
        crls.append(path)
      else:
        certs.append(path)

  return certs, crls

ANY_POLICY = 'anyPolicy'
TEST_POLICY_1 = 'NIST-test-policy-1'
TEST_POLICY_2 = 'NIST-test-policy-2'
TEST_POLICY_3 = 'NIST-test-policy-3'
TEST_POLICY_6 = 'NIST-test-policy-6'

class TestInfo(object):

  def __init__(self, should_validate,

               initial_policy_set = [ANY_POLICY],
               initial_explicit_policy = False,
               initial_policy_mapping_inhibit = False,
               initial_inhibit_any_policy = False,

               user_constrained_policy_set = [TEST_POLICY_1],
               include_subpart_in_test_number = False):
    self.should_validate = should_validate
    self.initial_policy_set = initial_policy_set
    self.initial_explicit_policy = initial_explicit_policy
    self.initial_policy_mapping_inhibit = initial_policy_mapping_inhibit
    self.initial_inhibit_any_policy = initial_inhibit_any_policy
    self.user_constrained_policy_set = user_constrained_policy_set
    self.include_subpart_in_test_number = include_subpart_in_test_number

TEST_OVERRIDES = {
  '4.8.1': [

    TestInfo(True, initial_explicit_policy=True,
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_explicit_policy=True,
             initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_explicit_policy=True,
             initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[]),

    TestInfo(True, initial_explicit_policy=True,
             initial_policy_set=[TEST_POLICY_1, TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.2': [

    TestInfo(True, user_constrained_policy_set=[]),

    TestInfo(False, initial_explicit_policy=True,
             user_constrained_policy_set=[]),
  ],

  '4.8.3': [

    TestInfo(True, user_constrained_policy_set=[]),

    TestInfo(False, initial_explicit_policy=True, user_constrained_policy_set=[]),

    TestInfo(False, initial_explicit_policy=True,
             initial_policy_set=[TEST_POLICY_1, TEST_POLICY_2],
             user_constrained_policy_set=[]),
  ],

  '4.8.4': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.5': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.6': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[]),
  ],

  '4.8.7': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.8': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.9': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.10': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1, TEST_POLICY_2]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_2]),
  ],

  '4.8.11': [

    TestInfo(True, user_constrained_policy_set=[ANY_POLICY]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.12': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.8.13': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_2]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_3],
             user_constrained_policy_set=[TEST_POLICY_3]),
  ],

  '4.8.14': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[]),
  ],

  '4.8.15': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.16': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.17': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.18': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_2]),
  ],

  '4.8.19': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.8.20': [

    TestInfo(True, initial_explicit_policy=True,
             initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.9.1': [

    TestInfo(True, user_constrained_policy_set=[]),
  ],

  '4.9.2': [

    TestInfo(True, user_constrained_policy_set=[]),
  ],

  '4.9.6': [

    TestInfo(True, user_constrained_policy_set=[]),
  ],

  '4.10.1': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1],
             include_subpart_in_test_number=True),

    TestInfo(False, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[],
             include_subpart_in_test_number=True),

    TestInfo(False, initial_policy_mapping_inhibit=True,
             user_constrained_policy_set=[],
             include_subpart_in_test_number=True),
  ],

  '4.10.2': [

    TestInfo(False, user_constrained_policy_set=[]),

    TestInfo(False, initial_policy_mapping_inhibit=True,
             user_constrained_policy_set=[]),
  ],

  '4.10.3': [

    TestInfo(False, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_2]),
  ],

  '4.10.4': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.10.5': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_policy_set=[TEST_POLICY_6],
             user_constrained_policy_set=[]),
  ],

  '4.10.6': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
                   user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_policy_set=[TEST_POLICY_6],
             user_constrained_policy_set=[]),
  ],

  '4.10.7': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.10.8': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.10.9': [

    TestInfo(True),
  ],

  '4.10.10': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.10.11': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.10.12': [

    TestInfo(True, initial_policy_set=[TEST_POLICY_1],
             user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_2]),
  ],

  '4.10.13': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(True, initial_policy_set=[TEST_POLICY_1, TEST_POLICY_2],
             user_constrained_policy_set=[TEST_POLICY_1]),
    TestInfo(False, initial_policy_set=[TEST_POLICY_2],
             user_constrained_policy_set=[]),
  ],

  '4.10.14': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.11.1': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.2': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.11.3': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.4': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_2]),
  ],

  '4.11.5': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.6': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.7': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.11.8': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.9': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.10': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.11.11': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.1': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.2': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.12.3': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),

    TestInfo(False, initial_inhibit_any_policy=True,
             user_constrained_policy_set=[]),
  ],

  '4.12.4': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.5': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.6': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.7': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.12.8': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],

  '4.12.9': [

    TestInfo(True, user_constrained_policy_set=[TEST_POLICY_1]),
  ],

  '4.12.10': [

    TestInfo(False, user_constrained_policy_set=[]),
  ],
}

def parse_test(lines, i, test_case_name, test_number, test_name,
               sanitized_test_names, output):

  i, test_sections = parse_main_test_sections(lines, i)

  certs, crls = parse_cert_path_lines(test_sections.cert_path_lines)

  overrides = TEST_OVERRIDES.get(test_number, None)

  if overrides is None:

    if CUSTOM_SETTINGS_MATCHER.match(" ".join(test_sections.description_lines)):
      sys.stderr.write('Unexpected custom settings for %s\n' % test_number)
      sys.exit(1)

    if not USING_DEFAULT_SETTINGS_MATCHER.match(
        " ".join(test_sections.procedure_lines)):
      sys.stderr.write('Unexpected procedure for %s: %s\n' %
                       (test_number, " ".join(test_section.procedure_lines)))
      sys.exit(1)

    result_match = TEST_RESULT_MATCHER.match(
       test_sections.expected_result_lines[0])
    if not result_match:
      sys.stderr.write('Unknown expectation for %s:\n%s\n' % (
          test_number, " ".join(test_sections.expected_result_lines)))
      sys.exit(1)

    info = TestInfo(result_match.group(1) == 'should validate')

    if test_number.startswith('4.9.') and not info.should_validate:
      info.user_constrained_policy_set = []

    output_test(test_case_name, test_number, test_name, None, info, certs,
                crls, sanitized_test_names, output)
  else:

    for subpart_i in range(len(overrides)):
      info = overrides[subpart_i]

      subpart_number = subpart_i + 1 if len(overrides) > 1 else None
      output_test(test_case_name, test_number, test_name, subpart_number, info,
                  certs, crls, sanitized_test_names, output)

  return i

def main():
  pkits_pdf_path, output_path = sys.argv[1:]

  pkits_txt_file = tempfile.NamedTemporaryFile()

  subprocess.check_call(['pdftotext', '-layout', '-nopgbrk', '-eol', 'unix',
                         pkits_pdf_path, pkits_txt_file.name])

  test_descriptions = pkits_txt_file.read().decode('utf-8')

  test_descriptions = test_descriptions.split(
      '4 Certification Path Validation Tests')[-1]
  test_descriptions = test_descriptions.split(
      '5 Relationship to Previous Test Suite', 1)[0]

  output = open(output_path, 'w')
  output.write('// Autogenerated by %s, do not edit\n\n' % sys.argv[0])
  output.write("""
// This file intentionally does not have header guards, it's intended to
// be inlined in another header file. The following line silences a
// presubmit warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)\n\n""")
  output.write('// Hack to allow disabling type parameterized test cases.\n'
               '// See https://github.com/google/googletest/issues/389\n')
  output.write('#define WRAPPED_TYPED_TEST_P(CaseName, TestName) '
               'TYPED_TEST_P(CaseName, TestName)\n')
  output.write('#define WRAPPED_REGISTER_TYPED_TEST_SUITE_P(CaseName, ...) '
               'REGISTER_TYPED_TEST_SUITE_P(CaseName, __VA_ARGS__)\n\n')

  test_case_name = None
  sanitized_test_names = []

  lines = test_descriptions.splitlines()

  i = 0
  while i < len(lines):
    section_match = SECTION_MATCHER.match(lines[i])
    match = TEST_MATCHER.match(lines[i])
    i += 1

    if section_match:
      if test_case_name:
        finalize_test_case(test_case_name, sanitized_test_names, output)
        sanitized_test_names = []

      test_case_name = 'PkitsTest%02d%s' % (
          int(section_match.group(1).split('.')[-1]),
          sanitize_name(section_match.group(2)))
      output.write('\ntemplate <typename PkitsTestDelegate>\n')
      output.write('class %s : public PkitsTest<PkitsTestDelegate> {};\n' %
                   test_case_name)
      output.write('TYPED_TEST_SUITE_P(%s);\n' % test_case_name)

    if match:
      test_number = match.group(1)
      test_name = match.group(2)
      if not test_case_name:
        output.write('// Skipped %s %s\n' % (test_number, test_name))
        continue
      i, parse_test(lines, i, test_case_name, test_number,
                    test_name, sanitized_test_names, output)

  if test_case_name:
    finalize_test_case(test_case_name, sanitized_test_names, output)

if __name__ == '__main__':
  main()
