// Copyright (c) 2007, Google Inc.
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

#ifndef COMMON_SIMPLE_STRING_DICTIONARY_H_
#define COMMON_SIMPLE_STRING_DICTIONARY_H_

#include <assert.h>
#include <string.h>

#include "common/basictypes.h"

namespace google_breakpad {

struct SerializedNonAllocatingMap;

template <size_t KeySize, size_t ValueSize, size_t NumEntries>
class NonAllocatingMap {
 public:

  static const size_t key_size = KeySize;
  static const size_t value_size = ValueSize;
  static const size_t num_entries = NumEntries;

  struct Entry {
    char key[KeySize];
    char value[ValueSize];

    bool is_active() const {
      return key[0] != '\0';
    }
  };

  class Iterator {
   public:
    explicit Iterator(const NonAllocatingMap& map)
        : map_(map),
          current_(0) {
    }

    const Entry* Next() {
      while (current_ < map_.num_entries) {
        const Entry* entry = &map_.entries_[current_++];
        if (entry->is_active()) {
          return entry;
        }
      }
      return NULL;
    }

   private:
    const NonAllocatingMap& map_;
    size_t current_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  NonAllocatingMap() : entries_() {
  }

  NonAllocatingMap(const NonAllocatingMap& other) {
    *this = other;
  }

  NonAllocatingMap& operator=(const NonAllocatingMap& other) {
    assert(other.key_size == key_size);
    assert(other.value_size == value_size);
    assert(other.num_entries == num_entries);
    if (other.key_size == key_size && other.value_size == value_size &&
        other.num_entries == num_entries) {
      memcpy(entries_, other.entries_, sizeof(entries_));
    }
    return *this;
  }

  NonAllocatingMap(const SerializedNonAllocatingMap* map, size_t size) {
    assert(size == sizeof(entries_));
    if (size == sizeof(entries_)) {
      memcpy(entries_, map, size);
    }
  }

  size_t GetCount() const {
    size_t count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (entries_[i].is_active()) {
        ++count;
      }
    }
    return count;
  }

  const char* GetValueForKey(const char* key) const {
    assert(key);
    if (!key)
      return NULL;

    const Entry* entry = GetConstEntryForKey(key);
    if (!entry)
      return NULL;

    return entry->value;
  }

  void SetKeyValue(const char* key, const char* value) {
    if (!value) {
      RemoveKey(key);
      return;
    }

    assert(key);
    if (!key)
      return;

    assert(key[0] != '\0');
    if (key[0] == '\0')
      return;

    Entry* entry = GetEntryForKey(key);

    if (!entry) {
      for (size_t i = 0; i < num_entries; ++i) {
        if (!entries_[i].is_active()) {
          entry = &entries_[i];

          strncpy(entry->key, key, key_size);
          entry->key[key_size - 1] = '\0';

          break;
        }
      }
    }

    if (!entry)
      return;

#ifndef NDEBUG

    int count = 0;
    for (size_t i = 0; i < num_entries; ++i) {
      if (strncmp(entries_[i].key, key, key_size) == 0)
        ++count;
    }
    assert(count == 1);
#endif

    strncpy(entry->value, value, value_size);
    entry->value[value_size - 1] = '\0';
  }

  void RemoveKey(const char* key) {
    assert(key);
    if (!key)
      return;

    Entry* entry = GetEntryForKey(key);
    if (entry) {
      entry->key[0] = '\0';
      entry->value[0] = '\0';
    }

#ifndef NDEBUG
    assert(GetEntryForKey(key) == NULL);
#endif
  }

  size_t Serialize(const SerializedNonAllocatingMap** map) const {
    *map = reinterpret_cast<const SerializedNonAllocatingMap*>(entries_);
    return sizeof(entries_);
  }

 private:
  const Entry* GetConstEntryForKey(const char* key) const {
    for (size_t i = 0; i < num_entries; ++i) {
      if (strncmp(key, entries_[i].key, key_size) == 0) {
        return &entries_[i];
      }
    }
    return NULL;
  }

  Entry* GetEntryForKey(const char* key) {
    return const_cast<Entry*>(GetConstEntryForKey(key));
  }

  Entry entries_[NumEntries];
};

typedef NonAllocatingMap<256, 256, 64> SimpleStringDictionary;

}

#endif
