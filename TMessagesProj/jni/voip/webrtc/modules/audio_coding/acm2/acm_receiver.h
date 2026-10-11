/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_ACM_RECEIVER_H_
#define MODULES_AUDIO_CODING_ACM2_ACM_RECEIVER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/neteq/neteq.h"
#include "api/neteq/neteq_factory.h"
#include "modules/audio_coding/acm2/acm_resampler.h"
#include "modules/audio_coding/acm2/call_statistics.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;
class NetEq;
struct RTPHeader;

namespace acm2 {

class AcmReceiver {
 public:
  struct Config {
    explicit Config(
        rtc::scoped_refptr<AudioDecoderFactory> decoder_factory = nullptr);
    Config(const Config&);
    ~Config();

    NetEq::Config neteq_config;
    Clock& clock;
    rtc::scoped_refptr<AudioDecoderFactory> decoder_factory;
    NetEqFactory* neteq_factory = nullptr;
  };

  explicit AcmReceiver(const Config& config);

  ~AcmReceiver();

  int InsertPacket(const RTPHeader& rtp_header,
                   rtc::ArrayView<const uint8_t> incoming_payload);

  int GetAudio(int desired_freq_hz, AudioFrame* audio_frame, bool* muted);

  void SetCodecs(const std::map<int, SdpAudioFormat>& codecs);

  int SetMinimumDelay(int delay_ms);

  int SetMaximumDelay(int delay_ms);

  bool SetBaseMinimumDelayMs(int delay_ms);

  int GetBaseMinimumDelayMs() const;

  void ResetInitialDelay();

  absl::optional<int> last_packet_sample_rate_hz() const;

  int last_output_sample_rate_hz() const;

  void GetNetworkStatistics(NetworkStatistics* statistics,
                            bool get_and_clear_legacy_stats = true) const;

  void FlushBuffers();

  void RemoveAllCodecs();

  absl::optional<uint32_t> GetPlayoutTimestamp();

  int FilteredCurrentDelayMs() const;

  int TargetDelayMs() const;

  absl::optional<std::pair<int, SdpAudioFormat>> LastDecoder() const;

  int EnableNack(size_t max_nack_list_size);

  void DisableNack();

  std::vector<uint16_t> GetNackList(int64_t round_trip_time_ms) const;

  void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const;

 private:
  struct DecoderInfo {
    int payload_type;
    int sample_rate_hz;
    int num_channels;
    SdpAudioFormat sdp_format;
  };

  uint32_t NowInTimestamp(int decoder_sampling_rate) const;

  mutable Mutex mutex_;
  absl::optional<DecoderInfo> last_decoder_ RTC_GUARDED_BY(mutex_);
  ACMResampler resampler_ RTC_GUARDED_BY(mutex_);
  std::unique_ptr<int16_t[]> last_audio_buffer_ RTC_GUARDED_BY(mutex_);
  CallStatistics call_stats_ RTC_GUARDED_BY(mutex_);
  const std::unique_ptr<NetEq> neteq_;
  Clock& clock_;
  bool resampled_last_output_frame_ RTC_GUARDED_BY(mutex_);
};

}

}

#endif
