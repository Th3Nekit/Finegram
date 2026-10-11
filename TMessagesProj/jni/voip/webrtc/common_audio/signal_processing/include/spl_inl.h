/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This header file includes the inline functions in
// the fix point signal processing library.

#ifndef COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SPL_INL_H_
#define COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_SPL_INL_H_

#include <stdint.h>

#include "rtc_base/compile_assert_c.h"

extern const int8_t kWebRtcSpl_CountLeadingZeros32_Table[64];

static __inline int WebRtcSpl_CountLeadingZeros32_NotBuiltin(uint32_t n) {

  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;

  return kWebRtcSpl_CountLeadingZeros32_Table[(n * 0x8c0b2891) >> 26];
}

static __inline int WebRtcSpl_CountLeadingZeros64_NotBuiltin(uint64_t n) {
  const int leading_zeros = n >> 32 == 0 ? 32 : 0;
  return leading_zeros + WebRtcSpl_CountLeadingZeros32_NotBuiltin(
                             (uint32_t)(n >> (32 - leading_zeros)));
}

static __inline int WebRtcSpl_CountLeadingZeros32(uint32_t n) {
#ifdef __GNUC__
  RTC_COMPILE_ASSERT(sizeof(unsigned int) == sizeof(uint32_t));
  return n == 0 ? 32 : __builtin_clz(n);
#else
  return WebRtcSpl_CountLeadingZeros32_NotBuiltin(n);
#endif
}

static __inline int WebRtcSpl_CountLeadingZeros64(uint64_t n) {
#ifdef __GNUC__
  RTC_COMPILE_ASSERT(sizeof(unsigned long long) == sizeof(uint64_t));
  return n == 0 ? 64 : __builtin_clzll(n);
#else
  return WebRtcSpl_CountLeadingZeros64_NotBuiltin(n);
#endif
}

#ifdef WEBRTC_ARCH_ARM_V7
#include "common_audio/signal_processing/include/spl_inl_armv7.h"
#else

#if defined(MIPS32_LE)
#include "common_audio/signal_processing/include/spl_inl_mips.h"
#endif

#if !defined(MIPS_DSP_R1_LE)
static __inline int16_t WebRtcSpl_SatW32ToW16(int32_t value32) {
  int16_t out16 = (int16_t)value32;

  if (value32 > 32767)
    out16 = 32767;
  else if (value32 < -32768)
    out16 = -32768;

  return out16;
}

static __inline int32_t WebRtcSpl_AddSatW32(int32_t a, int32_t b) {

  const int32_t sum = (int32_t)((uint32_t)a + (uint32_t)b);

  if ((a < 0) == (b < 0) && (a < 0) != (sum < 0)) {

    return sum < 0 ? INT32_MAX : INT32_MIN;
  }
  return sum;
}

static __inline int32_t WebRtcSpl_SubSatW32(int32_t a, int32_t b) {

  const int32_t diff = (int32_t)((uint32_t)a - (uint32_t)b);

  if ((a < 0) != (b < 0) && (a < 0) != (diff < 0)) {

    return diff < 0 ? INT32_MAX : INT32_MIN;
  }
  return diff;
}

static __inline int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b) {
  return WebRtcSpl_SatW32ToW16((int32_t)a + (int32_t)b);
}

static __inline int16_t WebRtcSpl_SubSatW16(int16_t var1, int16_t var2) {
  return WebRtcSpl_SatW32ToW16((int32_t)var1 - (int32_t)var2);
}
#endif

#if !defined(MIPS32_LE)
static __inline int16_t WebRtcSpl_GetSizeInBits(uint32_t n) {
  return 32 - WebRtcSpl_CountLeadingZeros32(n);
}

static __inline int16_t WebRtcSpl_NormW32(int32_t a) {
  return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a < 0 ? ~a : a) - 1;
}

static __inline int16_t WebRtcSpl_NormU32(uint32_t a) {
  return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a);
}

static __inline int16_t WebRtcSpl_NormW16(int16_t a) {
  const int32_t a32 = a;
  return a == 0 ? 0 : WebRtcSpl_CountLeadingZeros32(a < 0 ? ~a32 : a32) - 17;
}

static __inline int32_t WebRtc_MulAccumW16(int16_t a, int16_t b, int32_t c) {
  return (a * b + c);
}
#endif

#endif

#endif
