/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_PACKET_BUFFER_H_
#define MODULES_VIDEO_CODING_PACKET_BUFFER_H_

#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "absl/base/attributes.h"
#include "api/rtp_packet_info.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
namespace video_coding {

class PacketBuffer {
 public:
  struct Packet {
    Packet() = default;
    Packet(const RtpPacketReceived& rtp_packet,
           const RTPVideoHeader& video_header);
    Packet(const Packet&) = delete;
    Packet(Packet&&) = delete;
    Packet& operator=(const Packet&) = delete;
    Packet& operator=(Packet&&) = delete;
    ~Packet() = default;

    VideoCodecType codec() const { return video_header.codec; }
    int width() const { return video_header.width; }
    int height() const { return video_header.height; }

    bool is_first_packet_in_frame() const {
      return video_header.is_first_packet_in_frame;
    }
    bool is_last_packet_in_frame() const {
      return video_header.is_last_packet_in_frame;
    }

    bool continuous = false;
    bool marker_bit = false;
    uint8_t payload_type = 0;
    uint16_t seq_num = 0;
    uint32_t timestamp = 0;
    int times_nacked = -1;

    rtc::CopyOnWriteBuffer video_payload;
    RTPVideoHeader video_header;
  };
  struct InsertResult {
    std::vector<std::unique_ptr<Packet>> packets;

    bool buffer_cleared = false;
  };

  PacketBuffer(size_t start_buffer_size, size_t max_buffer_size);
  ~PacketBuffer();

  ABSL_MUST_USE_RESULT InsertResult
  InsertPacket(std::unique_ptr<Packet> packet);
  ABSL_MUST_USE_RESULT InsertResult InsertPadding(uint16_t seq_num);
  void ClearTo(uint16_t seq_num);
  void Clear();

  void ForceSpsPpsIdrIsH264Keyframe();
  void ResetSpsPpsIdrIsH264Keyframe();

 private:
  void ClearInternal();

  bool ExpandBufferSize();

  bool PotentialNewFrame(uint16_t seq_num) const;

  std::vector<std::unique_ptr<Packet>> FindFrames(uint16_t seq_num);

  void UpdateMissingPackets(uint16_t seq_num);

  const size_t max_size_;

  uint16_t first_seq_num_;

  bool first_packet_received_;

  bool is_cleared_to_first_seq_num_;

  std::vector<std::unique_ptr<Packet>> buffer_;

  absl::optional<uint16_t> newest_inserted_seq_num_;
  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> missing_packets_;

  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> received_padding_;

  bool sps_pps_idr_is_h264_keyframe_;
};

}
}

#endif
