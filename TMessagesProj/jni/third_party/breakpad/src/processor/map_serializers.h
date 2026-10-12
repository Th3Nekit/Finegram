// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// map_serializers.h: defines templates for serializing std::map and its
// wrappers: AddressMap, RangeMap, and ContainedRangeMap.
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_MAP_SERIALIZERS_H__
#define PROCESSOR_MAP_SERIALIZERS_H__

#include <map>
#include <string>

#include "processor/simple_serializer.h"

#include "processor/address_map-inl.h"
#include "processor/range_map-inl.h"
#include "processor/contained_range_map-inl.h"

namespace google_breakpad {

template<typename Key, typename Value>
class StdMapSerializer {
 public:

  size_t SizeOf(const std::map<Key, Value> &m) const;

  char* Write(const std::map<Key, Value> &m, char* dest) const;

  char* Serialize(const std::map<Key, Value> &m, unsigned int *size) const;

 private:
  SimpleSerializer<Key> key_serializer_;
  SimpleSerializer<Value> value_serializer_;
};

template<typename Addr, typename Entry>
class AddressMapSerializer {
 public:

  size_t SizeOf(const AddressMap<Addr, Entry> &m) const {
    return std_map_serializer_.SizeOf(m.map_);
  }

  char* Write(const AddressMap<Addr, Entry> &m, char *dest) const {
    return std_map_serializer_.Write(m.map_, dest);
  }

  char* Serialize(const AddressMap<Addr, Entry> &m, unsigned int *size) const {
    return std_map_serializer_.Serialize(m.map_, size);
  }

 private:

  StdMapSerializer<Addr, Entry> std_map_serializer_;
};

template<typename Address, typename Entry>
class RangeMapSerializer {
 public:

  size_t SizeOf(const RangeMap<Address, Entry> &m) const;

  char* Write(const RangeMap<Address, Entry> &m, char* dest) const;

  char* Serialize(const RangeMap<Address, Entry> &m, unsigned int *size) const;

 private:

  typedef typename RangeMap<Address, Entry>::Range Range;

  SimpleSerializer<Address> address_serializer_;

  SimpleSerializer<Entry> entry_serializer_;
};

template<class AddrType, class EntryType>
class ContainedRangeMapSerializer {
 public:

  size_t SizeOf(const ContainedRangeMap<AddrType, EntryType> *m) const;

  char* Write(const ContainedRangeMap<AddrType, EntryType> *m,
              char* dest) const;

  char* Serialize(const ContainedRangeMap<AddrType, EntryType> *m,
                  unsigned int *size) const;

 private:

  typedef std::map<AddrType, ContainedRangeMap<AddrType, EntryType>*> Map;

  SimpleSerializer<AddrType> addr_serializer_;
  SimpleSerializer<EntryType> entry_serializer_;
};

}

#endif
