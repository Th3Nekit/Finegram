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

// contained_range_map-inl.h: Hierarchically-organized range map implementation.
//
// See contained_range_map.h for documentation.
//
// Author: Mark Mentovai

#ifndef PROCESSOR_CONTAINED_RANGE_MAP_INL_H__
#define PROCESSOR_CONTAINED_RANGE_MAP_INL_H__

#include "processor/contained_range_map.h"

#include <assert.h>

#include "processor/logging.h"

namespace google_breakpad {

template<typename AddressType, typename EntryType>
ContainedRangeMap<AddressType, EntryType>::~ContainedRangeMap() {

  Clear();
}

template<typename AddressType, typename EntryType>
bool ContainedRangeMap<AddressType, EntryType>::StoreRange(
    const AddressType &base, const AddressType &size, const EntryType &entry) {
  AddressType high = base + size - 1;

  if (size <= 0 || high < base) {

    return false;
  }

  if (!map_)
    map_ = new AddressToRangeMap();

  MapIterator iterator_base = map_->lower_bound(base);
  MapIterator iterator_high = map_->lower_bound(high);
  MapIterator iterator_end = map_->end();

  if (iterator_base == iterator_high && iterator_base != iterator_end &&
      base >= iterator_base->second->base_) {

    if (iterator_base->second->base_ == base && iterator_base->first == high) {

      return false;
    }

    return iterator_base->second->StoreRange(base, size, entry);
  }

  bool contains_high = iterator_high != iterator_end &&
                       high >= iterator_high->second->base_;

  if ((iterator_base != iterator_end && base > iterator_base->second->base_) ||
      (contains_high && high < iterator_high->first)) {

    return false;
  }

  if (contains_high)
    ++iterator_high;

  AddressToRangeMap *child_map = NULL;

  if (iterator_base != iterator_high) {

    child_map = new AddressToRangeMap(iterator_base, iterator_high);

    map_->erase(iterator_base, iterator_high);
  }

  map_->insert(MapValue(high,
                        new ContainedRangeMap(base, entry, child_map)));
  return true;
}

template<typename AddressType, typename EntryType>
bool ContainedRangeMap<AddressType, EntryType>::RetrieveRange(
    const AddressType &address, EntryType *entry) const {
  BPLOG_IF(ERROR, !entry) << "ContainedRangeMap::RetrieveRange requires "
                             "|entry|";
  assert(entry);

  if (!map_)
    return false;

  MapConstIterator iterator = map_->lower_bound(address);
  if (iterator == map_->end() || address < iterator->second->base_)
    return false;

  if (!iterator->second->RetrieveRange(address, entry))
    *entry = iterator->second->entry_;

  return true;
}

template<typename AddressType, typename EntryType>
void ContainedRangeMap<AddressType, EntryType>::Clear() {
  if (map_) {
    MapConstIterator end = map_->end();
    for (MapConstIterator child = map_->begin(); child != end; ++child)
      delete child->second;

    delete map_;
    map_ = NULL;
  }
}

}

#endif
