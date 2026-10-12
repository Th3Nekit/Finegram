// Copyright (c) 2010 Google Inc.
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

// range_map-inl.h: Range map implementation.
//
// See range_map.h for documentation.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_RANGE_MAP_INL_H__
#define PROCESSOR_RANGE_MAP_INL_H__

#include <assert.h>

#include "processor/range_map.h"
#include "processor/logging.h"

namespace google_breakpad {

template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::StoreRange(const AddressType &base,
                                                  const AddressType &size,
                                                  const EntryType &entry) {
  AddressType high = base + size - 1;

  if (size <= 0 || high < base) {

    BPLOG_IF(INFO, size != 0) << "StoreRange failed, " << HexString(base) <<
                                 "+" << HexString(size) << ", " <<
                                 HexString(high);
    return false;
  }

  MapConstIterator iterator_base = map_.lower_bound(base);
  MapConstIterator iterator_high = map_.lower_bound(high);

  if (iterator_base != iterator_high) {

    return false;
  }

  if (iterator_high != map_.end()) {
    if (iterator_high->second.base() <= high) {

      return false;
    }
  }

  map_.insert(MapValue(high, Range(base, entry)));
  return true;
}

template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType &address, EntryType *entry,
    AddressType *entry_base, AddressType *entry_size) const {
  BPLOG_IF(ERROR, !entry) << "RangeMap::RetrieveRange requires |entry|";
  assert(entry);

  MapConstIterator iterator = map_.lower_bound(address);
  if (iterator == map_.end())
    return false;

  if (address < iterator->second.base())
    return false;

  *entry = iterator->second.entry();
  if (entry_base)
    *entry_base = iterator->second.base();
  if (entry_size)
    *entry_size = iterator->first - iterator->second.base() + 1;

  return true;
}

template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveNearestRange(
    const AddressType &address, EntryType *entry,
    AddressType *entry_base, AddressType *entry_size) const {
  BPLOG_IF(ERROR, !entry) << "RangeMap::RetrieveNearestRange requires |entry|";
  assert(entry);

  if (RetrieveRange(address, entry, entry_base, entry_size))
    return true;

  MapConstIterator iterator = map_.upper_bound(address);
  if (iterator == map_.begin())
    return false;
  --iterator;

  *entry = iterator->second.entry();
  if (entry_base)
    *entry_base = iterator->second.base();
  if (entry_size)
    *entry_size = iterator->first - iterator->second.base() + 1;

  return true;
}

template<typename AddressType, typename EntryType>
bool RangeMap<AddressType, EntryType>::RetrieveRangeAtIndex(
    int index, EntryType *entry,
    AddressType *entry_base, AddressType *entry_size) const {
  BPLOG_IF(ERROR, !entry) << "RangeMap::RetrieveRangeAtIndex requires |entry|";
  assert(entry);

  if (index >= GetCount()) {
    BPLOG(ERROR) << "Index out of range: " << index << "/" << GetCount();
    return false;
  }

  MapConstIterator iterator = map_.begin();
  for (int this_index = 0; this_index < index; ++this_index)
    ++iterator;

  *entry = iterator->second.entry();
  if (entry_base)
    *entry_base = iterator->second.base();
  if (entry_size)
    *entry_size = iterator->first - iterator->second.base() + 1;

  return true;
}

template<typename AddressType, typename EntryType>
int RangeMap<AddressType, EntryType>::GetCount() const {
  return map_.size();
}

template<typename AddressType, typename EntryType>
void RangeMap<AddressType, EntryType>::Clear() {
  map_.clear();
}

}

#endif
