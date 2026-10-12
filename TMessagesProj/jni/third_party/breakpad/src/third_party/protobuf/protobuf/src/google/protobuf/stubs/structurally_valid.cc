// Copyright 2005-2008 Google Inc. All Rights Reserved.
// Author: jrm@google.com (Jim Meehan)

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace internal {

struct RemapEntry {
  uint8 delete_bytes;
  uint8 add_bytes;
  uint16 bytes_offset;
};

typedef enum {
  kExitDstSpaceFull = 239,
  kExitIllegalStructure,
  kExitOK,
  kExitReject,
  kExitReplace1,
  kExitReplace2,
  kExitReplace3,
  kExitReplace21,
  kExitReplace31,
  kExitReplace32,
  kExitReplaceOffset1,
  kExitReplaceOffset2,
  kExitReplace1S0,
  kExitSpecial,
  kExitDoAgain,
  kExitRejectAlt,
  kExitNone
} ExitReason;

typedef struct {
  const uint32 state0;
  const uint32 state0_size;
  const uint32 total_size;
  const int max_expand;
  const int entry_shift;
  const int bytes_per_entry;
  const uint32 losub;
  const uint32 hiadd;
  const uint8* state_table;
  const RemapEntry* remap_base;
  const uint8* remap_string;
  const uint8* fast_state;
} UTF8StateMachineObj;

typedef UTF8StateMachineObj UTF8ScanObj;

#define X__ (kExitIllegalStructure)
#define RJ_ (kExitReject)
#define S1_ (kExitReplace1)
#define S2_ (kExitReplace2)
#define S3_ (kExitReplace3)
#define S21 (kExitReplace21)
#define S31 (kExitReplace31)
#define S32 (kExitReplace32)
#define T1_ (kExitReplaceOffset1)
#define T2_ (kExitReplaceOffset2)
#define S11 (kExitReplace1S0)
#define SP_ (kExitSpecial)
#define D__ (kExitDoAgain)
#define RJA (kExitRejectAlt)

static const unsigned int utf8acceptnonsurrogates_STATE0 = 0;
static const unsigned int utf8acceptnonsurrogates_STATE0_SIZE = 256;
static const unsigned int utf8acceptnonsurrogates_TOTAL_SIZE = 2304;
static const unsigned int utf8acceptnonsurrogates_MAX_EXPAND_X4 = 0;
static const unsigned int utf8acceptnonsurrogates_SHIFT = 8;
static const unsigned int utf8acceptnonsurrogates_BYTES = 1;
static const unsigned int utf8acceptnonsurrogates_LOSUB = 0x20202020;
static const unsigned int utf8acceptnonsurrogates_HIADD = 0x00000000;

static const uint8 utf8acceptnonsurrogates[] = {

  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  2,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   7,   3,   3,
  4,   5,   5,   5,   6, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  8,   8,   8,   8,   8,   8,   8,   8,    8,   8,   8,   8,   8,   8,   8,   8,
  8,   8,   8,   8,   8,   8,   8,   8,    8,   8,   8,   8,   8,   8,   8,   8,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,

RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,  RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,
RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,  RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,
RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,  RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,
RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,  RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_, RJ_,

X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
X__, X__, X__, X__, X__, X__, X__, X__,  X__, X__, X__, X__, X__, X__, X__, X__,
};

static const RemapEntry utf8acceptnonsurrogates_remap_base[] = {
{0, 0, 0} };

static const unsigned char utf8acceptnonsurrogates_remap_string[] = {
0 };

static const unsigned char utf8acceptnonsurrogates_fast[256] = {
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,

0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,

1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,

1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
};

static const UTF8ScanObj utf8acceptnonsurrogates_obj = {
  utf8acceptnonsurrogates_STATE0,
  utf8acceptnonsurrogates_STATE0_SIZE,
  utf8acceptnonsurrogates_TOTAL_SIZE,
  utf8acceptnonsurrogates_MAX_EXPAND_X4,
  utf8acceptnonsurrogates_SHIFT,
  utf8acceptnonsurrogates_BYTES,
  utf8acceptnonsurrogates_LOSUB,
  utf8acceptnonsurrogates_HIADD,
  utf8acceptnonsurrogates,
  utf8acceptnonsurrogates_remap_base,
  utf8acceptnonsurrogates_remap_string,
  utf8acceptnonsurrogates_fast
};

#undef X__
#undef RJ_
#undef S1_
#undef S2_
#undef S3_
#undef S21
#undef S31
#undef S32
#undef T1_
#undef T2_
#undef S11
#undef SP_
#undef D__
#undef RJA

static inline bool InStateZero(const UTF8ScanObj* st, const uint8* Tbl) {
  const uint8* Tbl0 = &st->state_table[st->state0];
  return (static_cast<uint32>(Tbl - Tbl0) < st->state0_size);
}

int UTF8GenericScan(const UTF8ScanObj* st,
                    const char * str,
                    int str_length,
                    int* bytes_consumed) {
  *bytes_consumed = 0;
  if (str_length == 0) return kExitOK;

  int eshift = st->entry_shift;
  const uint8* isrc = reinterpret_cast<const uint8*>(str);
  const uint8* src = isrc;
  const uint8* srclimit = isrc + str_length;
  const uint8* srclimit8 = srclimit - 7;
  const uint8* Tbl_0 = &st->state_table[st->state0];

 DoAgain:

  int e = 0;
  uint8 c;
  const uint8* Tbl2 = &st->fast_state[0];
  const uint32 losub = st->losub;
  const uint32 hiadd = st->hiadd;

  while ((((uintptr_t)src & 0x07) != 0) &&
         (src < srclimit) &&
         Tbl2[src[0]] == 0) {
    src++;
  }
  if (((uintptr_t)src & 0x07) == 0) {

    while (src < srclimit8) {
      uint32 s0123 = (reinterpret_cast<const uint32 *>(src))[0];
      uint32 s4567 = (reinterpret_cast<const uint32 *>(src))[1];
      src += 8;

      uint32 temp = (s0123 - losub) | (s0123 + hiadd) |
                    (s4567 - losub) | (s4567 + hiadd);
      if ((temp & 0x80808080) != 0) {

        int e0123 = (Tbl2[src[-8]] | Tbl2[src[-7]]) |
                    (Tbl2[src[-6]] | Tbl2[src[-5]]);
        if (e0123 != 0) {
          src -= 8;
          break;
        }
        e0123 = (Tbl2[src[-4]] | Tbl2[src[-3]]) |
                (Tbl2[src[-2]] | Tbl2[src[-1]]);
        if (e0123 != 0) {
          src -= 4;
          break;
        }

      }
    }
  }

  const uint8* Tbl = Tbl_0;
  while (src < srclimit) {
    c = *src;
    e = Tbl[c];
    src++;
    if (e >= kExitIllegalStructure) {break;}
    Tbl = &Tbl_0[e << eshift];
  }

  if (e >= kExitIllegalStructure) {

    src--;

    if (!InStateZero(st, Tbl)) {
      do {
        src--;
      } while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
    }
  } else if (!InStateZero(st, Tbl)) {

    e = kExitIllegalStructure;
    do {
      src--;
    } while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
  } else {

    e = kExitOK;
  }

  if (e == kExitDoAgain) {

    goto DoAgain;
  }

  *bytes_consumed = src - isrc;
  return e;
}

int UTF8GenericScanFastAscii(const UTF8ScanObj* st,
                    const char * str,
                    int str_length,
                    int* bytes_consumed) {
  *bytes_consumed = 0;
  if (str_length == 0) return kExitOK;

  const uint8* isrc =  reinterpret_cast<const uint8*>(str);
  const uint8* src = isrc;
  const uint8* srclimit = isrc + str_length;
  const uint8* srclimit8 = srclimit - 7;
  int n;
  int rest_consumed;
  int exit_reason;
  do {

    while ((((uintptr_t)src & 0x07) != 0) &&
           (src < srclimit) && (src[0] < 0x80)) {
      src++;
    }
    if (((uintptr_t)src & 0x07) == 0) {
      while ((src < srclimit8) &&
             (((reinterpret_cast<const uint32*>(src)[0] |
                reinterpret_cast<const uint32*>(src)[1]) & 0x80808080) == 0)) {
        src += 8;
      }
    }
    while ((src < srclimit) && (src[0] < 0x80)) {
      src++;
    }

    n = src - isrc;
    exit_reason = UTF8GenericScan(st, str + n, str_length - n, &rest_consumed);
    src += rest_consumed;
  } while ( exit_reason == kExitDoAgain );

  *bytes_consumed = src - isrc;
  return exit_reason;
}

namespace {

bool module_initialized_ = false;

struct InitDetector {
  InitDetector() {
    module_initialized_ = true;
  }
};
InitDetector init_detector;

}

bool IsStructurallyValidUTF8(const char* buf, int len) {
  if (!module_initialized_) return true;

  int bytes_consumed = 0;
  UTF8GenericScanFastAscii(&utf8acceptnonsurrogates_obj,
                           buf, len, &bytes_consumed);
  return (bytes_consumed == len);
}

}
}
}
