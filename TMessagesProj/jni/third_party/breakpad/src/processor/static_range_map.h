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
// static_range_map.h: StaticRangeMap.
//
// StaticRangeMap is similar as RangeMap.  However, StaticRangeMap wraps a
// StaticMap instead of std::map, and does not support dynamic operations like
// StoreRange(...).  StaticRangeMap provides same Retrieve*() interfaces as
// RangeMap.  Please see range_map.h for more documentation.
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_STATIC_RANGE_MAP_H__
#define PROCESSOR_STATIC_RANGE_MAP_H__

#include "processor/static_map-inl.h"

namespace google_breakpad {

template<typename AddressType, typename EntryType>
class StaticRangeMap {
 public:
  StaticRangeMap(): map_() { }
  explicit StaticRangeMap(const char *memory): map_(memory) { }

  bool RetrieveRange(const AddressType &address, const EntryType *&entry,
                     AddressType *entry_base, AddressType *entry_size) const;

  bool RetrieveNearestRange(const AddressType &address, const EntryType *&entry,
                            AddressType *entry_base, AddressType *entry_size)
                            const;

  bool RetrieveRangeAtIndex(int index, const EntryType *&entry,
                            AddressType *entry_base, AddressType *entry_size)
                            const;

  inline int GetCount() const { return map_.size(); }

 private:
  friend class ModuleComparer;
  class Range {
   public:
    AddressType base() const {
      return *(reinterpret_cast<const AddressType*>(this));
    }
    const EntryType* entryptr() const {
      return reinterpret_cast<const EntryType*>(this + sizeof(AddressType));
    }
  };

  typedef StaticRangeMap* SelfPtr;
  typedef StaticMap<AddressType, Range> AddressToRangeMap;
  typedef typename AddressToRangeMap::const_iterator MapConstIterator;

  AddressToRangeMap map_;
};

}

#endif
