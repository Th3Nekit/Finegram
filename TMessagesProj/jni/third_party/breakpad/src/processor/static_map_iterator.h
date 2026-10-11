// Copyright 2010 Google Inc. All Rights Reserved.
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

// static_map_iterator.h: StaticMapIterator template class declaration.
//
// StaticMapIterator provides increment and decrement operators to iterate
// through a StaticMap map.  It does not provide *, -> operators, user should
// use GetKeyPtr(), GetKey(), GetValuePtr() interfaces to retrieve data or
// pointer to data.  StaticMapIterator is essentially a const_iterator.
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_STATIC_MAP_ITERATOR_H__
#define PROCESSOR_STATIC_MAP_ITERATOR_H__

#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

template<typename Key, typename Value, typename Compare> class StaticMap;

template<typename Key, typename Value, typename Compare>
class StaticMapIterator {
 public:

  StaticMapIterator(): index_(-1), base_(NULL) { }

  StaticMapIterator& operator++();
  StaticMapIterator operator++(int post_fix_operator);

  StaticMapIterator& operator--();
  StaticMapIterator operator--(int post_fix_operator);

  const Key* GetKeyPtr() const;

  inline const Key GetKey() const { return *GetKeyPtr(); }

  const char* GetValueRawPtr() const;

  inline const Value* GetValuePtr() const {
    return reinterpret_cast<const Value*>(GetValueRawPtr());
  }

  bool operator==(const StaticMapIterator& x) const;
  bool operator!=(const StaticMapIterator& x) const;

  bool IsValid() const;

 private:
  friend class StaticMap<Key, Value, Compare>;

  explicit StaticMapIterator(const char* base, const int32_t &index);

  int32_t index_;

  const char* base_;

  int32_t num_nodes_;

  const uint32_t* offsets_;

  const Key* keys_;
};

}

#endif
