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
//
// Author: Satoru Takabayashi
//
// For reference check out:
// http://www.codesourcery.com/public/cxx-abi/abi.html#mangling
//
// Note that we only have partial C++0x support yet.

#include <stdio.h>
#include "demangle.h"

_START_GOOGLE_NAMESPACE_

typedef struct {
  const char *abbrev;
  const char *real_name;
} AbbrevPair;

static const AbbrevPair kOperatorList[] = {
  { "nw", "new" },
  { "na", "new[]" },
  { "dl", "delete" },
  { "da", "delete[]" },
  { "ps", "+" },
  { "ng", "-" },
  { "ad", "&" },
  { "de", "*" },
  { "co", "~" },
  { "pl", "+" },
  { "mi", "-" },
  { "ml", "*" },
  { "dv", "/" },
  { "rm", "%" },
  { "an", "&" },
  { "or", "|" },
  { "eo", "^" },
  { "aS", "=" },
  { "pL", "+=" },
  { "mI", "-=" },
  { "mL", "*=" },
  { "dV", "/=" },
  { "rM", "%=" },
  { "aN", "&=" },
  { "oR", "|=" },
  { "eO", "^=" },
  { "ls", "<<" },
  { "rs", ">>" },
  { "lS", "<<=" },
  { "rS", ">>=" },
  { "eq", "==" },
  { "ne", "!=" },
  { "lt", "<" },
  { "gt", ">" },
  { "le", "<=" },
  { "ge", ">=" },
  { "nt", "!" },
  { "aa", "&&" },
  { "oo", "||" },
  { "pp", "++" },
  { "mm", "--" },
  { "cm", "," },
  { "pm", "->*" },
  { "pt", "->" },
  { "cl", "()" },
  { "ix", "[]" },
  { "qu", "?" },
  { "st", "sizeof" },
  { "sz", "sizeof" },
  { NULL, NULL },
};

static const AbbrevPair kBuiltinTypeList[] = {
  { "v", "void" },
  { "w", "wchar_t" },
  { "b", "bool" },
  { "c", "char" },
  { "a", "signed char" },
  { "h", "unsigned char" },
  { "s", "short" },
  { "t", "unsigned short" },
  { "i", "int" },
  { "j", "unsigned int" },
  { "l", "long" },
  { "m", "unsigned long" },
  { "x", "long long" },
  { "y", "unsigned long long" },
  { "n", "__int128" },
  { "o", "unsigned __int128" },
  { "f", "float" },
  { "d", "double" },
  { "e", "long double" },
  { "g", "__float128" },
  { "z", "ellipsis" },
  { NULL, NULL }
};

static const AbbrevPair kSubstitutionList[] = {
  { "St", "" },
  { "Sa", "allocator" },
  { "Sb", "basic_string" },

  { "Ss", "string"},

  { "Si", "istream" },

  { "So", "ostream" },

  { "Sd", "iostream" },
  { NULL, NULL }
};

typedef struct {
  const char *mangled_cur;
  char *out_cur;
  const char *out_begin;
  const char *out_end;
  const char *prev_name;
  int prev_name_length;
  short nest_level;
  bool append;
  bool overflowed;
} State;

static size_t StrLen(const char *str) {
  size_t len = 0;
  while (*str != '\0') {
    ++str;
    ++len;
  }
  return len;
}

static bool AtLeastNumCharsRemaining(const char *str, int n) {
  for (int i = 0; i < n; ++i) {
    if (str == '\0') {
      return false;
    }
  }
  return true;
}

static bool StrPrefix(const char *str, const char *prefix) {
  size_t i = 0;
  while (str[i] != '\0' && prefix[i] != '\0' &&
         str[i] == prefix[i]) {
    ++i;
  }
  return prefix[i] == '\0';
}

static void InitState(State *state, const char *mangled,
                      char *out, int out_size) {
  state->mangled_cur = mangled;
  state->out_cur = out;
  state->out_begin = out;
  state->out_end = out + out_size;
  state->prev_name  = NULL;
  state->prev_name_length = -1;
  state->nest_level = -1;
  state->append = true;
  state->overflowed = false;
}

static bool ParseOneCharToken(State *state, const char one_char_token) {
  if (state->mangled_cur[0] == one_char_token) {
    ++state->mangled_cur;
    return true;
  }
  return false;
}

static bool ParseTwoCharToken(State *state, const char *two_char_token) {
  if (state->mangled_cur[0] == two_char_token[0] &&
      state->mangled_cur[1] == two_char_token[1]) {
    state->mangled_cur += 2;
    return true;
  }
  return false;
}

static bool ParseCharClass(State *state, const char *char_class) {
  if (state->mangled_cur == '\0') {
    return false;
  }
  const char *p = char_class;
  for (; *p != '\0'; ++p) {
    if (state->mangled_cur[0] == *p) {
      ++state->mangled_cur;
      return true;
    }
  }
  return false;
}

static bool Optional(bool status) {
  return true;
}

typedef bool (*ParseFunc)(State *);
static bool OneOrMore(ParseFunc parse_func, State *state) {
  if (parse_func(state)) {
    while (parse_func(state)) {
    }
    return true;
  }
  return false;
}

static bool ZeroOrMore(ParseFunc parse_func, State *state) {
  while (parse_func(state)) {
  }
  return true;
}

static void Append(State *state, const char * const str, const int length) {
  int i;
  for (i = 0; i < length; ++i) {
    if (state->out_cur + 1 < state->out_end) {
      *state->out_cur = str[i];
      ++state->out_cur;
    } else {
      state->overflowed = true;
      break;
    }
  }
  if (!state->overflowed) {
    *state->out_cur = '\0';
  }
}

static bool IsLower(char c) {
  return c >= 'a' && c <= 'z';
}

static bool IsAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool IsDigit(char c) {
  return c >= '0' && c <= '9';
}

static bool IsFunctionCloneSuffix(const char *str) {
  size_t i = 0;
  while (str[i] != '\0') {

    if (str[i] != '.' || !IsAlpha(str[i + 1])) {
      return false;
    }
    i += 2;
    while (IsAlpha(str[i])) {
      ++i;
    }
    if (str[i] != '.' || !IsDigit(str[i + 1])) {
      return false;
    }
    i += 2;
    while (IsDigit(str[i])) {
      ++i;
    }
  }
  return true;
}

static void MaybeAppendWithLength(State *state, const char * const str,
                                  const int length) {
  if (state->append && length > 0) {

    if (str[0] == '<' && state->out_begin < state->out_cur  &&
        state->out_cur[-1] == '<') {
      Append(state, " ", 1);
    }

    if (IsAlpha(str[0]) || str[0] == '_') {
      state->prev_name = state->out_cur;
      state->prev_name_length = length;
    }
    Append(state, str, length);
  }
}

static bool MaybeAppend(State *state, const char * const str) {
  if (state->append) {
    int length = StrLen(str);
    MaybeAppendWithLength(state, str, length);
  }
  return true;
}

static bool EnterNestedName(State *state) {
  state->nest_level = 0;
  return true;
}

static bool LeaveNestedName(State *state, short prev_value) {
  state->nest_level = prev_value;
  return true;
}

static bool DisableAppend(State *state) {
  state->append = false;
  return true;
}

static bool RestoreAppend(State *state, bool prev_value) {
  state->append = prev_value;
  return true;
}

static void MaybeIncreaseNestLevel(State *state) {
  if (state->nest_level > -1) {
    ++state->nest_level;
  }
}

static void MaybeAppendSeparator(State *state) {
  if (state->nest_level >= 1) {
    MaybeAppend(state, "::");
  }
}

static void MaybeCancelLastSeparator(State *state) {
  if (state->nest_level >= 1 && state->append &&
      state->out_begin <= state->out_cur - 2) {
    state->out_cur -= 2;
    *state->out_cur = '\0';
  }
}

static bool IdentifierIsAnonymousNamespace(State *state, int length) {
  static const char anon_prefix[] = "_GLOBAL__N_";
  return (length > sizeof(anon_prefix) - 1 &&
          StrPrefix(state->mangled_cur, anon_prefix));
}

static bool ParseMangledName(State *state);
static bool ParseEncoding(State *state);
static bool ParseName(State *state);
static bool ParseUnscopedName(State *state);
static bool ParseUnscopedTemplateName(State *state);
static bool ParseNestedName(State *state);
static bool ParsePrefix(State *state);
static bool ParseUnqualifiedName(State *state);
static bool ParseSourceName(State *state);
static bool ParseLocalSourceName(State *state);
static bool ParseNumber(State *state, int *number_out);
static bool ParseFloatNumber(State *state);
static bool ParseSeqId(State *state);
static bool ParseIdentifier(State *state, int length);
static bool ParseOperatorName(State *state);
static bool ParseSpecialName(State *state);
static bool ParseCallOffset(State *state);
static bool ParseNVOffset(State *state);
static bool ParseVOffset(State *state);
static bool ParseCtorDtorName(State *state);
static bool ParseType(State *state);
static bool ParseCVQualifiers(State *state);
static bool ParseBuiltinType(State *state);
static bool ParseFunctionType(State *state);
static bool ParseBareFunctionType(State *state);
static bool ParseClassEnumType(State *state);
static bool ParseArrayType(State *state);
static bool ParsePointerToMemberType(State *state);
static bool ParseTemplateParam(State *state);
static bool ParseTemplateTemplateParam(State *state);
static bool ParseTemplateArgs(State *state);
static bool ParseTemplateArg(State *state);
static bool ParseExpression(State *state);
static bool ParseExprPrimary(State *state);
static bool ParseLocalName(State *state);
static bool ParseDiscriminator(State *state);
static bool ParseSubstitution(State *state);

static bool ParseMangledName(State *state) {
  return ParseTwoCharToken(state, "_Z") && ParseEncoding(state);
}

static bool ParseEncoding(State *state) {
  State copy = *state;
  if (ParseName(state) && ParseBareFunctionType(state)) {
    return true;
  }
  *state = copy;

  if (ParseName(state) || ParseSpecialName(state)) {
    return true;
  }
  return false;
}

static bool ParseName(State *state) {
  if (ParseNestedName(state) || ParseLocalName(state)) {
    return true;
  }

  State copy = *state;
  if (ParseUnscopedTemplateName(state) &&
      ParseTemplateArgs(state)) {
    return true;
  }
  *state = copy;

  if (ParseUnscopedName(state)) {
    return true;
  }
  return false;
}

static bool ParseUnscopedName(State *state) {
  if (ParseUnqualifiedName(state)) {
    return true;
  }

  State copy = *state;
  if (ParseTwoCharToken(state, "St") &&
      MaybeAppend(state, "std::") &&
      ParseUnqualifiedName(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseUnscopedTemplateName(State *state) {
  return ParseUnscopedName(state) || ParseSubstitution(state);
}

static bool ParseNestedName(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'N') &&
      EnterNestedName(state) &&
      Optional(ParseCVQualifiers(state)) &&
      ParsePrefix(state) &&
      LeaveNestedName(state, copy.nest_level) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParsePrefix(State *state) {
  bool has_something = false;
  while (true) {
    MaybeAppendSeparator(state);
    if (ParseTemplateParam(state) ||
        ParseSubstitution(state) ||
        ParseUnscopedName(state)) {
      has_something = true;
      MaybeIncreaseNestLevel(state);
      continue;
    }
    MaybeCancelLastSeparator(state);
    if (has_something && ParseTemplateArgs(state)) {
      return ParsePrefix(state);
    } else {
      break;
    }
  }
  return true;
}

static bool ParseUnqualifiedName(State *state) {
  return (ParseOperatorName(state) ||
          ParseCtorDtorName(state) ||
          ParseSourceName(state) ||
          ParseLocalSourceName(state));
}

static bool ParseSourceName(State *state) {
  State copy = *state;
  int length = -1;
  if (ParseNumber(state, &length) && ParseIdentifier(state, length)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseLocalSourceName(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'L') && ParseSourceName(state) &&
      Optional(ParseDiscriminator(state))) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseNumber(State *state, int *number_out) {
  int sign = 1;
  if (ParseOneCharToken(state, 'n')) {
    sign = -1;
  }
  const char *p = state->mangled_cur;
  int number = 0;
  for (;*p != '\0'; ++p) {
    if (IsDigit(*p)) {
      number = number * 10 + (*p - '0');
    } else {
      break;
    }
  }
  if (p != state->mangled_cur) {
    state->mangled_cur = p;
    if (number_out != NULL) {
      *number_out = number * sign;
    }
    return true;
  }
  return false;
}

static bool ParseFloatNumber(State *state) {
  const char *p = state->mangled_cur;
  for (;*p != '\0'; ++p) {
    if (!IsDigit(*p) && !(*p >= 'a' && *p <= 'f')) {
      break;
    }
  }
  if (p != state->mangled_cur) {
    state->mangled_cur = p;
    return true;
  }
  return false;
}

static bool ParseSeqId(State *state) {
  const char *p = state->mangled_cur;
  for (;*p != '\0'; ++p) {
    if (!IsDigit(*p) && !(*p >= 'A' && *p <= 'Z')) {
      break;
    }
  }
  if (p != state->mangled_cur) {
    state->mangled_cur = p;
    return true;
  }
  return false;
}

static bool ParseIdentifier(State *state, int length) {
  if (length == -1 ||
      !AtLeastNumCharsRemaining(state->mangled_cur, length)) {
    return false;
  }
  if (IdentifierIsAnonymousNamespace(state, length)) {
    MaybeAppend(state, "(anonymous namespace)");
  } else {
    MaybeAppendWithLength(state, state->mangled_cur, length);
  }
  state->mangled_cur += length;
  return true;
}

static bool ParseOperatorName(State *state) {
  if (!AtLeastNumCharsRemaining(state->mangled_cur, 2)) {
    return false;
  }

  State copy = *state;
  if (ParseTwoCharToken(state, "cv") &&
      MaybeAppend(state, "operator ") &&
      EnterNestedName(state) &&
      ParseType(state) &&
      LeaveNestedName(state, copy.nest_level)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'v') && ParseCharClass(state, "0123456789") &&
      ParseSourceName(state)) {
    return true;
  }
  *state = copy;

  if (!(IsLower(state->mangled_cur[0]) &&
        IsAlpha(state->mangled_cur[1]))) {
    return false;
  }

  const AbbrevPair *p;
  for (p = kOperatorList; p->abbrev != NULL; ++p) {
    if (state->mangled_cur[0] == p->abbrev[0] &&
        state->mangled_cur[1] == p->abbrev[1]) {
      MaybeAppend(state, "operator");
      if (IsLower(*p->real_name)) {
        MaybeAppend(state, " ");
      }
      MaybeAppend(state, p->real_name);
      state->mangled_cur += 2;
      return true;
    }
  }
  return false;
}

static bool ParseSpecialName(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'T') &&
      ParseCharClass(state, "VTIS") &&
      ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "Tc") && ParseCallOffset(state) &&
      ParseCallOffset(state) && ParseEncoding(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "GV") &&
      ParseName(state)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'T') && ParseCallOffset(state) &&
      ParseEncoding(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "TC") && ParseType(state) &&
      ParseNumber(state, NULL) && ParseOneCharToken(state, '_') &&
      DisableAppend(state) &&
      ParseType(state)) {
    RestoreAppend(state, copy.append);
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'T') && ParseCharClass(state, "FJ") &&
      ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "GR") && ParseName(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "GA") && ParseEncoding(state)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'T') && ParseCharClass(state, "hv") &&
      ParseCallOffset(state) && ParseEncoding(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseCallOffset(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'h') &&
      ParseNVOffset(state) && ParseOneCharToken(state, '_')) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'v') &&
      ParseVOffset(state) && ParseOneCharToken(state, '_')) {
    return true;
  }
  *state = copy;

  return false;
}

static bool ParseNVOffset(State *state) {
  return ParseNumber(state, NULL);
}

static bool ParseVOffset(State *state) {
  State copy = *state;
  if (ParseNumber(state, NULL) && ParseOneCharToken(state, '_') &&
      ParseNumber(state, NULL)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseCtorDtorName(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'C') &&
      ParseCharClass(state, "123")) {
    const char * const prev_name = state->prev_name;
    const int prev_name_length = state->prev_name_length;
    MaybeAppendWithLength(state, prev_name, prev_name_length);
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'D') &&
      ParseCharClass(state, "012")) {
    const char * const prev_name = state->prev_name;
    const int prev_name_length = state->prev_name_length;
    MaybeAppend(state, "~");
    MaybeAppendWithLength(state, prev_name, prev_name_length);
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseType(State *state) {

  State copy = *state;
  if (ParseCVQualifiers(state) && ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseCharClass(state, "OPRCG") && ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "Dp") && ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'D') && ParseCharClass(state, "tT") &&
      ParseExpression(state) && ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'U') && ParseSourceName(state) &&
      ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseBuiltinType(state) ||
      ParseFunctionType(state) ||
      ParseClassEnumType(state) ||
      ParseArrayType(state) ||
      ParsePointerToMemberType(state) ||
      ParseSubstitution(state)) {
    return true;
  }

  if (ParseTemplateTemplateParam(state) &&
      ParseTemplateArgs(state)) {
    return true;
  }
  *state = copy;

  if (ParseTemplateParam(state)) {
    return true;
  }

  return false;
}

static bool ParseCVQualifiers(State *state) {
  int num_cv_qualifiers = 0;
  num_cv_qualifiers += ParseOneCharToken(state, 'r');
  num_cv_qualifiers += ParseOneCharToken(state, 'V');
  num_cv_qualifiers += ParseOneCharToken(state, 'K');
  return num_cv_qualifiers > 0;
}

static bool ParseBuiltinType(State *state) {
  const AbbrevPair *p;
  for (p = kBuiltinTypeList; p->abbrev != NULL; ++p) {
    if (state->mangled_cur[0] == p->abbrev[0]) {
      MaybeAppend(state, p->real_name);
      ++state->mangled_cur;
      return true;
    }
  }

  State copy = *state;
  if (ParseOneCharToken(state, 'u') && ParseSourceName(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseFunctionType(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'F') &&
      Optional(ParseOneCharToken(state, 'Y')) &&
      ParseBareFunctionType(state) && ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseBareFunctionType(State *state) {
  State copy = *state;
  DisableAppend(state);
  if (OneOrMore(ParseType, state)) {
    RestoreAppend(state, copy.append);
    MaybeAppend(state, "()");
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseClassEnumType(State *state) {
  return ParseName(state);
}

static bool ParseArrayType(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'A') && ParseNumber(state, NULL) &&
      ParseOneCharToken(state, '_') && ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'A') && Optional(ParseExpression(state)) &&
      ParseOneCharToken(state, '_') && ParseType(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParsePointerToMemberType(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'M') && ParseType(state) &&
      ParseType(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseTemplateParam(State *state) {
  if (ParseTwoCharToken(state, "T_")) {
    MaybeAppend(state, "?");
    return true;
  }

  State copy = *state;
  if (ParseOneCharToken(state, 'T') && ParseNumber(state, NULL) &&
      ParseOneCharToken(state, '_')) {
    MaybeAppend(state, "?");
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseTemplateTemplateParam(State *state) {
  return (ParseTemplateParam(state) ||
          ParseSubstitution(state));
}

static bool ParseTemplateArgs(State *state) {
  State copy = *state;
  DisableAppend(state);
  if (ParseOneCharToken(state, 'I') &&
      OneOrMore(ParseTemplateArg, state) &&
      ParseOneCharToken(state, 'E')) {
    RestoreAppend(state, copy.append);
    MaybeAppend(state, "<>");
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseTemplateArg(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'I') &&
      ZeroOrMore(ParseTemplateArg, state) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  if (ParseType(state) ||
      ParseExprPrimary(state)) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'X') && ParseExpression(state) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseExpression(State *state) {
  if (ParseTemplateParam(state) || ParseExprPrimary(state)) {
    return true;
  }

  State copy = *state;
  if (ParseOperatorName(state) &&
      ParseExpression(state) &&
      ParseExpression(state) &&
      ParseExpression(state)) {
    return true;
  }
  *state = copy;

  if (ParseOperatorName(state) &&
      ParseExpression(state) &&
      ParseExpression(state)) {
    return true;
  }
  *state = copy;

  if (ParseOperatorName(state) &&
      ParseExpression(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "st") && ParseType(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "sr") && ParseType(state) &&
      ParseUnqualifiedName(state) &&
      ParseTemplateArgs(state)) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "sr") && ParseType(state) &&
      ParseUnqualifiedName(state)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseExprPrimary(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'L') && ParseType(state) &&
      ParseNumber(state, NULL) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'L') && ParseType(state) &&
      ParseFloatNumber(state) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'L') && ParseMangledName(state) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  if (ParseTwoCharToken(state, "LZ") && ParseEncoding(state) &&
      ParseOneCharToken(state, 'E')) {
    return true;
  }
  *state = copy;

  return false;
}

static bool ParseLocalName(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, 'Z') && ParseEncoding(state) &&
      ParseOneCharToken(state, 'E') && MaybeAppend(state, "::") &&
      ParseName(state) && Optional(ParseDiscriminator(state))) {
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'Z') && ParseEncoding(state) &&
      ParseTwoCharToken(state, "Es") && Optional(ParseDiscriminator(state))) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseDiscriminator(State *state) {
  State copy = *state;
  if (ParseOneCharToken(state, '_') && ParseNumber(state, NULL)) {
    return true;
  }
  *state = copy;
  return false;
}

static bool ParseSubstitution(State *state) {
  if (ParseTwoCharToken(state, "S_")) {
    MaybeAppend(state, "?");
    return true;
  }

  State copy = *state;
  if (ParseOneCharToken(state, 'S') && ParseSeqId(state) &&
      ParseOneCharToken(state, '_')) {
    MaybeAppend(state, "?");
    return true;
  }
  *state = copy;

  if (ParseOneCharToken(state, 'S')) {
    const AbbrevPair *p;
    for (p = kSubstitutionList; p->abbrev != NULL; ++p) {
      if (state->mangled_cur[0] == p->abbrev[1]) {
        MaybeAppend(state, "std");
        if (p->real_name[0] != '\0') {
          MaybeAppend(state, "::");
          MaybeAppend(state, p->real_name);
        }
        ++state->mangled_cur;
        return true;
      }
    }
  }
  *state = copy;
  return false;
}

static bool ParseTopLevelMangledName(State *state) {
  if (ParseMangledName(state)) {
    if (state->mangled_cur[0] != '\0') {

      if (IsFunctionCloneSuffix(state->mangled_cur)) {
        return true;
      }

      if (state->mangled_cur[0] == '@') {
        MaybeAppend(state, state->mangled_cur);
        return true;
      }
      return false;
    }
    return true;
  }
  return false;
}

bool Demangle(const char *mangled, char *out, int out_size) {
  State state;
  InitState(&state, mangled, out, out_size);
  return ParseTopLevelMangledName(&state) && !state.overflowed;
}

_END_GOOGLE_NAMESPACE_
