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

// from google3/strings/strutil.h

#ifndef GOOGLE_PROTOBUF_STUBS_STRUTIL_H__
#define GOOGLE_PROTOBUF_STUBS_STRUTIL_H__

#include <stdlib.h>
#include <vector>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

#ifdef _MSC_VER
#define strtoll  _strtoi64
#define strtoull _strtoui64
#elif defined(__DECCXX) && defined(__osf__)

#define strtoll strtol
#define strtoull strtoul
#endif

inline bool ascii_isalnum(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9');
}

inline bool ascii_isdigit(char c) {
  return ('0' <= c && c <= '9');
}

inline bool HasPrefixString(const string& str,
                            const string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

inline string StripPrefixString(const string& str, const string& prefix) {
  if (HasPrefixString(str, prefix)) {
    return str.substr(prefix.size());
  } else {
    return str;
  }
}

inline bool HasSuffixString(const string& str,
                            const string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline string StripSuffixString(const string& str, const string& suffix) {
  if (HasSuffixString(str, suffix)) {
    return str.substr(0, str.size() - suffix.size());
  } else {
    return str;
  }
}

LIBPROTOBUF_EXPORT void StripString(string* s, const char* remove,
                                    char replacewith);

inline void LowerString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {

    if ('A' <= *i && *i <= 'Z') *i += 'a' - 'A';
  }
}

inline void UpperString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {

    if ('a' <= *i && *i <= 'z') *i += 'A' - 'a';
  }
}

LIBPROTOBUF_EXPORT string StringReplace(const string& s, const string& oldsub,
                                        const string& newsub, bool replace_all);

LIBPROTOBUF_EXPORT void SplitStringUsing(const string& full, const char* delim,
                                         vector<string>* res);

LIBPROTOBUF_EXPORT void JoinStrings(const vector<string>& components,
                                    const char* delim, string* result);

inline string JoinStrings(const vector<string>& components,
                          const char* delim) {
  string result;
  JoinStrings(components, delim, &result);
  return result;
}

LIBPROTOBUF_EXPORT int UnescapeCEscapeSequences(const char* source, char* dest);
LIBPROTOBUF_EXPORT int UnescapeCEscapeSequences(const char* source, char* dest,
                                                vector<string> *errors);

LIBPROTOBUF_EXPORT int UnescapeCEscapeString(const string& src, string* dest);
LIBPROTOBUF_EXPORT int UnescapeCEscapeString(const string& src, string* dest,
                                             vector<string> *errors);
LIBPROTOBUF_EXPORT string UnescapeCEscapeString(const string& src);

LIBPROTOBUF_EXPORT int CEscapeString(const char* src, int src_len,
                                     char* dest, int dest_len);

LIBPROTOBUF_EXPORT string CEscape(const string& src);

namespace strings {

LIBPROTOBUF_EXPORT string Utf8SafeCEscape(const string& src);

LIBPROTOBUF_EXPORT string CHexEscape(const string& src);
}

LIBPROTOBUF_EXPORT int32 strto32_adaptor(const char *nptr, char **endptr,
                                         int base);
LIBPROTOBUF_EXPORT uint32 strtou32_adaptor(const char *nptr, char **endptr,
                                           int base);

inline int32 strto32(const char *nptr, char **endptr, int base) {
  if (sizeof(int32) == sizeof(long))
    return strtol(nptr, endptr, base);
  else
    return strto32_adaptor(nptr, endptr, base);
}

inline uint32 strtou32(const char *nptr, char **endptr, int base) {
  if (sizeof(uint32) == sizeof(unsigned long))
    return strtoul(nptr, endptr, base);
  else
    return strtou32_adaptor(nptr, endptr, base);
}

inline int64 strto64(const char *nptr, char **endptr, int base) {
  GOOGLE_COMPILE_ASSERT(sizeof(int64) == sizeof(long long),
                        sizeof_int64_is_not_sizeof_long_long);
  return strtoll(nptr, endptr, base);
}

inline uint64 strtou64(const char *nptr, char **endptr, int base) {
  GOOGLE_COMPILE_ASSERT(sizeof(uint64) == sizeof(unsigned long long),
                        sizeof_uint64_is_not_sizeof_long_long);
  return strtoull(nptr, endptr, base);
}

static const int kFastToBufferSize = 32;

LIBPROTOBUF_EXPORT char* FastInt32ToBuffer(int32 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastInt64ToBuffer(int64 i, char* buffer);
char* FastUInt32ToBuffer(uint32 i, char* buffer);
char* FastUInt64ToBuffer(uint64 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastHexToBuffer(int i, char* buffer);
LIBPROTOBUF_EXPORT char* FastHex64ToBuffer(uint64 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastHex32ToBuffer(uint32 i, char* buffer);

inline char* FastIntToBuffer(int i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastInt32ToBuffer(i, buffer) : FastInt64ToBuffer(i, buffer));
}
inline char* FastUIntToBuffer(unsigned int i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastUInt32ToBuffer(i, buffer) : FastUInt64ToBuffer(i, buffer));
}
inline char* FastLongToBuffer(long i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastInt32ToBuffer(i, buffer) : FastInt64ToBuffer(i, buffer));
}
inline char* FastULongToBuffer(unsigned long i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastUInt32ToBuffer(i, buffer) : FastUInt64ToBuffer(i, buffer));
}

LIBPROTOBUF_EXPORT char* FastInt32ToBufferLeft(int32 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastUInt32ToBufferLeft(uint32 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastInt64ToBufferLeft(int64 i, char* buffer);
LIBPROTOBUF_EXPORT char* FastUInt64ToBufferLeft(uint64 i, char* buffer);

inline char* FastUInt32ToBuffer(uint32 i, char* buffer) {
  FastUInt32ToBufferLeft(i, buffer);
  return buffer;
}
inline char* FastUInt64ToBuffer(uint64 i, char* buffer) {
  FastUInt64ToBufferLeft(i, buffer);
  return buffer;
}

LIBPROTOBUF_EXPORT string SimpleItoa(int i);
LIBPROTOBUF_EXPORT string SimpleItoa(unsigned int i);
LIBPROTOBUF_EXPORT string SimpleItoa(long i);
LIBPROTOBUF_EXPORT string SimpleItoa(unsigned long i);
LIBPROTOBUF_EXPORT string SimpleItoa(long long i);
LIBPROTOBUF_EXPORT string SimpleItoa(unsigned long long i);

LIBPROTOBUF_EXPORT string SimpleDtoa(double value);
LIBPROTOBUF_EXPORT string SimpleFtoa(float value);

LIBPROTOBUF_EXPORT char* DoubleToBuffer(double i, char* buffer);
LIBPROTOBUF_EXPORT char* FloatToBuffer(float i, char* buffer);

static const int kDoubleToBufferSize = 32;
static const int kFloatToBufferSize = 24;

LIBPROTOBUF_EXPORT double NoLocaleStrtod(const char* text, char** endptr);

}
}

#endif
