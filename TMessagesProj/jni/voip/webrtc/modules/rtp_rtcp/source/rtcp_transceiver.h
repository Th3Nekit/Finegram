/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_config.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_impl.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class RtcpTransceiver : public RtcpFeedbackSenderInterface {
 public:
  explicit RtcpTransceiver(const RtcpTransceiverConfig& config);
  RtcpTransceiver(const RtcpTransceiver&) = delete;
  RtcpTransceiver& operator=(const RtcpTransceiver&) = delete;

  ~RtcpTransceiver() override;

  void Stop(absl::AnyInvocable<void() &&> on_destroyed);

  void AddMediaReceiverRtcpObserver(uint32_t remote_ssrc,
                                    MediaReceiverRtcpObserver* observer);

  void RemoveMediaReceiverRtcpObserver(
      uint32_t remote_ssrc,
      MediaReceiverRtcpObserver* observer,
      absl::AnyInvocable<void() &&> on_removed);

  void SetReadyToSend(bool ready);

  void ReceivePacket(rtc::CopyOnWriteBuffer packet);

  void SendCompoundPacket();

  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override;

  void UnsetRemb() override;

  void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) override;

  void SendNack(uint32_t ssrc, std::vector<uint16_t> sequence_numbers);

  void SendPictureLossIndication(uint32_t ssrc);

  void SendFullIntraRequest(std::vector<uint32_t> ssrcs);

  void SendFullIntraRequest(std::vector<uint32_t> ssrcs, bool new_request);

 private:
  Clock* const clock_;
  TaskQueueBase* const task_queue_;
  std::unique_ptr<RtcpTransceiverImpl> rtcp_transceiver_;
};

}

#endif
