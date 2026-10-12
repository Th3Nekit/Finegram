/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_
#define MODULES_RTP_RTCP_INCLUDE_FLEXFEC_RECEIVER_H_

#include <stdint.h>

#include <memory>

#include "api/sequence_checker.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/recovered_packet_receiver.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/ulpfec_receiver.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;

class FlexfecReceiver {
 public:
  FlexfecReceiver(uint32_t ssrc,
                  uint32_t protected_media_ssrc,
                  RecoveredPacketReceiver* recovered_packet_receiver);
  FlexfecReceiver(Clock* clock,
                  uint32_t ssrc,
                  uint32_t protected_media_ssrc,
                  RecoveredPacketReceiver* recovered_packet_receiver);
  ~FlexfecReceiver();

  void OnRtpPacket(const RtpPacketReceived& packet);

  FecPacketCounter GetPacketCounter() const;

 protected:
  std::unique_ptr<ForwardErrorCorrection::ReceivedPacket> AddReceivedPacket(
      const RtpPacketReceived& packet);
  void ProcessReceivedPacket(
      const ForwardErrorCorrection::ReceivedPacket& received_packet);

 private:

  const uint32_t ssrc_;
  const uint32_t protected_media_ssrc_;

  std::unique_ptr<ForwardErrorCorrection> erasure_code_
      RTC_GUARDED_BY(sequence_checker_);
  ForwardErrorCorrection::RecoveredPacketList recovered_packets_
      RTC_GUARDED_BY(sequence_checker_);
  RecoveredPacketReceiver* const recovered_packet_receiver_;

  Clock* const clock_;
  Timestamp last_recovered_packet_ RTC_GUARDED_BY(sequence_checker_) =
      Timestamp::MinusInfinity();
  FecPacketCounter packet_counter_ RTC_GUARDED_BY(sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}

#endif
