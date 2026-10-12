#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
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

root = gencerts.create_self_signed_root_certificate('Root')

intermediate1 = gencerts.create_intermediate_certificate('Intermediate1', root)
intermediate1.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

intermediate2 = gencerts.create_intermediate_certificate('Intermediate2',
                                                       intermediate1)
intermediate2.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

target = gencerts.create_end_entity_certificate('Target', intermediate2)

chain = [target, intermediate2, intermediate1, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
