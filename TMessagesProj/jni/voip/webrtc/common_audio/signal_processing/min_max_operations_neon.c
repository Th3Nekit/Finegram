/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include <stdlib.h>

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

int16_t WebRtcSpl_MaxAbsValueW16Neon(const int16_t* vector, size_t length) {
  int absolute = 0, maximum = 0;

  RTC_DCHECK_GT(length, 0);

  const int16_t* p_start = vector;
  size_t rest = length & 7;
  const int16_t* p_end = vector + length - rest;

  int16x8_t v;
  uint16x8_t max_qv;
  max_qv = vdupq_n_u16(0);

  while (p_start < p_end) {
    v = vld1q_s16(p_start);

    v = vabsq_s16(v);

    max_qv = vmaxq_u16(max_qv, vreinterpretq_u16_s16(v));
    p_start += 8;
  }

#ifdef WEBRTC_ARCH_ARM64
  maximum = (int)vmaxvq_u16(max_qv);
#else
  uint16x4_t max_dv;
  max_dv = vmax_u16(vget_low_u16(max_qv), vget_high_u16(max_qv));
  max_dv = vpmax_u16(max_dv, max_dv);
  max_dv = vpmax_u16(max_dv, max_dv);

  maximum = (int)vget_lane_u16(max_dv, 0);
#endif

  p_end = vector + length;
  while (p_start < p_end) {
    absolute = abs((int)(*p_start));

    if (absolute > maximum) {
      maximum = absolute;
    }
    p_start++;
  }

  if (maximum > WEBRTC_SPL_WORD16_MAX) {
    maximum = WEBRTC_SPL_WORD16_MAX;
  }

  return (int16_t)maximum;
}

int32_t WebRtcSpl_MaxAbsValueW32Neon(const int32_t* vector, size_t length) {

  uint32_t absolute = 0, maximum = 0;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int32_t* p_start = vector;
  uint32x4_t max32x4_0 = vdupq_n_u32(0);
  uint32x4_t max32x4_1 = vdupq_n_u32(0);

  for (i = 0; i < length - residual; i += 8) {
    int32x4_t in32x4_0 = vld1q_s32(p_start);
    p_start += 4;
    int32x4_t in32x4_1 = vld1q_s32(p_start);
    p_start += 4;
    in32x4_0 = vabsq_s32(in32x4_0);
    in32x4_1 = vabsq_s32(in32x4_1);

    max32x4_0 = vmaxq_u32(max32x4_0, vreinterpretq_u32_s32(in32x4_0));
    max32x4_1 = vmaxq_u32(max32x4_1, vreinterpretq_u32_s32(in32x4_1));
  }

  uint32x4_t max32x4 = vmaxq_u32(max32x4_0, max32x4_1);
#if defined(WEBRTC_ARCH_ARM64)
  maximum = vmaxvq_u32(max32x4);
#else
  uint32x2_t max32x2 = vmax_u32(vget_low_u32(max32x4), vget_high_u32(max32x4));
  max32x2 = vpmax_u32(max32x2, max32x2);

  maximum = vget_lane_u32(max32x2, 0);
#endif

  for (i = residual; i > 0; i--) {
    absolute = abs((int)(*p_start));
    if (absolute > maximum) {
      maximum = absolute;
    }
    p_start++;
  }

  maximum = WEBRTC_SPL_MIN(maximum, WEBRTC_SPL_WORD32_MAX);

  return (int32_t)maximum;
}

int16_t WebRtcSpl_MaxValueW16Neon(const int16_t* vector, size_t length) {
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int16_t* p_start = vector;
  int16x8_t max16x8 = vdupq_n_s16(WEBRTC_SPL_WORD16_MIN);

  for (i = 0; i < length - residual; i += 8) {
    int16x8_t in16x8 = vld1q_s16(p_start);
    max16x8 = vmaxq_s16(max16x8, in16x8);
    p_start += 8;
  }

#if defined(WEBRTC_ARCH_ARM64)
  maximum = vmaxvq_s16(max16x8);
#else
  int16x4_t max16x4 = vmax_s16(vget_low_s16(max16x8), vget_high_s16(max16x8));
  max16x4 = vpmax_s16(max16x4, max16x4);
  max16x4 = vpmax_s16(max16x4, max16x4);

  maximum = vget_lane_s16(max16x4, 0);
#endif

  for (i = residual; i > 0; i--) {
    if (*p_start > maximum)
      maximum = *p_start;
    p_start++;
  }
  return maximum;
}

int32_t WebRtcSpl_MaxValueW32Neon(const int32_t* vector, size_t length) {
  int32_t maximum = WEBRTC_SPL_WORD32_MIN;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int32_t* p_start = vector;
  int32x4_t max32x4_0 = vdupq_n_s32(WEBRTC_SPL_WORD32_MIN);
  int32x4_t max32x4_1 = vdupq_n_s32(WEBRTC_SPL_WORD32_MIN);

  for (i = 0; i < length - residual; i += 8) {
    int32x4_t in32x4_0 = vld1q_s32(p_start);
    p_start += 4;
    int32x4_t in32x4_1 = vld1q_s32(p_start);
    p_start += 4;
    max32x4_0 = vmaxq_s32(max32x4_0, in32x4_0);
    max32x4_1 = vmaxq_s32(max32x4_1, in32x4_1);
  }

  int32x4_t max32x4 = vmaxq_s32(max32x4_0, max32x4_1);
#if defined(WEBRTC_ARCH_ARM64)
  maximum = vmaxvq_s32(max32x4);
#else
  int32x2_t max32x2 = vmax_s32(vget_low_s32(max32x4), vget_high_s32(max32x4));
  max32x2 = vpmax_s32(max32x2, max32x2);

  maximum = vget_lane_s32(max32x2, 0);
#endif

  for (i = residual; i > 0; i--) {
    if (*p_start > maximum)
      maximum = *p_start;
    p_start++;
  }
  return maximum;
}

int16_t WebRtcSpl_MinValueW16Neon(const int16_t* vector, size_t length) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int16_t* p_start = vector;
  int16x8_t min16x8 = vdupq_n_s16(WEBRTC_SPL_WORD16_MAX);

  for (i = 0; i < length - residual; i += 8) {
    int16x8_t in16x8 = vld1q_s16(p_start);
    min16x8 = vminq_s16(min16x8, in16x8);
    p_start += 8;
  }

#if defined(WEBRTC_ARCH_ARM64)
  minimum = vminvq_s16(min16x8);
#else
  int16x4_t min16x4 = vmin_s16(vget_low_s16(min16x8), vget_high_s16(min16x8));
  min16x4 = vpmin_s16(min16x4, min16x4);
  min16x4 = vpmin_s16(min16x4, min16x4);

  minimum = vget_lane_s16(min16x4, 0);
#endif

  for (i = residual; i > 0; i--) {
    if (*p_start < minimum)
      minimum = *p_start;
    p_start++;
  }
  return minimum;
}

int32_t WebRtcSpl_MinValueW32Neon(const int32_t* vector, size_t length) {
  int32_t minimum = WEBRTC_SPL_WORD32_MAX;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int32_t* p_start = vector;
  int32x4_t min32x4_0 = vdupq_n_s32(WEBRTC_SPL_WORD32_MAX);
  int32x4_t min32x4_1 = vdupq_n_s32(WEBRTC_SPL_WORD32_MAX);

  for (i = 0; i < length - residual; i += 8) {
    int32x4_t in32x4_0 = vld1q_s32(p_start);
    p_start += 4;
    int32x4_t in32x4_1 = vld1q_s32(p_start);
    p_start += 4;
    min32x4_0 = vminq_s32(min32x4_0, in32x4_0);
    min32x4_1 = vminq_s32(min32x4_1, in32x4_1);
  }

  int32x4_t min32x4 = vminq_s32(min32x4_0, min32x4_1);
#if defined(WEBRTC_ARCH_ARM64)
  minimum = vminvq_s32(min32x4);
#else
  int32x2_t min32x2 = vmin_s32(vget_low_s32(min32x4), vget_high_s32(min32x4));
  min32x2 = vpmin_s32(min32x2, min32x2);

  minimum = vget_lane_s32(min32x2, 0);
#endif

  for (i = residual; i > 0; i--) {
    if (*p_start < minimum)
      minimum = *p_start;
    p_start++;
  }
  return minimum;
}

void WebRtcSpl_MinMaxW16Neon(const int16_t* vector, size_t length,
                             int16_t* min_val, int16_t* max_val) {
  int16_t minimum = WEBRTC_SPL_WORD16_MAX;
  int16_t maximum = WEBRTC_SPL_WORD16_MIN;
  size_t i = 0;
  size_t residual = length & 0x7;

  RTC_DCHECK_GT(length, 0);

  const int16_t* p_start = vector;
  int16x8_t min16x8 = vdupq_n_s16(WEBRTC_SPL_WORD16_MAX);
  int16x8_t max16x8 = vdupq_n_s16(WEBRTC_SPL_WORD16_MIN);

  for (i = 0; i < length - residual; i += 8) {
    int16x8_t in16x8 = vld1q_s16(p_start);
    min16x8 = vminq_s16(min16x8, in16x8);
    max16x8 = vmaxq_s16(max16x8, in16x8);
    p_start += 8;
  }

#if defined(WEBRTC_ARCH_ARM64)
  minimum = vminvq_s16(min16x8);
  maximum = vmaxvq_s16(max16x8);
#else
  int16x4_t min16x4 = vmin_s16(vget_low_s16(min16x8), vget_high_s16(min16x8));
  min16x4 = vpmin_s16(min16x4, min16x4);
  min16x4 = vpmin_s16(min16x4, min16x4);

  minimum = vget_lane_s16(min16x4, 0);

  int16x4_t max16x4 = vmax_s16(vget_low_s16(max16x8), vget_high_s16(max16x8));
  max16x4 = vpmax_s16(max16x4, max16x4);
  max16x4 = vpmax_s16(max16x4, max16x4);

  maximum = vget_lane_s16(max16x4, 0);
#endif

  for (i = residual; i > 0; i--) {
    if (*p_start < minimum)
      minimum = *p_start;
    if (*p_start > maximum)
      maximum = *p_start;
    p_start++;
  }
  *min_val = minimum;
  *max_val = maximum;
}
