/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTX_RECEIVE_STREAM_H_
#define CALL_RTX_RECEIVE_STREAM_H_

#include <cstdint>
#include <map>

#include "api/sequence_checker.h"
#include "call/rtp_packet_sink_interface.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

class ReceiveStatistics;

class RtxReceiveStream : public RtpPacketSinkInterface {
 public:
  RtxReceiveStream(RtpPacketSinkInterface* media_sink,
                   std::map<int, int> associated_payload_types,
                   uint32_t media_ssrc,

                   ReceiveStatistics* rtp_receive_statistics = nullptr);
  ~RtxReceiveStream() override;

  void SetAssociatedPayloadTypes(std::map<int, int> associated_payload_types);

  void OnRtpPacket(const RtpPacketReceived& packet) override;

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_checker_;
  RtpPacketSinkInterface* const media_sink_;

  std::map<int, int> associated_payload_types_ RTC_GUARDED_BY(&packet_checker_);

  const uint32_t media_ssrc_;
  ReceiveStatistics* const rtp_receive_statistics_;
};

}

#endif
