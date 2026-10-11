/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/normal.h"

#include <string.h>

#include <algorithm>

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/background_noise.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/expand.h"
#include "rtc_base/checks.h"

namespace webrtc {

int Normal::Process(const int16_t* input,
                    size_t length,
                    NetEq::Mode last_mode,
                    AudioMultiVector* output) {
  if (length == 0) {

    output->Clear();
    return static_cast<int>(length);
  }

  RTC_DCHECK(output->Empty());

  if (length % output->Channels() != 0) {

    output->Clear();
    return 0;
  }
  output->PushBackInterleaved(rtc::ArrayView<const int16_t>(input, length));

  const int fs_mult = fs_hz_ / 8000;
  RTC_DCHECK_GT(fs_mult, 0);

  const int fs_shift = 30 - WebRtcSpl_NormW32(fs_mult);

  if (last_mode == NetEq::Mode::kCodecPlc) {
    statistics_->EndExpandEvent(fs_hz_);
  }

  if (last_mode == NetEq::Mode::kExpand) {

    expand_->SetParametersForNormalAfterExpand();

    AudioMultiVector expanded(output->Channels());
    expand_->Process(&expanded);
    expand_->Reset();

    size_t length_per_channel = length / output->Channels();
    std::unique_ptr<int16_t[]> signal(new int16_t[length_per_channel]);
    for (size_t channel_ix = 0; channel_ix < output->Channels(); ++channel_ix) {

      int16_t mute_factor = expand_->MuteFactor(channel_ix);

      (*output)[channel_ix].CopyTo(length_per_channel, 0, signal.get());

      int16_t decoded_max =
          WebRtcSpl_MaxAbsValueW16(signal.get(), length_per_channel);

      size_t energy_length =
          std::min(static_cast<size_t>(fs_mult * 64), length_per_channel);
      int scaling = 6 + fs_shift - WebRtcSpl_NormW32(decoded_max * decoded_max);
      scaling = std::max(scaling, 0);
      int32_t energy = WebRtcSpl_DotProductWithScale(signal.get(), signal.get(),
                                                     energy_length, scaling);
      int32_t scaled_energy_length =
          static_cast<int32_t>(energy_length >> scaling);
      if (scaled_energy_length > 0) {
        energy = energy / scaled_energy_length;
      } else {
        energy = 0;
      }

      int local_mute_factor = 16384;
      if ((energy != 0) && (energy > background_noise_.Energy(channel_ix))) {

        scaling = WebRtcSpl_NormW32(energy) - 16;

        int32_t bgn_energy = WEBRTC_SPL_SHIFT_W32(
            background_noise_.Energy(channel_ix), scaling + 14);
        int16_t energy_scaled =
            static_cast<int16_t>(WEBRTC_SPL_SHIFT_W32(energy, scaling));
        int32_t ratio = WebRtcSpl_DivW32W16(bgn_energy, energy_scaled);
        local_mute_factor =
            std::min(local_mute_factor, WebRtcSpl_SqrtFloor(ratio << 14));
      }
      mute_factor = std::max<int16_t>(mute_factor, local_mute_factor);
      RTC_DCHECK_LE(mute_factor, 16384);
      RTC_DCHECK_GE(mute_factor, 0);

      const int back_to_fullscale_inc =
          static_cast<int>((16384 - mute_factor) / length_per_channel);
      const int increment = std::max(64 / fs_mult, back_to_fullscale_inc);
      for (size_t i = 0; i < length_per_channel; i++) {

        RTC_DCHECK_LT(channel_ix, output->Channels());
        RTC_DCHECK_LT(i, output->Size());
        int32_t scaled_signal = (*output)[channel_ix][i] * mute_factor;

        (*output)[channel_ix][i] =
            static_cast<int16_t>((scaled_signal + 8192) >> 14);

        mute_factor =
            static_cast<int16_t>(std::min(mute_factor + increment, 16384));
      }

      size_t win_length = samples_per_ms_;
      int16_t win_slope_Q14 = default_win_slope_Q14_;
      RTC_DCHECK_LT(channel_ix, output->Channels());
      if (win_length > output->Size()) {
        win_length = output->Size();
        win_slope_Q14 = (1 << 14) / static_cast<int16_t>(win_length);
      }
      int16_t win_up_Q14 = 0;
      for (size_t i = 0; i < win_length; i++) {
        win_up_Q14 += win_slope_Q14;
        (*output)[channel_ix][i] =
            (win_up_Q14 * (*output)[channel_ix][i] +
             ((1 << 14) - win_up_Q14) * expanded[channel_ix][i] + (1 << 13)) >>
            14;
      }
      RTC_DCHECK_GT(win_up_Q14,
                    (1 << 14) - 32);
    }
  } else if (last_mode == NetEq::Mode::kRfc3389Cng) {
    RTC_DCHECK_EQ(output->Channels(), 1);
    static const size_t kCngLength = 48;
    RTC_DCHECK_LE(8 * fs_mult, kCngLength);
    int16_t cng_output[kCngLength];
    ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();

    if (cng_decoder) {

      if (!cng_decoder->Generate(cng_output, false)) {

        memset(cng_output, 0, sizeof(cng_output));
      }
    } else {

      (*output)[0].CopyTo(fs_mult * 8, 0, cng_output);
    }

    size_t win_length = samples_per_ms_;
    int16_t win_slope_Q14 = default_win_slope_Q14_;
    if (win_length > kCngLength) {
      win_length = kCngLength;
      win_slope_Q14 = (1 << 14) / static_cast<int16_t>(win_length);
    }
    int16_t win_up_Q14 = 0;
    for (size_t i = 0; i < win_length; i++) {
      win_up_Q14 += win_slope_Q14;
      (*output)[0][i] =
          (win_up_Q14 * (*output)[0][i] +
           ((1 << 14) - win_up_Q14) * cng_output[i] + (1 << 13)) >>
          14;
    }
    RTC_DCHECK_GT(win_up_Q14,
                  (1 << 14) - 32);
  }

  return static_cast<int>(length);
}

}
