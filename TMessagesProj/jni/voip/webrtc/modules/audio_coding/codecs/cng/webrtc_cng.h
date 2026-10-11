/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_CNG_WEBRTC_CNG_H_
#define MODULES_AUDIO_CODING_CODECS_CNG_WEBRTC_CNG_H_

#include <stdint.h>

#include <cstddef>

#include "api/array_view.h"
#include "rtc_base/buffer.h"

#define WEBRTC_CNG_MAX_LPC_ORDER 12

namespace webrtc {

class ComfortNoiseDecoder {
 public:
  ComfortNoiseDecoder();
  ~ComfortNoiseDecoder() = default;

  ComfortNoiseDecoder(const ComfortNoiseDecoder&) = delete;
  ComfortNoiseDecoder& operator=(const ComfortNoiseDecoder&) = delete;

  void Reset();

  void UpdateSid(rtc::ArrayView<const uint8_t> sid);

  bool Generate(rtc::ArrayView<int16_t> out_data, bool new_period);

 private:
  uint32_t dec_seed_;
  int32_t dec_target_energy_;
  int32_t dec_used_energy_;
  int16_t dec_target_reflCoefs_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  int16_t dec_used_reflCoefs_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  int16_t dec_filtstate_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  int16_t dec_filtstateLow_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  uint16_t dec_order_;
  int16_t dec_target_scale_factor_;
  int16_t dec_used_scale_factor_;
};

class ComfortNoiseEncoder {
 public:

  ComfortNoiseEncoder(int fs, int interval, int quality);
  ~ComfortNoiseEncoder() = default;

  ComfortNoiseEncoder(const ComfortNoiseEncoder&) = delete;
  ComfortNoiseEncoder& operator=(const ComfortNoiseEncoder&) = delete;

  void Reset(int fs, int interval, int quality);

  size_t Encode(rtc::ArrayView<const int16_t> speech,
                bool force_sid,
                rtc::Buffer* output);

 private:
  size_t enc_nrOfCoefs_;
  int enc_sampfreq_;
  int16_t enc_interval_;
  int16_t enc_msSinceSid_;
  int32_t enc_Energy_;
  int16_t enc_reflCoefs_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  int32_t enc_corrVector_[WEBRTC_CNG_MAX_LPC_ORDER + 1];
  uint32_t enc_seed_;
};

}

#endif
