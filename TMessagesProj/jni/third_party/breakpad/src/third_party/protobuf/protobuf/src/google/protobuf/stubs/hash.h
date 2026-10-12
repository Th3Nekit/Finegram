// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
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

// Author: kenton@google.com (Kenton Varda)
//
// Deals with the fact that hash_map is not defined everywhere.

#ifndef GOOGLE_PROTOBUF_STUBS_HASH_H__
#define GOOGLE_PROTOBUF_STUBS_HASH_H__

#include <string.h>
#include <google/protobuf/stubs/common.h>
#include "config.h"

#if defined(HAVE_HASH_MAP) && defined(HAVE_HASH_SET)
#include HASH_MAP_H
#include HASH_SET_H
#else
#define MISSING_HASH
#include <map>
#include <set>
#endif

namespace google {
namespace protobuf {

#ifdef MISSING_HASH

template <typename Key>
struct hash {

  int operator()(const Key& key) {
    GOOGLE_LOG(FATAL) << "Should never be called.";
    return 0;
  }

  inline bool operator()(const Key& a, const Key& b) const {
    return a < b;
  }
};

template <>
struct hash<const char*> {

  int operator()(const char* key) {
    GOOGLE_LOG(FATAL) << "Should never be called.";
    return 0;
  }

  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_map : public std::map<Key, Data, HashFcn> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_set : public std::set<Key, HashFcn> {
};

#elif defined(_MSC_VER) && !defined(_STLPORT_VERSION)

template <typename Key>
struct hash : public HASH_NAMESPACE::hash_compare<Key> {
};

class CstringLess {
 public:
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <>
struct hash<const char*>
  : public HASH_NAMESPACE::hash_compare<const char*, CstringLess> {
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_map : public HASH_NAMESPACE::hash_map<
    Key, Data, HashFcn> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = int >
class hash_set : public HASH_NAMESPACE::hash_set<
    Key, HashFcn> {
};

#else

template <typename Key>
struct hash : public HASH_NAMESPACE::hash<Key> {
};

template <typename Key>
struct hash<const Key*> {
  inline size_t operator()(const Key* key) const {
    return reinterpret_cast<size_t>(key);
  }
};

template <>
struct hash<const char*> {
  inline size_t operator()(const char* str) const {
    size_t result = 0;
    for (; *str != '\0'; str++) {
      result = 5 * result + *str;
    }
    return result;
  }
};

template <typename Key, typename Data,
          typename HashFcn = hash<Key>,
          typename EqualKey = std::equal_to<Key> >
class hash_map : public HASH_NAMESPACE::HASH_MAP_CLASS<
    Key, Data, HashFcn, EqualKey> {
};

template <typename Key,
          typename HashFcn = hash<Key>,
          typename EqualKey = std::equal_to<Key> >
class hash_set : public HASH_NAMESPACE::HASH_SET_CLASS<
    Key, HashFcn, EqualKey> {
};

#endif

template <>
struct hash<string> {
  inline size_t operator()(const string& key) const {
    return hash<const char*>()(key.c_str());
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const string& a, const string& b) const {
    return a < b;
  }
};

template <typename First, typename Second>
struct hash<pair<First, Second> > {
  inline size_t operator()(const pair<First, Second>& key) const {
    size_t first_hash = hash<First>()(key.first);
    size_t second_hash = hash<Second>()(key.second);

    return first_hash * ((1 << 16) - 1) + second_hash;
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const pair<First, Second>& a,
                           const pair<First, Second>& b) const {
    return a < b;
  }
};

struct streq {
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) == 0;
  }
};

}
}

#endif
