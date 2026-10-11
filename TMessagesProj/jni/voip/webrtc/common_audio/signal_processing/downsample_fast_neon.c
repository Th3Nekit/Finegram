/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/signal_processing/include/signal_processing_library.h"

#include <arm_neon.h>

int WebRtcSpl_DownsampleFastNeon(const int16_t* data_in,
                                 size_t data_in_length,
                                 int16_t* data_out,
                                 size_t data_out_length,
                                 const int16_t* __restrict coefficients,
                                 size_t coefficients_length,
                                 int factor,
                                 size_t delay) {
  size_t i = 0;
  size_t j = 0;
  int32_t out_s32 = 0;
  size_t endpos = delay + factor * (data_out_length - 1) + 1;
  size_t res = data_out_length & 0x7;
  size_t endpos1 = endpos - factor * res;

  if (data_out_length == 0 || coefficients_length == 0
                           || data_in_length < endpos) {
    return -1;
  }

  switch (factor) {
    case 2: {
      for (i = delay; i < endpos1; i += 16) {

        int32x4_t out32x4_0 = vdupq_n_s32(2048);
        int32x4_t out32x4_1 = vdupq_n_s32(2048);

#if defined(WEBRTC_ARCH_ARM64)

        for (j = 0; j < coefficients_length - 1; j += 2) {
          int32x2_t coeff32 = vld1_dup_s32((int32_t*)&coefficients[j]);
          int16x4_t coeff16x4 = vreinterpret_s16_s32(coeff32);
          int16x8x2_t in16x8x2 = vld2q_s16(&data_in[i - j - 1]);

          int16x4_t in16x4_0 = vget_low_s16(in16x8x2.val[0]);
          int16x4_t in16x4_1 = vget_low_s16(in16x8x2.val[1]);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 1);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_1, coeff16x4, 0);

          int16x4_t in16x4_2 = vget_high_s16(in16x8x2.val[0]);
          int16x4_t in16x4_3 = vget_high_s16(in16x8x2.val[1]);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_2, coeff16x4, 1);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_3, coeff16x4, 0);
        }

        for (; j < coefficients_length; j++) {
          int16x4_t coeff16x4 = vld1_dup_s16(&coefficients[j]);
          int16x8x2_t in16x8x2 = vld2q_s16(&data_in[i - j]);

          int16x4_t in16x4_0 = vget_low_s16(in16x8x2.val[0]);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 0);

          int16x4_t in16x4_1 = vget_high_s16(in16x8x2.val[0]);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_1, coeff16x4, 0);
        }
#else

        for (j = 0; j < coefficients_length; j++) {
          int16x4_t coeff16x4 = vld1_dup_s16(&coefficients[j]);
          int16x8x2_t in16x8x2 = vld2q_s16(&data_in[i - j]);

          int16x4_t in16x4_0 = vget_low_s16(in16x8x2.val[0]);
          int16x4_t in16x4_1 = vget_high_s16(in16x8x2.val[0]);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 0);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_1, coeff16x4, 0);
        }
#endif

        int16x4_t out16x4_0 = vqshrn_n_s32(out32x4_0, 12);
        int16x4_t out16x4_1 = vqshrn_n_s32(out32x4_1, 12);
        vst1q_s16(data_out, vcombine_s16(out16x4_0, out16x4_1));
        data_out += 8;
      }
      break;
    }
    case 4: {
      for (i = delay; i < endpos1; i += 32) {

        int32x4_t out32x4_0 = vdupq_n_s32(2048);
        int32x4_t out32x4_1 = vdupq_n_s32(2048);

        for (j = 0; j < coefficients_length - 3; j += 4) {
          int16x4_t coeff16x4 = vld1_s16(&coefficients[j]);
          int16x8x4_t in16x8x4 = vld4q_s16(&data_in[i - j - 3]);

          int16x4_t in16x4_0 = vget_low_s16(in16x8x4.val[0]);
          int16x4_t in16x4_2 = vget_low_s16(in16x8x4.val[1]);
          int16x4_t in16x4_4 = vget_low_s16(in16x8x4.val[2]);
          int16x4_t in16x4_6 = vget_low_s16(in16x8x4.val[3]);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 3);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_2, coeff16x4, 2);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_4, coeff16x4, 1);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_6, coeff16x4, 0);

          int16x4_t in16x4_1 = vget_high_s16(in16x8x4.val[0]);
          int16x4_t in16x4_3 = vget_high_s16(in16x8x4.val[1]);
          int16x4_t in16x4_5 = vget_high_s16(in16x8x4.val[2]);
          int16x4_t in16x4_7 = vget_high_s16(in16x8x4.val[3]);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_1, coeff16x4, 3);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_3, coeff16x4, 2);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_5, coeff16x4, 1);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_7, coeff16x4, 0);
        }

        for (; j < coefficients_length; j++) {
          int16x4_t coeff16x4 = vld1_dup_s16(&coefficients[j]);
          int16x8x4_t in16x8x4 = vld4q_s16(&data_in[i - j]);

          int16x4_t in16x4_0 = vget_low_s16(in16x8x4.val[0]);
          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 0);

          int16x4_t in16x4_1 = vget_high_s16(in16x8x4.val[0]);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_1, coeff16x4, 0);
        }

        int16x4_t out16x4_0 = vqshrn_n_s32(out32x4_0, 12);
        int16x4_t out16x4_1 = vqshrn_n_s32(out32x4_1, 12);
        vst1q_s16(data_out, vcombine_s16(out16x4_0, out16x4_1));
        data_out += 8;
      }
      break;
    }
    default: {
      for (i = delay; i < endpos1; i += factor * 8) {

        int32x4_t out32x4_0 = vdupq_n_s32(2048);
        int32x4_t out32x4_1 = vdupq_n_s32(2048);

        for (j = 0; j < coefficients_length; j++) {
          int16x4_t coeff16x4 = vld1_dup_s16(&coefficients[j]);
          int16x4_t in16x4_0 = vld1_dup_s16(&data_in[i - j]);
          in16x4_0 = vld1_lane_s16(&data_in[i + factor - j], in16x4_0, 1);
          in16x4_0 = vld1_lane_s16(&data_in[i + factor * 2 - j], in16x4_0, 2);
          in16x4_0 = vld1_lane_s16(&data_in[i + factor * 3 - j], in16x4_0, 3);
          int16x4_t in16x4_1 = vld1_dup_s16(&data_in[i + factor * 4 - j]);
          in16x4_1 = vld1_lane_s16(&data_in[i + factor * 5 - j], in16x4_1, 1);
          in16x4_1 = vld1_lane_s16(&data_in[i + factor * 6 - j], in16x4_1, 2);
          in16x4_1 = vld1_lane_s16(&data_in[i + factor * 7 - j], in16x4_1, 3);

          out32x4_0 = vmlal_lane_s16(out32x4_0, in16x4_0, coeff16x4, 0);
          out32x4_1 = vmlal_lane_s16(out32x4_1, in16x4_1, coeff16x4, 0);
        }

        int16x4_t out16x4_0 = vqshrn_n_s32(out32x4_0, 12);
        int16x4_t out16x4_1 = vqshrn_n_s32(out32x4_1, 12);
        vst1q_s16(data_out, vcombine_s16(out16x4_0, out16x4_1));
        data_out += 8;
      }
      break;
    }
  }

  for (; i < endpos; i += factor) {
    out_s32 = 2048;

    for (j = 0; j < coefficients_length; j++) {
      out_s32 = WebRtc_MulAccumW16(coefficients[j], data_in[i - j], out_s32);
    }

    out_s32 >>= 12;
    *data_out++ = WebRtcSpl_SatW32ToW16(out_s32);
  }

  return 0;
}
