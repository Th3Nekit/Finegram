/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/deprecated/decoding_state.h"

#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "modules/video_coding/deprecated/frame_buffer.h"
#include "modules/video_coding/deprecated/packet.h"
#include "modules/video_coding/deprecated/session_info.h"
#include "test/gtest.h"

namespace webrtc {

TEST(TestDecodingState, Sanity) {
  VCMDecodingState dec_state;
  dec_state.Reset();
  EXPECT_TRUE(dec_state.in_initial_state());
  EXPECT_TRUE(dec_state.full_sync());
}

TEST(TestDecodingState, FrameContinuity) {
  VCMDecodingState dec_state;

  VCMFrameBuffer frame;
  VCMFrameBuffer frame_key;
  VCMPacket packet;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 1;
  packet.seqNum = 0xffff;
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.codec = kVideoCodecVP8;
  auto& vp8_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.pictureId = 0x007F;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));

  dec_state.Reset();
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_LE(0, frame_key.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame_key));
  dec_state.SetState(&frame);
  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;

  packet.video_header.is_first_packet_in_frame = false;
  vp8_header.pictureId = 0x0002;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  frame.Reset();
  vp8_header.pictureId = 0;
  packet.seqNum = 10;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));

  vp8_header.pictureId = kNoPictureId;
  frame.Reset();
  packet.seqNum = dec_state.sequence_num() - 1u;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  frame.Reset();
  packet.seqNum = dec_state.sequence_num() + 1u;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));

  packet.seqNum++;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));

  EXPECT_LE(dec_state.sequence_num(), 0xffff);
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  dec_state.Reset();
  frame.Reset();
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 0;
  packet.seqNum = 1;
  packet.timestamp = 1;
  EXPECT_TRUE(dec_state.full_sync());
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();

  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 1;
  vp8_header.pictureId = 1;
  packet.seqNum = 2;
  packet.timestamp = 2;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();

  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 3;
  vp8_header.pictureId = 3;
  packet.seqNum = 4;
  packet.timestamp = 4;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  vp8_header.tl0PicIdx = 1;
  vp8_header.temporalIdx = 2;
  vp8_header.pictureId = 4;
  packet.seqNum = 5;
  packet.timestamp = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));

  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  vp8_header.tl0PicIdx = 1;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 5;
  packet.seqNum = 6;
  packet.timestamp = 6;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  frame.Reset();
  vp8_header.tl0PicIdx = 0x00FF;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 6;
  packet.seqNum = 7;
  packet.timestamp = 7;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
  frame.Reset();
  vp8_header.tl0PicIdx = 0x0000;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 7;
  packet.seqNum = 8;
  packet.timestamp = 8;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));

  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
}

TEST(TestDecodingState, UpdateOldPacket) {
  VCMDecodingState dec_state;

  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.timestamp = 1;
  packet.seqNum = 1;
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_EQ(dec_state.sequence_num(), 1);

  packet.timestamp = 2;
  dec_state.UpdateOldPacket(&packet);
  EXPECT_EQ(dec_state.sequence_num(), 1);

  packet.timestamp = 1;
  packet.seqNum = 2;
  packet.video_header.frame_type = VideoFrameType::kEmptyFrame;
  packet.sizeBytes = 0;
  dec_state.UpdateOldPacket(&packet);
  EXPECT_EQ(dec_state.sequence_num(), 2);

  packet.timestamp = 1;
  packet.seqNum = 3;
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.sizeBytes = 1400;
  dec_state.UpdateOldPacket(&packet);
  EXPECT_EQ(dec_state.sequence_num(), 3);

  packet.timestamp = 0;
  packet.seqNum = 4;
  packet.video_header.frame_type = VideoFrameType::kEmptyFrame;
  packet.sizeBytes = 0;
  dec_state.UpdateOldPacket(&packet);
  EXPECT_EQ(dec_state.sequence_num(), 3);
}

TEST(TestDecodingState, MultiLayerBehavior) {

  VCMDecodingState dec_state;

  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.codec = kVideoCodecVP8;
  packet.timestamp = 0;
  packet.seqNum = 0;
  auto& vp8_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 0;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.timestamp = 1;
  packet.seqNum = 1;
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 1;
  vp8_header.pictureId = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  packet.timestamp = 3;
  packet.seqNum = 3;
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 3;
  vp8_header.pictureId = 3;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  frame.Reset();
  packet.timestamp = 4;
  packet.seqNum = 4;
  vp8_header.tl0PicIdx = 1;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 4;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 5;
  packet.seqNum = 5;
  vp8_header.tl0PicIdx = 2;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.timestamp = 6;
  packet.seqNum = 6;
  vp8_header.tl0PicIdx = 3;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 6;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 8;
  packet.seqNum = 8;
  vp8_header.tl0PicIdx = 4;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 8;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 9;
  packet.seqNum = 9;
  vp8_header.tl0PicIdx = 4;
  vp8_header.temporalIdx = 2;
  vp8_header.pictureId = 9;
  vp8_header.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  dec_state.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.is_first_packet_in_frame = true;
  packet.markerBit = 1;
  packet.timestamp = 0;
  packet.seqNum = 0;
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 0;
  vp8_header.layerSync = false;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.is_first_packet_in_frame = true;
  packet.markerBit = 0;
  packet.timestamp = 1;
  packet.seqNum = 1;
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 2;
  vp8_header.pictureId = 1;
  vp8_header.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.is_first_packet_in_frame = true;
  packet.markerBit = 1;
  packet.timestamp = 2;
  packet.seqNum = 3;
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 1;
  vp8_header.pictureId = 2;
  vp8_header.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
}

TEST(TestDecodingState, DiscontinuousPicIdContinuousSeqNum) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  packet.video_header.codec = kVideoCodecVP8;
  packet.timestamp = 0;
  packet.seqNum = 0;
  auto& vp8_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 0;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.timestamp += 3000;
  ++packet.seqNum;
  vp8_header.temporalIdx = 1;
  vp8_header.pictureId = 2;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
}

TEST(TestDecodingState, OldInput) {
  VCMDecodingState dec_state;

  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.timestamp = 10;
  packet.seqNum = 1;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  packet.timestamp = 9;
  EXPECT_TRUE(dec_state.IsOldPacket(&packet));

  frame.Reset();
  frame.InsertPacket(packet, 0, frame_data);
  EXPECT_TRUE(dec_state.IsOldFrame(&frame));
}

TEST(TestDecodingState, PictureIdRepeat) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  packet.video_header.codec = kVideoCodecVP8;
  packet.timestamp = 0;
  packet.seqNum = 0;
  auto& vp8_header =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP8>();
  vp8_header.tl0PicIdx = 0;
  vp8_header.temporalIdx = 0;
  vp8_header.pictureId = 0;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);

  frame.Reset();
  ++packet.timestamp;
  ++packet.seqNum;
  vp8_header.temporalIdx++;
  vp8_header.pictureId++;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  frame.Reset();

  vp8_header.tl0PicIdx += 3;
  vp8_header.temporalIdx++;
  vp8_header.tl0PicIdx = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
}

TEST(TestDecodingState, FrameContinuityFlexibleModeKeyFrame) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 1;
  packet.seqNum = 0xffff;
  uint8_t data[] = "I need a data pointer for this test!";
  packet.sizeBytes = sizeof(data);
  packet.dataPtr = data;
  packet.video_header.codec = kVideoCodecVP9;

  auto& vp9_hdr =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
  vp9_hdr.picture_id = 10;
  vp9_hdr.flexible_mode = true;

  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;

  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  vp9_hdr.picture_id = 11;
  frame.Reset();
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  vp9_hdr.picture_id = 12;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
}

TEST(TestDecodingState, FrameContinuityFlexibleModeOutOfOrderFrames) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 1;
  packet.seqNum = 0xffff;
  uint8_t data[] = "I need a data pointer for this test!";
  packet.sizeBytes = sizeof(data);
  packet.dataPtr = data;
  packet.video_header.codec = kVideoCodecVP9;

  auto& vp9_hdr =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
  vp9_hdr.picture_id = 10;
  vp9_hdr.flexible_mode = true;

  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;

  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  vp9_hdr.picture_id = 15;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 12;
  vp9_hdr.pid_diff[0] = 2;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 20;
  vp9_hdr.num_ref_pics = 3;
  vp9_hdr.pid_diff[0] = 10;
  vp9_hdr.pid_diff[1] = 8;
  vp9_hdr.pid_diff[2] = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
}

TEST(TestDecodingState, FrameContinuityFlexibleModeGeneral) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  packet.video_header.is_first_packet_in_frame = true;
  packet.timestamp = 1;
  packet.seqNum = 0xffff;
  uint8_t data[] = "I need a data pointer for this test!";
  packet.sizeBytes = sizeof(data);
  packet.dataPtr = data;
  packet.video_header.codec = kVideoCodecVP9;

  auto& vp9_hdr =
      packet.video_header.video_type_header.emplace<RTPVideoHeaderVP9>();
  vp9_hdr.picture_id = 10;
  vp9_hdr.flexible_mode = true;

  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;

  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  dec_state.SetState(&frame);
  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.picture_id = 15;
  vp9_hdr.pid_diff[0] = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 16;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  vp9_hdr.picture_id = 16;
  vp9_hdr.pid_diff[0] = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 20;
  vp9_hdr.num_ref_pics = 2;
  vp9_hdr.pid_diff[0] = 9;
  vp9_hdr.pid_diff[1] = 5;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  vp9_hdr.picture_id = 22;
  vp9_hdr.num_ref_pics = 3;
  vp9_hdr.pid_diff[0] = 12;
  vp9_hdr.pid_diff[1] = 7;
  vp9_hdr.pid_diff[2] = 6;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  vp9_hdr.picture_id = VCMDecodingState::kFrameDecodedLength - 2;
  vp9_hdr.num_ref_pics = 0;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  vp9_hdr.picture_id = VCMDecodingState::kFrameDecodedLength - 1;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 0;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 20;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 20;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 23;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 13;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  vp9_hdr.picture_id = 25;
  vp9_hdr.num_ref_pics = 0;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  packet.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  vp9_hdr.picture_id = 26;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  frame.Reset();
  vp9_hdr.picture_id = 30;
  vp9_hdr.num_ref_pics = 1;
  vp9_hdr.pid_diff[0] = 30;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
}

}
