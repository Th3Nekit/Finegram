/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_FLEXFEC_SENDER_H_
#define MODULES_RTP_RTCP_INCLUDE_FLEXFEC_SENDER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtp_parameters.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extension_size.h"
#include "modules/rtp_rtcp/source/ulpfec_generator.h"
#include "modules/rtp_rtcp/source/video_fec_generator.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/random.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class Clock;
class RtpPacketToSend;

class FlexfecSender : public VideoFecGenerator {
 public:
  FlexfecSender(int payload_type,
                uint32_t ssrc,
                uint32_t protected_media_ssrc,
                absl::string_view mid,
                const std::vector<RtpExtension>& rtp_header_extensions,
                rtc::ArrayView<const RtpExtensionSize> extension_sizes,
                const RtpState* rtp_state,
                Clock* clock);
  ~FlexfecSender();

  FecType GetFecType() const override {
    return VideoFecGenerator::FecType::kFlexFec;
  }
  absl::optional<uint32_t> FecSsrc() override { return ssrc_; }

  void SetProtectionParameters(const FecProtectionParams& delta_params,
                               const FecProtectionParams& key_params) override;

  void AddPacketAndGenerateFec(const RtpPacketToSend& packet) override;

  std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() override;

  size_t MaxPacketOverhead() const override;

  DataRate CurrentFecRate() const override;

  absl::optional<RtpState> GetRtpState() override;

 private:

  Clock* const clock_;
  Random random_;
  Timestamp last_generated_packet_ = Timestamp::MinusInfinity();

  const int payload_type_;
  const uint32_t timestamp_offset_;
  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  const std::string mid_;

  uint16_t seq_num_;

  UlpfecGenerator ulpfec_generator_;
  const RtpHeaderExtensionMap rtp_header_extension_map_;
  const size_t header_extensions_size_;

  mutable Mutex mutex_;
  BitrateTracker fec_bitrate_ RTC_GUARDED_BY(mutex_);
};

}

#endif
