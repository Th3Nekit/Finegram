#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
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

import sys
sys.path += ['../..']

import gencerts

def generate_chain(intermediate_digest_algorithm):

  root = gencerts.create_self_signed_root_certificate('Root')

  intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
  intermediate.set_signature_hash(intermediate_digest_algorithm)
  intermediate.get_extensions().set_property('extendedKeyUsage',
                                             'nsSGC')

  target = gencerts.create_end_entity_certificate('Target', intermediate)
  target.get_extensions().set_property('extendedKeyUsage',
                                   'serverAuth,clientAuth')

  target.get_extensions().set_property('subjectAltName', 'DNS:test.example')

  chain = [target, intermediate, root]
  gencerts.write_chain(__doc__, chain,
                       '%s-chain.pem' % intermediate_digest_algorithm)

for digest in ['sha1', 'sha256']:
  generate_chain(digest)
