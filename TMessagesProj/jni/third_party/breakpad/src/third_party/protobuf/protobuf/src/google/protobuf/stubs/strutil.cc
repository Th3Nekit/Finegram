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

// from google3/strings/strutil.cc

#include <google/protobuf/stubs/strutil.h>
#include <errno.h>
#include <float.h>
#include <limits>
#include <limits.h>
#include <stdio.h>
#include <iterator>

#ifdef _WIN32

#define snprintf _snprintf
#endif

namespace google {
namespace protobuf {

inline bool IsNaN(double value) {

  return value != value;
}

#undef isxdigit
#undef isprint

inline bool isxdigit(char c) {
  return ('0' <= c && c <= '9') ||
         ('a' <= c && c <= 'f') ||
         ('A' <= c && c <= 'F');
}

inline bool isprint(char c) {
  return c >= 0x20 && c <= 0x7E;
}

void StripString(string* s, const char* remove, char replacewith) {
  const char * str_start = s->c_str();
  const char * str = str_start;
  for (str = strpbrk(str, remove);
       str != NULL;
       str = strpbrk(str + 1, remove)) {
    (*s)[str - str_start] = replacewith;
  }
}

void StringReplace(const string& s, const string& oldsub,
                   const string& newsub, bool replace_all,
                   string* res) {
  if (oldsub.empty()) {
    res->append(s);
    return;
  }

  string::size_type start_pos = 0;
  string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == string::npos) {
      break;
    }
    res->append(s, start_pos, pos - start_pos);
    res->append(newsub);
    start_pos = pos + oldsub.size();
  } while (replace_all);
  res->append(s, start_pos, s.length() - start_pos);
}

string StringReplace(const string& s, const string& oldsub,
                     const string& newsub, bool replace_all) {
  string ret;
  StringReplace(s, oldsub, newsub, replace_all, &ret);
  return ret;
}

template <typename ITR>
static inline
void SplitStringToIteratorUsing(const string& full,
                                const char* delim,
                                ITR& result) {

  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    const char* p = full.data();
    const char* end = p + full.size();
    while (p != end) {
      if (*p == c) {
        ++p;
      } else {
        const char* start = p;
        while (++p != end && *p != c);
        *result++ = string(start, p - start);
      }
    }
    return;
  }

  string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

void SplitStringUsing(const string& full,
                      const char* delim,
                      vector<string>* result) {
  back_insert_iterator< vector<string> > it(*result);
  SplitStringToIteratorUsing(full, delim, it);
}

template <class ITERATOR>
static void JoinStringsIterator(const ITERATOR& start,
                                const ITERATOR& end,
                                const char* delim,
                                string* result) {
  GOOGLE_CHECK(result != NULL);
  result->clear();
  int delim_length = strlen(delim);

  int length = 0;
  for (ITERATOR iter = start; iter != end; ++iter) {
    if (iter != start) {
      length += delim_length;
    }
    length += iter->size();
  }
  result->reserve(length);

  for (ITERATOR iter = start; iter != end; ++iter) {
    if (iter != start) {
      result->append(delim, delim_length);
    }
    result->append(iter->data(), iter->size());
  }
}

void JoinStrings(const vector<string>& components,
                 const char* delim,
                 string * result) {
  JoinStringsIterator(components.begin(), components.end(), delim, result);
}

#define IS_OCTAL_DIGIT(c) (((c) >= '0') && ((c) <= '7'))

inline int hex_digit_to_int(char c) {

  assert('0' == 0x30 && 'A' == 0x41 && 'a' == 0x61);
  assert(isxdigit(c));
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

#define LOG_STRING(LEVEL, VECTOR) GOOGLE_LOG_IF(LEVEL, false)

int UnescapeCEscapeSequences(const char* source, char* dest) {
  return UnescapeCEscapeSequences(source, dest, NULL);
}

int UnescapeCEscapeSequences(const char* source, char* dest,
                             vector<string> *errors) {
  GOOGLE_DCHECK(errors == NULL) << "Error reporting not implemented.";

  char* d = dest;
  const char* p = source;

  while ( p == d && *p != '\0' && *p != '\\' )
    p++, d++;

  while (*p != '\0') {
    if (*p != '\\') {
      *d++ = *p++;
    } else {
      switch ( *++p ) {
        case '\0':
          LOG_STRING(ERROR, errors) << "String cannot end with \\";
          *d = '\0';
          return d - dest;
        case 'a':  *d++ = '\a';  break;
        case 'b':  *d++ = '\b';  break;
        case 'f':  *d++ = '\f';  break;
        case 'n':  *d++ = '\n';  break;
        case 'r':  *d++ = '\r';  break;
        case 't':  *d++ = '\t';  break;
        case 'v':  *d++ = '\v';  break;
        case '\\': *d++ = '\\';  break;
        case '?':  *d++ = '\?';  break;
        case '\'': *d++ = '\'';  break;
        case '"':  *d++ = '\"';  break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': {
          char ch = *p - '0';
          if ( IS_OCTAL_DIGIT(p[1]) )
            ch = ch * 8 + *++p - '0';
          if ( IS_OCTAL_DIGIT(p[1]) )
            ch = ch * 8 + *++p - '0';
          *d++ = ch;
          break;
        }
        case 'x': case 'X': {
          if (!isxdigit(p[1])) {
            if (p[1] == '\0') {
              LOG_STRING(ERROR, errors) << "String cannot end with \\x";
            } else {
              LOG_STRING(ERROR, errors) <<
                "\\x cannot be followed by non-hex digit: \\" << *p << p[1];
            }
            break;
          }
          unsigned int ch = 0;
          const char *hex_start = p;
          while (isxdigit(p[1]))
            ch = (ch << 4) + hex_digit_to_int(*++p);
          if (ch > 0xFF)
            LOG_STRING(ERROR, errors) << "Value of " <<
              "\\" << string(hex_start, p+1-hex_start) << " exceeds 8 bits";
          *d++ = ch;
          break;
        }
#if 0
        case 'u': {

          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 4; ++i) {
            if (isxdigit(p[1])) {
              rune = (rune << 4) + hex_digit_to_int(*++p);
            } else {
              LOG_STRING(ERROR, errors)
                << "\\u must be followed by 4 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
        case 'U': {

          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 8; ++i) {
            if (isxdigit(p[1])) {

              char32 newrune = (rune << 4) + hex_digit_to_int(*++p);
              if (newrune > 0x10FFFF) {
                LOG_STRING(ERROR, errors)
                  << "Value of \\"
                  << string(hex_start, p + 1 - hex_start)
                  << " exceeds Unicode limit (0x10FFFF)";
                break;
              } else {
                rune = newrune;
              }
            } else {
              LOG_STRING(ERROR, errors)
                << "\\U must be followed by 8 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
#endif
        default:
          LOG_STRING(ERROR, errors) << "Unknown escape sequence: \\" << *p;
      }
      p++;
    }
  }
  *d = '\0';
  return d - dest;
}

int UnescapeCEscapeString(const string& src, string* dest) {
  return UnescapeCEscapeString(src, dest, NULL);
}

int UnescapeCEscapeString(const string& src, string* dest,
                          vector<string> *errors) {
  scoped_array<char> unescaped(new char[src.size() + 1]);
  int len = UnescapeCEscapeSequences(src.c_str(), unescaped.get(), errors);
  GOOGLE_CHECK(dest);
  dest->assign(unescaped.get(), len);
  return len;
}

string UnescapeCEscapeString(const string& src) {
  scoped_array<char> unescaped(new char[src.size() + 1]);
  int len = UnescapeCEscapeSequences(src.c_str(), unescaped.get(), NULL);
  return string(unescaped.get(), len);
}

int CEscapeInternal(const char* src, int src_len, char* dest,
                    int dest_len, bool use_hex, bool utf8_safe) {
  const char* src_end = src + src_len;
  int used = 0;
  bool last_hex_escape = false;

  for (; src < src_end; src++) {
    if (dest_len - used < 2)
      return -1;

    bool is_hex_escape = false;
    switch (*src) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n';  break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r';  break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't';  break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default:

        if ((!utf8_safe || static_cast<uint8>(*src) < 0x80) &&
            (!isprint(*src) ||
             (last_hex_escape && isxdigit(*src)))) {
          if (dest_len - used < 4)
            return -1;
          sprintf(dest + used, (use_hex ? "\\x%02x" : "\\%03o"),
                  static_cast<uint8>(*src));
          is_hex_escape = use_hex;
          used += 4;
        } else {
          dest[used++] = *src; break;
        }
    }
    last_hex_escape = is_hex_escape;
  }

  if (dest_len - used < 1)
    return -1;

  dest[used] = '\0';
  return used;
}

int CEscapeString(const char* src, int src_len, char* dest, int dest_len) {
  return CEscapeInternal(src, src_len, dest, dest_len, false, false);
}

string CEscape(const string& src) {
  const int dest_length = src.size() * 4 + 1;
  scoped_array<char> dest(new char[dest_length]);
  const int len = CEscapeInternal(src.data(), src.size(),
                                  dest.get(), dest_length, false, false);
  GOOGLE_DCHECK_GE(len, 0);
  return string(dest.get(), len);
}

namespace strings {

string Utf8SafeCEscape(const string& src) {
  const int dest_length = src.size() * 4 + 1;
  scoped_array<char> dest(new char[dest_length]);
  const int len = CEscapeInternal(src.data(), src.size(),
                                  dest.get(), dest_length, false, true);
  GOOGLE_DCHECK_GE(len, 0);
  return string(dest.get(), len);
}

string CHexEscape(const string& src) {
  const int dest_length = src.size() * 4 + 1;
  scoped_array<char> dest(new char[dest_length]);
  const int len = CEscapeInternal(src.data(), src.size(),
                                  dest.get(), dest_length, true, false);
  GOOGLE_DCHECK_GE(len, 0);
  return string(dest.get(), len);
}

}

int32 strto32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const long result = strtol(nptr, endptr, base);
  if (errno == ERANGE && result == LONG_MIN) {
    return kint32min;
  } else if (errno == ERANGE && result == LONG_MAX) {
    return kint32max;
  } else if (errno == 0 && result < kint32min) {
    errno = ERANGE;
    return kint32min;
  } else if (errno == 0 && result > kint32max) {
    errno = ERANGE;
    return kint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<int32>(result);
}

uint32 strtou32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const unsigned long result = strtoul(nptr, endptr, base);
  if (errno == ERANGE && result == ULONG_MAX) {
    return kuint32max;
  } else if (errno == 0 && result > kuint32max) {
    errno = ERANGE;
    return kuint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<uint32>(result);
}

static const int kFastInt64ToBufferOffset = 21;

char *FastInt64ToBuffer(int64 i, char* buffer) {

  char* p = buffer + kFastInt64ToBufferOffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {

    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {

      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;

      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

static const int kFastInt32ToBufferOffset = 11;

char *FastInt32ToBuffer(int32 i, char* buffer) {

  char* p = buffer + kFastInt32ToBufferOffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {

    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {

      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;

      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

char *FastHexToBuffer(int i, char* buffer) {
  GOOGLE_CHECK(i >= 0) << "FastHexToBuffer() wants non-negative integers, not " << i;

  static const char *hexdigits = "0123456789abcdef";
  char *p = buffer + 21;
  *p-- = '\0';
  do {
    *p-- = hexdigits[i & 15];
    i >>= 4;
  } while (i > 0);
  return p + 1;
}

char *InternalFastHexToBuffer(uint64 value, char* buffer, int num_byte) {
  static const char *hexdigits = "0123456789abcdef";
  buffer[num_byte] = '\0';
  for (int i = num_byte - 1; i >= 0; i--) {
#ifdef _M_X64

    buffer[i] = hexdigits[value & 0xf];
#else
    buffer[i] = hexdigits[uint32(value) & 0xf];
#endif
    value >>= 4;
  }
  return buffer;
}

char *FastHex64ToBuffer(uint64 value, char* buffer) {
  return InternalFastHexToBuffer(value, buffer, 16);
}

char *FastHex32ToBuffer(uint32 value, char* buffer) {
  return InternalFastHexToBuffer(value, buffer, 8);
}

static inline char* PlaceNum(char* p, int num, char prev_sep) {
   *p-- = '0' + num % 10;
   *p-- = '0' + num / 10;
   *p-- = prev_sep;
   return p;
}

static const char two_ASCII_digits[100][2] = {
  {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'},
  {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
  {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'},
  {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
  {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'},
  {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
  {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'},
  {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
  {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'},
  {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
  {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'},
  {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
  {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'},
  {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
  {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'},
  {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
  {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'},
  {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
  {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'},
  {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
};

char* FastUInt32ToBufferLeft(uint32 u, char* buffer) {
  int digits;
  const char *ASCII_digits = NULL;

  if (u >= 1000000000) {
    digits = u / 100000000;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt100_000_000:
    u -= digits * 100000000;
lt100_000_000:
    digits = u / 1000000;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt1_000_000:
    u -= digits * 1000000;
lt1_000_000:
    digits = u / 10000;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt10_000:
    u -= digits * 10000;
lt10_000:
    digits = u / 100;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt100:
    u -= digits * 100;
lt100:
    digits = u;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
done:
    *buffer = 0;
    return buffer;
  }

  if (u < 100) {
    digits = u;
    if (u >= 10) goto lt100;
    *buffer++ = '0' + digits;
    goto done;
  }
  if (u  <  10000) {
    if (u >= 1000) goto lt10_000;
    digits = u / 100;
    *buffer++ = '0' + digits;
    goto sublt100;
  }
  if (u  <  1000000) {
    if (u >= 100000) goto lt1_000_000;
    digits = u / 10000;
    *buffer++ = '0' + digits;
    goto sublt10_000;
  }
  if (u  <  100000000) {
    if (u >= 10000000) goto lt100_000_000;
    digits = u / 1000000;
    *buffer++ = '0' + digits;
    goto sublt1_000_000;
  }

  digits = u / 100000000;
  *buffer++ = '0' + digits;
  goto sublt100_000_000;
}

char* FastInt32ToBufferLeft(int32 i, char* buffer) {
  uint32 u = i;
  if (i < 0) {
    *buffer++ = '-';
    u = -i;
  }
  return FastUInt32ToBufferLeft(u, buffer);
}

char* FastUInt64ToBufferLeft(uint64 u64, char* buffer) {
  int digits;
  const char *ASCII_digits = NULL;

  uint32 u = static_cast<uint32>(u64);
  if (u == u64) return FastUInt32ToBufferLeft(u, buffer);

  uint64 top_11_digits = u64 / 1000000000;
  buffer = FastUInt64ToBufferLeft(top_11_digits, buffer);
  u = u64 - (top_11_digits * 1000000000);

  digits = u / 10000000;
  GOOGLE_DCHECK_LT(digits, 100);
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 10000000;
  digits = u / 100000;
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 100000;
  digits = u / 1000;
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 1000;
  digits = u / 10;
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 10;
  digits = u;
  *buffer++ = '0' + digits;
  *buffer = 0;
  return buffer;
}

char* FastInt64ToBufferLeft(int64 i, char* buffer) {
  uint64 u = i;
  if (i < 0) {
    *buffer++ = '-';
    u = -i;
  }
  return FastUInt64ToBufferLeft(u, buffer);
}

string SimpleItoa(int i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned int i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

string SimpleItoa(long i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned long i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

string SimpleItoa(long long i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned long long i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

string SimpleDtoa(double value) {
  char buffer[kDoubleToBufferSize];
  return DoubleToBuffer(value, buffer);
}

string SimpleFtoa(float value) {
  char buffer[kFloatToBufferSize];
  return FloatToBuffer(value, buffer);
}

static inline bool IsValidFloatChar(char c) {
  return ('0' <= c && c <= '9') ||
         c == 'e' || c == 'E' ||
         c == '+' || c == '-';
}

void DelocalizeRadix(char* buffer) {

  if (strchr(buffer, '.') != NULL) return;

  while (IsValidFloatChar(*buffer)) ++buffer;

  if (*buffer == '\0') {

    return;
  }

  *buffer = '.';
  ++buffer;

  if (!IsValidFloatChar(*buffer) && *buffer != '\0') {

    char* target = buffer;
    do { ++buffer; } while (!IsValidFloatChar(*buffer) && *buffer != '\0');
    memmove(target, buffer, strlen(buffer) + 1);
  }
}

char* DoubleToBuffer(double value, char* buffer) {

  GOOGLE_COMPILE_ASSERT(DBL_DIG < 20, DBL_DIG_is_too_big);

  if (value == numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (IsNaN(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kDoubleToBufferSize, "%.*g", DBL_DIG, value);

  GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kDoubleToBufferSize);

  volatile double parsed_value = strtod(buffer, NULL);
  if (parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kDoubleToBufferSize, "%.*g", DBL_DIG+2, value);

    GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kDoubleToBufferSize);
  }

  DelocalizeRadix(buffer);
  return buffer;
}

bool safe_strtof(const char* str, float* value) {
  char* endptr;
  errno = 0;
#if defined(_WIN32) || defined (__hpux)
  *value = strtod(str, &endptr);
#else
  *value = strtof(str, &endptr);
#endif
  return *str != 0 && *endptr == 0 && errno == 0;
}

char* FloatToBuffer(float value, char* buffer) {

  GOOGLE_COMPILE_ASSERT(FLT_DIG < 10, FLT_DIG_is_too_big);

  if (value == numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (IsNaN(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kFloatToBufferSize, "%.*g", FLT_DIG, value);

  GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kFloatToBufferSize);

  float parsed_value;
  if (!safe_strtof(buffer, &parsed_value) || parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kFloatToBufferSize, "%.*g", FLT_DIG+2, value);

    GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kFloatToBufferSize);
  }

  DelocalizeRadix(buffer);
  return buffer;
}

string LocalizeRadix(const char* input, const char* radix_pos) {

  char temp[16];
  int size = sprintf(temp, "%.1f", 1.5);
  GOOGLE_CHECK_EQ(temp[0], '1');
  GOOGLE_CHECK_EQ(temp[size-1], '5');
  GOOGLE_CHECK_LE(size, 6);

  string result;
  result.reserve(strlen(input) + size - 3);
  result.append(input, radix_pos);
  result.append(temp + 1, size - 2);
  result.append(radix_pos + 1);
  return result;
}

double NoLocaleStrtod(const char* text, char** original_endptr) {

  char* temp_endptr;
  double result = strtod(text, &temp_endptr);
  if (original_endptr != NULL) *original_endptr = temp_endptr;
  if (*temp_endptr != '.') return result;

  string localized = LocalizeRadix(text, temp_endptr);
  const char* localized_cstr = localized.c_str();
  char* localized_endptr;
  result = strtod(localized_cstr, &localized_endptr);
  if ((localized_endptr - localized_cstr) >
      (temp_endptr - text)) {

    if (original_endptr != NULL) {

      int size_diff = localized.size() - strlen(text);

      *original_endptr = const_cast<char*>(
        text + (localized_endptr - localized_cstr - size_diff));
    }
  }

  return result;
}

}
}
