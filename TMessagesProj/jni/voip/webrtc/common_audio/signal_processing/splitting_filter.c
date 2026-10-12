/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the splitting filter functions.
 *
 */

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

enum
{
    kMaxBandFrameLength = 320
};

static const uint16_t WebRtcSpl_kAllPassFilter1[3] = {6418, 36982, 57261};
static const uint16_t WebRtcSpl_kAllPassFilter2[3] = {21333, 49062, 63010};

static void WebRtcSpl_AllPassQMF(int32_t* in_data,
                                 size_t data_length,
                                 int32_t* out_data,
                                 const uint16_t* filter_coefficients,
                                 int32_t* filter_state)
{

    size_t k;
    int32_t diff;

    diff = WebRtcSpl_SubSatW32(in_data[0], filter_state[1]);

    out_data[0] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[0], diff, filter_state[0]);

    for (k = 1; k < data_length; k++)
    {

        diff = WebRtcSpl_SubSatW32(in_data[k], out_data[k - 1]);

        out_data[k] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[0], diff, in_data[k - 1]);
    }

    filter_state[0] = in_data[data_length - 1];
    filter_state[1] = out_data[data_length - 1];

    diff = WebRtcSpl_SubSatW32(out_data[0], filter_state[3]);

    in_data[0] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[1], diff, filter_state[2]);
    for (k = 1; k < data_length; k++)
    {

        diff = WebRtcSpl_SubSatW32(out_data[k], in_data[k - 1]);

        in_data[k] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[1], diff, out_data[k-1]);
    }

    filter_state[2] = out_data[data_length - 1];
    filter_state[3] = in_data[data_length - 1];

    diff = WebRtcSpl_SubSatW32(in_data[0], filter_state[5]);

    out_data[0] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[2], diff, filter_state[4]);
    for (k = 1; k < data_length; k++)
    {

        diff = WebRtcSpl_SubSatW32(in_data[k], out_data[k - 1]);

        out_data[k] = WEBRTC_SPL_SCALEDIFF32(filter_coefficients[2], diff, in_data[k-1]);
    }
    filter_state[4] = in_data[data_length - 1];
    filter_state[5] = out_data[data_length - 1];
}

void WebRtcSpl_AnalysisQMF(const int16_t* in_data, size_t in_data_length,
                           int16_t* low_band, int16_t* high_band,
                           int32_t* filter_state1, int32_t* filter_state2)
{
    size_t i;
    int16_t k;
    int32_t tmp;
    int32_t half_in1[kMaxBandFrameLength];
    int32_t half_in2[kMaxBandFrameLength];
    int32_t filter1[kMaxBandFrameLength];
    int32_t filter2[kMaxBandFrameLength];
    const size_t band_length = in_data_length / 2;
    RTC_DCHECK_EQ(0, in_data_length % 2);
    RTC_DCHECK_LE(band_length, kMaxBandFrameLength);

    for (i = 0, k = 0; i < band_length; i++, k += 2)
    {
        half_in2[i] = ((int32_t)in_data[k]) * (1 << 10);
        half_in1[i] = ((int32_t)in_data[k + 1]) * (1 << 10);
    }

    WebRtcSpl_AllPassQMF(half_in1, band_length, filter1,
                         WebRtcSpl_kAllPassFilter1, filter_state1);
    WebRtcSpl_AllPassQMF(half_in2, band_length, filter2,
                         WebRtcSpl_kAllPassFilter2, filter_state2);

    for (i = 0; i < band_length; i++)
    {
        tmp = (filter1[i] + filter2[i] + 1024) >> 11;
        low_band[i] = WebRtcSpl_SatW32ToW16(tmp);

        tmp = (filter1[i] - filter2[i] + 1024) >> 11;
        high_band[i] = WebRtcSpl_SatW32ToW16(tmp);
    }
}

void WebRtcSpl_SynthesisQMF(const int16_t* low_band, const int16_t* high_band,
                            size_t band_length, int16_t* out_data,
                            int32_t* filter_state1, int32_t* filter_state2)
{
    int32_t tmp;
    int32_t half_in1[kMaxBandFrameLength];
    int32_t half_in2[kMaxBandFrameLength];
    int32_t filter1[kMaxBandFrameLength];
    int32_t filter2[kMaxBandFrameLength];
    size_t i;
    int16_t k;
    RTC_DCHECK_LE(band_length, kMaxBandFrameLength);

    for (i = 0; i < band_length; i++)
    {
        tmp = (int32_t)low_band[i] + (int32_t)high_band[i];
        half_in1[i] = tmp * (1 << 10);
        tmp = (int32_t)low_band[i] - (int32_t)high_band[i];
        half_in2[i] = tmp * (1 << 10);
    }

    WebRtcSpl_AllPassQMF(half_in1, band_length, filter1,
                         WebRtcSpl_kAllPassFilter2, filter_state1);
    WebRtcSpl_AllPassQMF(half_in2, band_length, filter2,
                         WebRtcSpl_kAllPassFilter1, filter_state2);

    for (i = 0, k = 0; i < band_length; i++)
    {
        tmp = (filter2[i] + 512) >> 10;
        out_data[k++] = WebRtcSpl_SatW32ToW16(tmp);

        tmp = (filter1[i] + 512) >> 10;
        out_data[k++] = WebRtcSpl_SatW32ToW16(tmp);
    }

}
