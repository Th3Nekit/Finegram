/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_H_

#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

template <typename T>
class AudioEncoderIsacT final : public AudioEncoder {
 public:

  struct Config {
    bool IsOk() const;
    int payload_type = 103;
    int sample_rate_hz = 16000;
    int frame_size_ms = 30;
    int bit_rate = kDefaultBitRate;

    int max_payload_size_bytes = -1;
    int max_bit_rate = -1;
  };

  explicit AudioEncoderIsacT(const Config& config);
  ~AudioEncoderIsacT() override;

  AudioEncoderIsacT(const AudioEncoderIsacT&) = delete;
  AudioEncoderIsacT& operator=(const AudioEncoderIsacT&) = delete;

  int SampleRateHz() const override;
  size_t NumChannels() const override;
  size_t Num10MsFramesInNextPacket() const override;
  size_t Max10MsFramesInAPacket() const override;
  int GetTargetBitrate() const override;
  void SetTargetBitrate(int target_bps) override;
  void OnReceivedTargetAudioBitrate(int target_bps) override;
  void OnReceivedUplinkBandwidth(
      int target_audio_bitrate_bps,
      absl::optional<int64_t> bwe_period_ms) override;
  void OnReceivedUplinkAllocation(BitrateAllocationUpdate update) override;
  void OnReceivedOverhead(size_t overhead_bytes_per_packet) override;
  EncodedInfo EncodeImpl(uint32_t rtp_timestamp,
                         rtc::ArrayView<const int16_t> audio,
                         rtc::Buffer* encoded) override;
  void Reset() override;
  absl::optional<std::pair<TimeDelta, TimeDelta>> GetFrameLengthRange()
      const override;

 private:

  static const size_t kSufficientEncodeBufferSizeBytes = 400;

  static constexpr int kDefaultBitRate = 32000;
  static constexpr int kMinBitrateBps = 10000;
  static constexpr int MaxBitrateBps(int sample_rate_hz) {
    return sample_rate_hz == 32000 ? 56000 : 32000;
  }

  void SetTargetBitrate(int target_bps, bool subtract_per_packet_overhead);

  void RecreateEncoderInstance(const Config& config);

  Config config_;
  typename T::instance_type* isac_state_ = nullptr;

  bool packet_in_progress_ = false;

  uint32_t packet_timestamp_;

  uint32_t last_encoded_timestamp_;

  const bool send_side_bwe_with_overhead_ =
      !field_trial::IsDisabled("WebRTC-SendSideBwe-WithOverhead");

  DataSize overhead_per_packet_ = DataSize::Bytes(28);
};

}

#endif
