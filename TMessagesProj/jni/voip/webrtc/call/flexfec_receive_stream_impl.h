/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_
#define CALL_FLEXFEC_RECEIVE_STREAM_IMPL_H_

#include <memory>
#include <vector>

#include "call/flexfec_receive_stream.h"
#include "call/rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "rtc_base/system/no_unique_address.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class FlexfecReceiver;
class ReceiveStatistics;
class RecoveredPacketReceiver;
class RtcpRttStats;
class RtpPacketReceived;
class RtpRtcp;
class RtpStreamReceiverControllerInterface;
class RtpStreamReceiverInterface;

class FlexfecReceiveStreamImpl : public FlexfecReceiveStream {
 public:
  FlexfecReceiveStreamImpl(Clock* clock,
                           Config config,
                           RecoveredPacketReceiver* recovered_packet_receiver,
                           RtcpRttStats* rtt_stats);

  ~FlexfecReceiveStreamImpl() override;

  void RegisterWithTransport(
      RtpStreamReceiverControllerInterface* receiver_controller);

  void UnregisterFromTransport();

  void OnRtpPacket(const RtpPacketReceived& packet) override;

  void SetPayloadType(int payload_type) override;
  int payload_type() const override;

  void SetLocalSsrc(uint32_t local_ssrc);

  uint32_t remote_ssrc() const { return remote_ssrc_; }

  void SetRtcpMode(RtcpMode mode) override {
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    rtp_rtcp_->SetRTCPStatus(mode);
  }

  const ReceiveStatistics* GetStats() const override {
    return rtp_receive_statistics_.get();
  }

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_;

  const uint32_t remote_ssrc_;

  int payload_type_ RTC_GUARDED_BY(packet_sequence_checker_) = -1;

  const std::unique_ptr<FlexfecReceiver> receiver_;

  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
  const std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;

  std::unique_ptr<RtpStreamReceiverInterface> rtp_stream_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);
};

}

#endif
