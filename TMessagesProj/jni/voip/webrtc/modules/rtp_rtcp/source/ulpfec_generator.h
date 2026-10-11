/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_
#define MODULES_RTP_RTCP_SOURCE_ULPFEC_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/video_fec_generator.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class FlexfecSender;

class UlpfecGenerator : public VideoFecGenerator {
  friend class FlexfecSender;

 public:
  UlpfecGenerator(int red_payload_type, int ulpfec_payload_type, Clock* clock);
  ~UlpfecGenerator();

  FecType GetFecType() const override {
    return VideoFecGenerator::FecType::kUlpFec;
  }
  absl::optional<uint32_t> FecSsrc() override { return absl::nullopt; }

  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override;

  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override;

  size_t MaxPacketOverhead() const override;

  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override;

  DataRate CurrentFecRate() const override;

  absl::optional<RtpState> GetRtpState() override { return absl::nullopt; }

  const FecProtectionParams& CurrentParams() const;

 private:
  struct Params {
    Params();
    Params(FecProtectionParams delta_params,
           FecProtectionParams keyframe_params);

    FecProtectionParams delta_params;
    FecProtectionParams keyframe_params;
  };

  UlpfecGenerator(std::unique_ptr<ForwardErrorCorrection> fec, Clock* clock);

  int Overhead() const;

  bool ExcessOverheadBelowMax() const;

  bool MinimumMediaPacketsReached() const;

  void ResetState();

  const int red_payload_type_;
  const int ulpfec_payload_type_;
  Clock* const clock_;

  rtc::RaceChecker race_checker_;
  const std::unique_ptr<ForwardErrorCorrection> fec_
      RTC_GUARDED_BY(race_checker_);
  ForwardErrorCorrection::PacketList media_packets_
      RTC_GUARDED_BY(race_checker_);
  absl::optional<RtpPacketToSend> last_media_packet_
      RTC_GUARDED_BY(race_checker_);
  std::list<ForwardErrorCorrection::Packet*> generated_fec_packets_
      RTC_GUARDED_BY(race_checker_);
  int num_protected_frames_ RTC_GUARDED_BY(race_checker_);
  int min_num_media_packets_ RTC_GUARDED_BY(race_checker_);
  Params current_params_ RTC_GUARDED_BY(race_checker_);
  bool media_contains_keyframe_ RTC_GUARDED_BY(race_checker_);

  mutable Mutex mutex_;
  absl::optional<Params> pending_params_ RTC_GUARDED_BY(mutex_);
  BitrateTracker fec_bitrate_ RTC_GUARDED_BY(mutex_);
};

}

#endif
