/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_VOIP_AUDIO_INGRESS_H_
#define AUDIO_VOIP_AUDIO_INGRESS_H_

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/audio_mixer.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/voip/voip_statistics.h"
#include "audio/audio_level.h"
#include "modules/audio_coding/acm2/acm_receiver.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class AudioIngress : public AudioMixer::Source {
 public:
  AudioIngress(RtpRtcpInterface* rtp_rtcp,
               Clock* clock,
               ReceiveStatistics* receive_statistics,
               rtc::scoped_refptr<AudioDecoderFactory> decoder_factory);
  ~AudioIngress() override;

  bool StartPlay();
  void StopPlay() {
    playing_ = false;
    output_audio_level_.ResetLevelFullRange();
  }

  bool IsPlaying() const { return playing_; }

  void SetReceiveCodecs(const std::map<int, SdpAudioFormat>& codecs);

  void ReceivedRTPPacket(rtc::ArrayView<const uint8_t> rtp_packet);
  void ReceivedRTCPPacket(rtc::ArrayView<const uint8_t> rtcp_packet);

  int GetOutputAudioLevel() const {
    return output_audio_level_.LevelFullRange();
  }
  double GetOutputTotalEnergy() { return output_audio_level_.TotalEnergy(); }
  double GetOutputTotalDuration() {
    return output_audio_level_.TotalDuration();
  }

  NetworkStatistics GetNetworkStatistics() const {
    NetworkStatistics stats;
    acm_receiver_.GetNetworkStatistics(&stats,
                                                                      false);
    return stats;
  }

  ChannelStatistics GetChannelStatistics();

  AudioMixer::Source::AudioFrameInfo GetAudioFrameWithInfo(
      int sampling_rate,
      AudioFrame* audio_frame) override;
  int Ssrc() const override {
    return rtc::dchecked_cast<int>(remote_ssrc_.load());
  }
  int PreferredSampleRate() const override {

    return std::max(acm_receiver_.last_packet_sample_rate_hz().value_or(0),
                    acm_receiver_.last_output_sample_rate_hz());
  }

 private:

  std::atomic<bool> playing_;

  std::atomic<uint32_t> remote_ssrc_;

  std::atomic<int64_t> first_rtp_timestamp_;

  ReceiveStatistics* const rtp_receive_statistics_;

  RtpRtcpInterface* const rtp_rtcp_;

  acm2::AcmReceiver acm_receiver_;

  voe::AudioLevel output_audio_level_;

  Mutex lock_;

  RemoteNtpTimeEstimator ntp_estimator_ RTC_GUARDED_BY(lock_);

  std::map<int, int> receive_codec_info_ RTC_GUARDED_BY(lock_);

  RtpTimestampUnwrapper timestamp_wrap_handler_ RTC_GUARDED_BY(lock_);
};

}

#endif
