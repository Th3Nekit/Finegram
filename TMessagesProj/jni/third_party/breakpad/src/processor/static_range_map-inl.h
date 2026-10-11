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

// static_range_map-inl.h: StaticRangeMap implementation.
//
// See static_range_map.h for documentation.
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_STATIC_RANGE_MAP_INL_H__
#define PROCESSOR_STATIC_RANGE_MAP_INL_H__

#include "processor/static_range_map.h"
#include "processor/logging.h"

namespace google_breakpad {

template<typename AddressType, typename EntryType>
bool StaticRangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType &address, const EntryType *&entry,
    AddressType *entry_base, AddressType *entry_size) const {
  MapConstIterator iterator = map_.lower_bound(address);
  if (iterator == map_.end())
    return false;

  const Range *range = iterator.GetValuePtr();

  if (address < range->base())
    return false;

  entry = range->entryptr();
  if (entry_base)
    *entry_base = range->base();
  if (entry_size)
    *entry_size = iterator.GetKey() - range->base() + 1;

  return true;
}

template<typename AddressType, typename EntryType>
bool StaticRangeMap<AddressType, EntryType>::RetrieveNearestRange(
    const AddressType &address, const EntryType *&entry,
    AddressType *entry_base, AddressType *entry_size) const {

  if (RetrieveRange(address, entry, entry_base, entry_size))
    return true;

  MapConstIterator iterator = map_.upper_bound(address);
  if (iterator == map_.begin())
    return false;
  --iterator;

  const Range *range = iterator.GetValuePtr();
  entry = range->entryptr();
  if (entry_base)
    *entry_base = range->base();
  if (entry_size)
    *entry_size = iterator.GetKey() - range->base() + 1;

  return true;
}

template<typename AddressType, typename EntryType>
bool StaticRangeMap<AddressType, EntryType>::RetrieveRangeAtIndex(
    int index, const EntryType *&entry,
    AddressType *entry_base, AddressType *entry_size) const {

  if (index >= GetCount()) {
    BPLOG(ERROR) << "Index out of range: " << index << "/" << GetCount();
    return false;
  }

  MapConstIterator iterator = map_.IteratorAtIndex(index);

  const Range *range = iterator.GetValuePtr();

  entry = range->entryptr();
  if (entry_base)
    *entry_base = range->base();
  if (entry_size)
    *entry_size = iterator.GetKey() - range->base() + 1;

  return true;
}

}

#endif
