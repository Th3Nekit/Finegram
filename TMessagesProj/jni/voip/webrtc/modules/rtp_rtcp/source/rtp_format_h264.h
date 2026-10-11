/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <queue>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "rtc_base/buffer.h"

namespace webrtc {

constexpr uint8_t kH264FBit = 0x80;
constexpr uint8_t kH264NriMask = 0x60;
constexpr uint8_t kH264TypeMask = 0x1F;

constexpr uint8_t kH264SBit = 0x80;
constexpr uint8_t kH264EBit = 0x40;
constexpr uint8_t kH264RBit = 0x20;

class RtpPacketizerH264 : public RtpPacketizer {
 public:

  RtpPacketizerH264(rtc::ArrayView<const uint8_t> payload,
                    PayloadSizeLimits limits,
                    H264PacketizationMode packetization_mode);

  ~RtpPacketizerH264() override;

  RtpPacketizerH264(const RtpPacketizerH264&) = delete;
  RtpPacketizerH264& operator=(const RtpPacketizerH264&) = delete;

  size_t NumPackets() const override;

  bool NextPacket(RtpPacketToSend* rtp_packet) override;

 private:

  struct PacketUnit {
    PacketUnit(rtc::ArrayView<const uint8_t> source_fragment,
               bool first_fragment,
               bool last_fragment,
               bool aggregated,
               uint8_t header)
        : source_fragment(source_fragment),
          first_fragment(first_fragment),
          last_fragment(last_fragment),
          aggregated(aggregated),
          header(header) {}

    rtc::ArrayView<const uint8_t> source_fragment;
    bool first_fragment;
    bool last_fragment;
    bool aggregated;
    uint8_t header;
  };

  bool GeneratePackets(H264PacketizationMode packetization_mode);
  bool PacketizeFuA(size_t fragment_index);
  size_t PacketizeStapA(size_t fragment_index);
  bool PacketizeSingleNalu(size_t fragment_index);

  void NextAggregatePacket(RtpPacketToSend* rtp_packet);
  void NextFragmentPacket(RtpPacketToSend* rtp_packet);

  const PayloadSizeLimits limits_;
  size_t num_packets_left_;
  std::deque<rtc::ArrayView<const uint8_t>> input_fragments_;
  std::queue<PacketUnit> packets_;
};
}
#endif
