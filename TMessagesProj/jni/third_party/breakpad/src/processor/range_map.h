// Copyright (c) 2006, Google Inc.
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

// range_map.h: Range maps.
//
// A range map associates a range of addresses with a specific object.  This
// is useful when certain objects of variable size are located within an
// address space.  The range map makes it simple to determine which object is
// associated with a specific address, which may be any address within the
// range associated with an object.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_RANGE_MAP_H__
#define PROCESSOR_RANGE_MAP_H__

#include <map>

namespace google_breakpad {

template<class, class> class RangeMapSerializer;

template<typename AddressType, typename EntryType>
class RangeMap {
 public:
  RangeMap() : map_() {}

  bool StoreRange(const AddressType &base,
                  const AddressType &size,
                  const EntryType &entry);

  bool RetrieveRange(const AddressType &address, EntryType *entry,
                     AddressType *entry_base, AddressType *entry_size) const;

  bool RetrieveNearestRange(const AddressType &address, EntryType *entry,
                            AddressType *entry_base, AddressType *entry_size)
                            const;

  bool RetrieveRangeAtIndex(int index, EntryType *entry,
                            AddressType *entry_base, AddressType *entry_size)
                            const;

  int GetCount() const;

  void Clear();

 private:

  friend class ModuleComparer;
  friend class RangeMapSerializer<AddressType, EntryType>;

  class Range {
   public:
    Range(const AddressType &base, const EntryType &entry)
        : base_(base), entry_(entry) {}

    AddressType base() const { return base_; }
    EntryType entry() const { return entry_; }

   private:

    const AddressType base_;

    const EntryType entry_;
  };

  typedef std::map<AddressType, Range> AddressToRangeMap;
  typedef typename AddressToRangeMap::const_iterator MapConstIterator;
  typedef typename AddressToRangeMap::value_type MapValue;

  AddressToRangeMap map_;
};

}

#endif
