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

import re
from xml.dom import minidom, Node
from googletest.test import gtest_test_utils

GTEST_DEFAULT_OUTPUT_FILE = 'test_detail.xml'

class GTestXMLTestCase(gtest_test_utils.TestCase):

  def AssertEquivalentNodes(self, expected_node, actual_node):

    if expected_node.nodeType == Node.CDATA_SECTION_NODE:
      self.assertEqual(Node.CDATA_SECTION_NODE, actual_node.nodeType)
      self.assertEqual(expected_node.nodeValue, actual_node.nodeValue)
      return

    self.assertEqual(Node.ELEMENT_NODE, actual_node.nodeType)
    self.assertEqual(Node.ELEMENT_NODE, expected_node.nodeType)
    self.assertEqual(expected_node.tagName, actual_node.tagName)

    expected_attributes = expected_node.attributes
    actual_attributes = actual_node.attributes
    self.assertEqual(
        expected_attributes.length,
        actual_attributes.length,
        'attribute numbers differ in element %s:\nExpected: %r\nActual: %r'
        % (
            actual_node.tagName,
            expected_attributes.keys(),
            actual_attributes.keys(),
        ),
    )
    for i in range(expected_attributes.length):
      expected_attr = expected_attributes.item(i)
      actual_attr = actual_attributes.get(expected_attr.name)
      self.assertTrue(
          actual_attr is not None,
          'expected attribute %s not found in element %s'
          % (expected_attr.name, actual_node.tagName),
      )
      self.assertEqual(
          expected_attr.value,
          actual_attr.value,
          ' values of attribute %s in element %s differ: %s vs %s'
          % (
              expected_attr.name,
              actual_node.tagName,
              expected_attr.value,
              actual_attr.value,
          ),
      )

    expected_children = self._GetChildren(expected_node)
    actual_children = self._GetChildren(actual_node)
    self.assertEqual(
        len(expected_children),
        len(actual_children),
        'number of child elements differ in element ' + actual_node.tagName,
    )
    for child_id, child in expected_children.items():
      self.assertTrue(
          child_id in actual_children,
          '<%s> is not in <%s> (in element %s)'
          % (child_id, actual_children, actual_node.tagName),
      )
      self.AssertEquivalentNodes(child, actual_children[child_id])

  identifying_attribute = {
      'testsuites': 'name',
      'testsuite': 'name',
      'testcase': 'name',
      'failure': 'message',
      'skipped': 'message',
      'property': 'name',
  }

  def _GetChildren(self, element):

    children = {}
    for child in element.childNodes:
      if child.nodeType == Node.ELEMENT_NODE:
        if child.tagName == 'properties':
          self.assertTrue(
              child.parentNode is not None,
              'Encountered <properties> element without a parent',
          )
          child_id = child.parentNode.getAttribute('name') + '-properties'
        else:
          self.assertTrue(
              child.tagName in self.identifying_attribute,
              'Encountered unknown element <%s>' % child.tagName,
          )
          child_id = child.getAttribute(
              self.identifying_attribute[child.tagName]
          )
        self.assertNotIn(child_id, children)
        children[child_id] = child
      elif child.nodeType in [Node.TEXT_NODE, Node.CDATA_SECTION_NODE]:
        if 'detail' not in children:
          if (
              child.nodeType == Node.CDATA_SECTION_NODE
              or not child.nodeValue.isspace()
          ):
            children['detail'] = child.ownerDocument.createCDATASection(
                child.nodeValue
            )
        else:
          children['detail'].nodeValue += child.nodeValue
      else:
        self.fail('Encountered unexpected node type %d' % child.nodeType)
    return children

  def NormalizeXml(self, element):

    if element.tagName == 'testcase':
      source_file = element.getAttributeNode('file')
      if source_file:
        source_file.value = re.sub(r'^.*[/\\](.*)', '\\1', source_file.value)
    if element.tagName in ('testsuites', 'testsuite', 'testcase'):
      timestamp = element.getAttributeNode('timestamp')
      timestamp.value = re.sub(
          r'^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d\d\d$', '*', timestamp.value
      )
    if element.tagName in ('testsuites', 'testsuite', 'testcase'):
      time = element.getAttributeNode('time')

      time.value = re.sub(r'^\d+\.(\d+)?$', '*', time.value)
      type_param = element.getAttributeNode('type_param')
      if type_param and type_param.value:
        type_param.value = '*'
    elif element.tagName == 'failure' or element.tagName == 'skipped':
      source_line_pat = r'^.*[/\\](.*:)\d+\n'

      message = element.getAttributeNode('message')
      message.value = re.sub(source_line_pat, '\\1*\n', message.value)
      for child in element.childNodes:
        if child.nodeType == Node.CDATA_SECTION_NODE:

          cdata = re.sub(source_line_pat, '\\1*\n', child.nodeValue)

          child.nodeValue = re.sub(
              r'Stack trace:\n(.|\n)*', 'Stack trace:\n*', cdata
          )
    for child in element.childNodes:
      if child.nodeType == Node.ELEMENT_NODE:
        self.NormalizeXml(child)
