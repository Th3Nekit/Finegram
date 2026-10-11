/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_FEC_TEST_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_FEC_TEST_HELPER_H_

#include <memory>

#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/random.h"

namespace webrtc {
namespace test {
namespace fec {

struct AugmentedPacket : public ForwardErrorCorrection::Packet {
  RTPHeader header;
};

class MediaPacketGenerator {
 public:
  MediaPacketGenerator(uint32_t min_packet_size,
                       uint32_t max_packet_size,
                       uint32_t ssrc,
                       Random* random);
  ~MediaPacketGenerator();

  ForwardErrorCorrection::PacketList ConstructMediaPackets(
      int num_media_packets,
      uint16_t start_seq_num);
  ForwardErrorCorrection::PacketList ConstructMediaPackets(
      int num_media_packets);

  uint16_t GetNextSeqNum();

 private:
  uint32_t min_packet_size_;
  uint32_t max_packet_size_;
  uint32_t ssrc_;
  Random* random_;

  ForwardErrorCorrection::PacketList media_packets_;
  uint16_t next_seq_num_;
};

class AugmentedPacketGenerator {
 public:
  explicit AugmentedPacketGenerator(uint32_t ssrc);

  void NewFrame(size_t num_packets);

  uint16_t NextPacketSeqNum();

  std::unique_ptr<AugmentedPacket> NextPacket(size_t offset, size_t length);

 protected:

  static void WriteRtpHeader(const RTPHeader& header, uint8_t* data);

  size_t num_packets_;

 private:
  uint32_t ssrc_;
  uint16_t seq_num_;
  uint32_t timestamp_;
};

class FlexfecPacketGenerator : public AugmentedPacketGenerator {
 public:
  FlexfecPacketGenerator(uint32_t media_ssrc, uint32_t flexfec_ssrc);

  std::unique_ptr<AugmentedPacket> BuildFlexfecPacket(
      const ForwardErrorCorrection::Packet& packet);

 private:
  uint32_t flexfec_ssrc_;
  uint16_t flexfec_seq_num_;
  uint32_t flexfec_timestamp_;
};

class UlpfecPacketGenerator : public AugmentedPacketGenerator {
 public:
  explicit UlpfecPacketGenerator(uint32_t ssrc);

  static RtpPacketReceived BuildMediaRedPacket(const AugmentedPacket& packet,
                                               bool is_recovered);

  RtpPacketReceived BuildUlpfecRedPacket(
      const ForwardErrorCorrection::Packet& packet);
};

}
}
}

#endif
