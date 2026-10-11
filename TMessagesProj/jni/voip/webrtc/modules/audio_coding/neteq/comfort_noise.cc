/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/comfort_noise.h"

#include <cstdint>
#include <memory>

#include "api/array_view.h"
#include "modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "modules/audio_coding/neteq/audio_multi_vector.h"
#include "modules/audio_coding/neteq/audio_vector.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/dsp_helper.h"
#include "modules/audio_coding/neteq/sync_buffer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

void ComfortNoise::Reset() {
  first_call_ = true;
}

int ComfortNoise::UpdateParameters(const Packet& packet) {

  if (decoder_database_->SetActiveCngDecoder(packet.payload_type) != kOK) {
    return kUnknownPayloadType;
  }
  ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
  RTC_DCHECK(cng_decoder);
  cng_decoder->UpdateSid(packet.payload);
  return kOK;
}

int ComfortNoise::Generate(size_t requested_length, AudioMultiVector* output) {

  RTC_DCHECK(fs_hz_ == 8000 || fs_hz_ == 16000 || fs_hz_ == 32000 ||
             fs_hz_ == 48000);

  if (output->Channels() != 1) {
    RTC_LOG(LS_ERROR) << "No multi-channel support";
    return kMultiChannelNotSupported;
  }

  size_t number_of_samples = requested_length;
  bool new_period = false;
  if (first_call_) {

    number_of_samples = requested_length + overlap_length_;
    new_period = true;
  }
  output->AssertSize(number_of_samples);

  ComfortNoiseDecoder* cng_decoder = decoder_database_->GetActiveCngDecoder();
  if (!cng_decoder) {
    RTC_LOG(LS_ERROR) << "Unknwown payload type";
    return kUnknownPayloadType;
  }

  std::unique_ptr<int16_t[]> temp(new int16_t[number_of_samples]);
  if (!cng_decoder->Generate(
          rtc::ArrayView<int16_t>(temp.get(), number_of_samples), new_period)) {

    output->Zeros(requested_length);
    RTC_LOG(LS_ERROR)
        << "ComfortNoiseDecoder::Genererate failed to generate comfort noise";
    return kInternalError;
  }
  (*output)[0].OverwriteAt(temp.get(), number_of_samples, 0);

  if (first_call_) {

    int16_t muting_window;
    int16_t muting_window_increment;
    int16_t unmuting_window;
    int16_t unmuting_window_increment;
    if (fs_hz_ == 8000) {
      muting_window = DspHelper::kMuteFactorStart8kHz;
      muting_window_increment = DspHelper::kMuteFactorIncrement8kHz;
      unmuting_window = DspHelper::kUnmuteFactorStart8kHz;
      unmuting_window_increment = DspHelper::kUnmuteFactorIncrement8kHz;
    } else if (fs_hz_ == 16000) {
      muting_window = DspHelper::kMuteFactorStart16kHz;
      muting_window_increment = DspHelper::kMuteFactorIncrement16kHz;
      unmuting_window = DspHelper::kUnmuteFactorStart16kHz;
      unmuting_window_increment = DspHelper::kUnmuteFactorIncrement16kHz;
    } else if (fs_hz_ == 32000) {
      muting_window = DspHelper::kMuteFactorStart32kHz;
      muting_window_increment = DspHelper::kMuteFactorIncrement32kHz;
      unmuting_window = DspHelper::kUnmuteFactorStart32kHz;
      unmuting_window_increment = DspHelper::kUnmuteFactorIncrement32kHz;
    } else {
      muting_window = DspHelper::kMuteFactorStart48kHz;
      muting_window_increment = DspHelper::kMuteFactorIncrement48kHz;
      unmuting_window = DspHelper::kUnmuteFactorStart48kHz;
      unmuting_window_increment = DspHelper::kUnmuteFactorIncrement48kHz;
    }

    size_t start_ix = sync_buffer_->Size() - overlap_length_;
    for (size_t i = 0; i < overlap_length_; i++) {

      (*sync_buffer_)[0][start_ix + i] =
          (((*sync_buffer_)[0][start_ix + i] * muting_window) +
           ((*output)[0][i] * unmuting_window) + 16384) >>
          15;
      muting_window += muting_window_increment;
      unmuting_window += unmuting_window_increment;
    }

    output->PopFront(overlap_length_);
  }
  first_call_ = false;
  return kOK;
}

}
