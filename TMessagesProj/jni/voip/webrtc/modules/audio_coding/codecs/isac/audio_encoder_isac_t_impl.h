/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_ENCODER_ISAC_T_IMPL_H_

#include "modules/audio_coding/codecs/isac/audio_encoder_isac_t.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

template <typename T>
bool AudioEncoderIsacT<T>::Config::IsOk() const {
  if (max_bit_rate < 32000 && max_bit_rate != -1)
    return false;
  if (max_payload_size_bytes < 120 && max_payload_size_bytes != -1)
    return false;

  switch (sample_rate_hz) {
    case 16000:
      if (max_bit_rate > 53400)
        return false;
      if (max_payload_size_bytes > 400)
        return false;
      return (frame_size_ms == 30 || frame_size_ms == 60) &&
             (bit_rate == 0 || (bit_rate >= 10000 && bit_rate <= 32000));
    case 32000:
      if (max_bit_rate > 160000)
        return false;
      if (max_payload_size_bytes > 600)
        return false;
      return T::has_swb &&
             (frame_size_ms == 30 &&
              (bit_rate == 0 || (bit_rate >= 10000 && bit_rate <= 56000)));
    default:
      return false;
  }
}

template <typename T>
AudioEncoderIsacT<T>::AudioEncoderIsacT(const Config& config) {
  RecreateEncoderInstance(config);
}

template <typename T>
AudioEncoderIsacT<T>::~AudioEncoderIsacT() {
  RTC_CHECK_EQ(0, T::Free(isac_state_));
}

template <typename T>
int AudioEncoderIsacT<T>::SampleRateHz() const {
  return T::EncSampRate(isac_state_);
}

template <typename T>
size_t AudioEncoderIsacT<T>::NumChannels() const {
  return 1;
}

template <typename T>
size_t AudioEncoderIsacT<T>::Num10MsFramesInNextPacket() const {
  const int samples_in_next_packet = T::GetNewFrameLen(isac_state_);
  return static_cast<size_t>(rtc::CheckedDivExact(
      samples_in_next_packet, rtc::CheckedDivExact(SampleRateHz(), 100)));
}

template <typename T>
size_t AudioEncoderIsacT<T>::Max10MsFramesInAPacket() const {
  return 6;
}

template <typename T>
int AudioEncoderIsacT<T>::GetTargetBitrate() const {
  return config_.bit_rate == 0 ? kDefaultBitRate : config_.bit_rate;
}

template <typename T>
void AudioEncoderIsacT<T>::SetTargetBitrate(int target_bps) {

  SetTargetBitrate(target_bps,
                                                    false);
}

template <typename T>
void AudioEncoderIsacT<T>::OnReceivedTargetAudioBitrate(int target_bps) {

  SetTargetBitrate(target_bps,
                                                    false);
}

template <typename T>
void AudioEncoderIsacT<T>::OnReceivedUplinkBandwidth(
    int target_audio_bitrate_bps,
    absl::optional<int64_t>                  ) {

  SetTargetBitrate(
      target_audio_bitrate_bps,
                                       send_side_bwe_with_overhead_);
}

template <typename T>
void AudioEncoderIsacT<T>::OnReceivedUplinkAllocation(
    BitrateAllocationUpdate update) {

  SetTargetBitrate(
      update.target_bitrate.bps<int>(),
                                       send_side_bwe_with_overhead_);
}

template <typename T>
void AudioEncoderIsacT<T>::OnReceivedOverhead(
    size_t overhead_bytes_per_packet) {
  overhead_per_packet_ = DataSize::Bytes(overhead_bytes_per_packet);
}

template <typename T>
AudioEncoder::EncodedInfo AudioEncoderIsacT<T>::EncodeImpl(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    rtc::Buffer* encoded) {
  if (!packet_in_progress_) {

    packet_in_progress_ = true;
    packet_timestamp_ = rtp_timestamp;
  }
  size_t encoded_bytes = encoded->AppendData(
      kSufficientEncodeBufferSizeBytes, [&](rtc::ArrayView<uint8_t> encoded) {
        int r = T::Encode(isac_state_, audio.data(), encoded.data());

        if (T::GetErrorCode(isac_state_) == 6450) {

          r = 0;
        }
        RTC_CHECK_GE(r, 0) << "Encode failed (error code "
                           << T::GetErrorCode(isac_state_) << ")";

        return static_cast<size_t>(r);
      });

  if (encoded_bytes == 0)
    return EncodedInfo();

  packet_in_progress_ = false;
  EncodedInfo info;
  info.encoded_bytes = encoded_bytes;
  info.encoded_timestamp = packet_timestamp_;
  info.payload_type = config_.payload_type;
  info.encoder_type = CodecType::kIsac;
  return info;
}

template <typename T>
void AudioEncoderIsacT<T>::Reset() {
  RecreateEncoderInstance(config_);
}

template <typename T>
absl::optional<std::pair<TimeDelta, TimeDelta>>
AudioEncoderIsacT<T>::GetFrameLengthRange() const {
  return {{TimeDelta::Millis(config_.frame_size_ms),
           TimeDelta::Millis(config_.frame_size_ms)}};
}

template <typename T>
void AudioEncoderIsacT<T>::SetTargetBitrate(int target_bps,
                                            bool subtract_per_packet_overhead) {
  if (subtract_per_packet_overhead) {
    const DataRate overhead_rate =
        overhead_per_packet_ / TimeDelta::Millis(config_.frame_size_ms);
    target_bps -= overhead_rate.bps();
  }
  target_bps = rtc::SafeClamp(target_bps, kMinBitrateBps,
                              MaxBitrateBps(config_.sample_rate_hz));
  int result = T::Control(isac_state_, target_bps, config_.frame_size_ms);
  RTC_DCHECK_EQ(result, 0);
  config_.bit_rate = target_bps;
}

template <typename T>
void AudioEncoderIsacT<T>::RecreateEncoderInstance(const Config& config) {
  RTC_CHECK(config.IsOk());
  packet_in_progress_ = false;
  if (isac_state_)
    RTC_CHECK_EQ(0, T::Free(isac_state_));
  RTC_CHECK_EQ(0, T::Create(&isac_state_));
  RTC_CHECK_EQ(0, T::EncoderInit(isac_state_,                 1));
  RTC_CHECK_EQ(0, T::SetEncSampRate(isac_state_, config.sample_rate_hz));
  const int bit_rate = config.bit_rate == 0 ? kDefaultBitRate : config.bit_rate;
  RTC_CHECK_EQ(0, T::Control(isac_state_, bit_rate, config.frame_size_ms));

  if (config.max_payload_size_bytes != -1)
    RTC_CHECK_EQ(
        0, T::SetMaxPayloadSize(isac_state_, config.max_payload_size_bytes));
  if (config.max_bit_rate != -1)
    RTC_CHECK_EQ(0, T::SetMaxRate(isac_state_, config.max_bit_rate));

  RTC_CHECK_EQ(0, T::SetDecSampRate(isac_state_, config.sample_rate_hz));

  config_ = config;
}

}

#endif
