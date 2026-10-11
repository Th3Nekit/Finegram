/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for PacketBuffer class.

#include "modules/audio_coding/neteq/packet_buffer.h"

#include <memory>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/mock/mock_decoder_database.h"
#include "modules/audio_coding/neteq/mock/mock_statistics_calculator.h"
#include "modules/audio_coding/neteq/packet.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
class MockEncodedAudioFrame : public webrtc::AudioDecoder::EncodedAudioFrame {
 public:
  MOCK_METHOD(size_t, Duration, (), (const, override));

  MOCK_METHOD(bool, IsDtxPacket, (), (const, override));

  MOCK_METHOD(absl::optional<DecodeResult>,
              Decode,
              (rtc::ArrayView<int16_t> decoded),
              (const, override));
};

class PacketGenerator {
 public:
  PacketGenerator(uint16_t seq_no, uint32_t ts, uint8_t pt, int frame_size);
  virtual ~PacketGenerator() {}
  void Reset(uint16_t seq_no, uint32_t ts, uint8_t pt, int frame_size);
  webrtc::Packet NextPacket(
      int payload_size_bytes,
      std::unique_ptr<webrtc::AudioDecoder::EncodedAudioFrame> audio_frame);

  uint16_t seq_no_;
  uint32_t ts_;
  uint8_t pt_;
  int frame_size_;
};

PacketGenerator::PacketGenerator(uint16_t seq_no,
                                 uint32_t ts,
                                 uint8_t pt,
                                 int frame_size) {
  Reset(seq_no, ts, pt, frame_size);
}

void PacketGenerator::Reset(uint16_t seq_no,
                            uint32_t ts,
                            uint8_t pt,
                            int frame_size) {
  seq_no_ = seq_no;
  ts_ = ts;
  pt_ = pt;
  frame_size_ = frame_size;
}

webrtc::Packet PacketGenerator::NextPacket(
    int payload_size_bytes,
    std::unique_ptr<webrtc::AudioDecoder::EncodedAudioFrame> audio_frame) {
  webrtc::Packet packet;
  packet.sequence_number = seq_no_;
  packet.timestamp = ts_;
  packet.payload_type = pt_;
  packet.payload.SetSize(payload_size_bytes);
  ++seq_no_;
  ts_ += frame_size_;
  packet.frame = std::move(audio_frame);
  return packet;
}

struct PacketsToInsert {
  uint16_t sequence_number;
  uint32_t timestamp;
  uint8_t payload_type;
  bool primary;

  int extract_order;
};

}

namespace webrtc {

TEST(PacketBuffer, CreateAndDestroy) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer* buffer =
      new PacketBuffer(10, &tick_timer, &mock_stats);
  EXPECT_TRUE(buffer->Empty());
  delete buffer;
}

TEST(PacketBuffer, InsertPacket) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(10, &tick_timer, &mock_stats);
  PacketGenerator gen(17u, 4711u, 0, 10);
  MockDecoderDatabase decoder_database;

  const int payload_len = 100;
  const Packet packet = gen.NextPacket(payload_len, nullptr);
  EXPECT_EQ(0, buffer.InsertPacket(           packet.Clone()));
  uint32_t next_ts;
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));
  EXPECT_EQ(4711u, next_ts);
  EXPECT_FALSE(buffer.Empty());
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  const Packet* next_packet = buffer.PeekNextPacket();
  EXPECT_EQ(packet, *next_packet);
  EXPECT_CALL(decoder_database, Die());

}

TEST(PacketBuffer, FlushBuffer) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(10, &tick_timer, &mock_stats);
  PacketGenerator gen(0, 0, 0, 10);
  const int payload_len = 10;
  MockDecoderDatabase decoder_database;

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacket(           gen.NextPacket(
                                     payload_len, nullptr)));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());
  EXPECT_FALSE(buffer.Empty());

  EXPECT_CALL(mock_stats, PacketsDiscarded(1)).Times(10);
  buffer.Flush();

  EXPECT_EQ(0u, buffer.NumPacketsInBuffer());
  EXPECT_TRUE(buffer.Empty());
  EXPECT_CALL(decoder_database, Die());
}

TEST(PacketBuffer, OverfillBuffer) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(10, &tick_timer, &mock_stats);
  PacketGenerator gen(0, 0, 0, 10);
  MockDecoderDatabase decoder_database;

  const int payload_len = 10;
  int i;
  for (i = 0; i < 10; ++i) {
    EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacket(           gen.NextPacket(
                                     payload_len, nullptr)));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());
  uint32_t next_ts;
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));
  EXPECT_EQ(0u, next_ts);

  EXPECT_CALL(mock_stats, PacketsDiscarded(1)).Times(10);
  const Packet packet = gen.NextPacket(payload_len, nullptr);

  EXPECT_EQ(PacketBuffer::kFlushed,
            buffer.InsertPacket(           packet.Clone()));
  EXPECT_EQ(1u, buffer.NumPacketsInBuffer());
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&next_ts));

  EXPECT_EQ(packet.timestamp, next_ts);

  EXPECT_CALL(decoder_database, Die());
}

TEST(PacketBuffer, ExtractOrderRedundancy) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(100, &tick_timer, &mock_stats);
  const int kPackets = 18;
  const int kFrameSize = 10;
  const int kPayloadLength = 10;

  PacketsToInsert packet_facts[kPackets] = {
      {0xFFFD, 0xFFFFFFD7, 0, true, 0},   {0xFFFE, 0xFFFFFFE1, 0, true, 1},
      {0xFFFE, 0xFFFFFFD7, 1, false, -1}, {0xFFFF, 0xFFFFFFEB, 0, true, 2},
      {0xFFFF, 0xFFFFFFE1, 1, false, -1}, {0x0000, 0xFFFFFFF5, 0, true, 3},
      {0x0000, 0xFFFFFFEB, 1, false, -1}, {0x0001, 0xFFFFFFFF, 0, true, 4},
      {0x0001, 0xFFFFFFF5, 1, false, -1}, {0x0002, 0x0000000A, 0, true, 5},
      {0x0002, 0xFFFFFFFF, 1, false, -1}, {0x0003, 0x0000000A, 1, false, -1},
      {0x0004, 0x0000001E, 0, true, 7},   {0x0004, 0x00000014, 1, false, 6},
      {0x0005, 0x0000001E, 0, true, -1},  {0x0005, 0x00000014, 1, false, -1},
      {0x0006, 0x00000028, 0, true, 8},   {0x0006, 0x0000001E, 1, false, -1},
  };
  MockDecoderDatabase decoder_database;

  const size_t kExpectPacketsInBuffer = 9;

  std::vector<Packet> expect_order(kExpectPacketsInBuffer);

  PacketGenerator gen(0, 0, 0, kFrameSize);

  InSequence s;
  MockFunction<void(int check_point_id)> check;
  for (int i = 0; i < kPackets; ++i) {
    gen.Reset(packet_facts[i].sequence_number, packet_facts[i].timestamp,
              packet_facts[i].payload_type, kFrameSize);
    Packet packet = gen.NextPacket(kPayloadLength, nullptr);
    packet.priority.codec_level = packet_facts[i].primary ? 0 : 1;
    if (packet_facts[i].extract_order < 0) {
      if (packet.priority.codec_level > 0) {
        EXPECT_CALL(mock_stats, SecondaryPacketsDiscarded(1));
      } else {
        EXPECT_CALL(mock_stats, PacketsDiscarded(1));
      }
    }
    EXPECT_CALL(check, Call(i));
    EXPECT_EQ(PacketBuffer::kOK,
              buffer.InsertPacket(           packet.Clone()));
    if (packet_facts[i].extract_order >= 0) {
      expect_order[packet_facts[i].extract_order] = std::move(packet);
    }
    check.Call(i);
  }

  EXPECT_EQ(kExpectPacketsInBuffer, buffer.NumPacketsInBuffer());

  for (size_t i = 0; i < kExpectPacketsInBuffer; ++i) {
    const absl::optional<Packet> packet = buffer.GetNextPacket();
    EXPECT_EQ(packet, expect_order[i]);
  }
  EXPECT_TRUE(buffer.Empty());
  EXPECT_CALL(decoder_database, Die());
}

TEST(PacketBuffer, DiscardPackets) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(100, &tick_timer, &mock_stats);
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  PacketList list;
  const int payload_len = 10;
  MockDecoderDatabase decoder_database;

  constexpr int kTotalPackets = 10;

  for (int i = 0; i < kTotalPackets; ++i) {
    buffer.InsertPacket(           gen.NextPacket(payload_len, nullptr));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());

  uint32_t current_ts = start_ts;

  constexpr int kDiscardPackets = 5;

  InSequence s;
  MockFunction<void(int check_point_id)> check;
  for (int i = 0; i < kDiscardPackets; ++i) {
    uint32_t ts;
    EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&ts));
    EXPECT_EQ(current_ts, ts);
    EXPECT_CALL(mock_stats, PacketsDiscarded(1));
    EXPECT_CALL(check, Call(i));
    EXPECT_EQ(PacketBuffer::kOK, buffer.DiscardNextPacket());
    current_ts += ts_increment;
    check.Call(i);
  }

  constexpr int kRemainingPackets = kTotalPackets - kDiscardPackets;

  constexpr size_t kSkipPackets = 1;
  EXPECT_CALL(mock_stats, PacketsDiscarded(1))
      .Times(kRemainingPackets - kSkipPackets);
  EXPECT_CALL(check, Call(17));
  buffer.DiscardOldPackets(start_ts + kTotalPackets * ts_increment,
                           kRemainingPackets * ts_increment);
  check.Call(17);

  EXPECT_EQ(kSkipPackets, buffer.NumPacketsInBuffer());
  uint32_t ts;
  EXPECT_EQ(PacketBuffer::kOK, buffer.NextTimestamp(&ts));
  EXPECT_EQ(current_ts, ts);

  EXPECT_CALL(mock_stats, PacketsDiscarded(kSkipPackets));
  buffer.DiscardAllOldPackets(start_ts + kTotalPackets * ts_increment);

  EXPECT_TRUE(buffer.Empty());
  EXPECT_CALL(decoder_database, Die());
}

TEST(PacketBuffer, Reordering) {
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(100, &tick_timer, &mock_stats);
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  const int payload_len = 10;

  PacketList list;
  for (int i = 0; i < 10; ++i) {
    Packet packet = gen.NextPacket(payload_len, nullptr);
    if (i % 2) {
      list.push_front(std::move(packet));
    } else {
      list.push_back(std::move(packet));
    }
  }

  for (Packet& packet : list) {
    EXPECT_EQ(PacketBuffer::kOK, buffer.InsertPacket(std::move(packet)));
  }
  EXPECT_EQ(10u, buffer.NumPacketsInBuffer());

  uint32_t current_ts = start_ts;
  for (int i = 0; i < 10; ++i) {
    const absl::optional<Packet> packet = buffer.GetNextPacket();
    ASSERT_TRUE(packet);
    EXPECT_EQ(current_ts, packet->timestamp);
    current_ts += ts_increment;
  }
  EXPECT_TRUE(buffer.Empty());
}

TEST(PacketBuffer, Failures) {
  const uint16_t start_seq_no = 17;
  const uint32_t start_ts = 4711;
  const uint32_t ts_increment = 10;
  int payload_len = 100;
  PacketGenerator gen(start_seq_no, start_ts, 0, ts_increment);
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;

  PacketBuffer buffer(100, &tick_timer, &mock_stats);
  {
    Packet packet = gen.NextPacket(payload_len, nullptr);
    packet.payload.Clear();
    EXPECT_EQ(PacketBuffer::kInvalidPacket,
              buffer.InsertPacket(           std::move(packet)));
  }

  uint32_t temp_ts;
  EXPECT_EQ(PacketBuffer::kBufferEmpty, buffer.NextTimestamp(&temp_ts));
  EXPECT_EQ(PacketBuffer::kBufferEmpty,
            buffer.NextHigherTimestamp(0, &temp_ts));
  EXPECT_EQ(NULL, buffer.PeekNextPacket());
  EXPECT_FALSE(buffer.GetNextPacket());

  EXPECT_EQ(PacketBuffer::kBufferEmpty, buffer.DiscardNextPacket());
  buffer.DiscardAllOldPackets(0);
}

TEST(PacketBuffer, ComparePackets) {
  PacketGenerator gen(0, 0, 0, 10);
  Packet a(gen.NextPacket(10, nullptr));
  Packet b(gen.NextPacket(10, nullptr));
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  a.timestamp = 0xFFFFFFFF - 10;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  EXPECT_TRUE(a == a);
  EXPECT_FALSE(a != a);
  EXPECT_FALSE(a < a);
  EXPECT_FALSE(a > a);
  EXPECT_TRUE(a <= a);
  EXPECT_TRUE(a >= a);

  a.timestamp = b.timestamp;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  a.sequence_number = 0xFFFF;
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(a > b);
  EXPECT_TRUE(a <= b);
  EXPECT_FALSE(a >= b);

  a.sequence_number = b.sequence_number;
  a.priority = {1, 0};
  b.priority = {0, 0};

  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a < b);
  EXPECT_TRUE(a > b);
  EXPECT_FALSE(a <= b);
  EXPECT_TRUE(a >= b);

  Packet c(gen.NextPacket(0, nullptr));
  Packet d(gen.NextPacket(0, nullptr));
  c.timestamp = b.timestamp;
  d.timestamp = b.timestamp;
  c.sequence_number = b.sequence_number;
  d.sequence_number = b.sequence_number;
  c.priority = {1, 1};
  d.priority = {0, 1};

  EXPECT_FALSE(c == d);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(c < d);
  EXPECT_TRUE(c > d);
  EXPECT_FALSE(c <= d);
  EXPECT_TRUE(c >= d);

  EXPECT_FALSE(c == a);
  EXPECT_TRUE(c != a);
  EXPECT_FALSE(c < a);
  EXPECT_TRUE(c > a);
  EXPECT_FALSE(c <= a);
  EXPECT_TRUE(c >= a);

  EXPECT_FALSE(c == b);
  EXPECT_TRUE(c != b);
  EXPECT_FALSE(c < b);
  EXPECT_TRUE(c > b);
  EXPECT_FALSE(c <= b);
  EXPECT_TRUE(c >= b);

  EXPECT_FALSE(a == d);
  EXPECT_TRUE(a != d);
  EXPECT_FALSE(a < d);
  EXPECT_TRUE(a > d);
  EXPECT_FALSE(a <= d);
  EXPECT_TRUE(a >= d);

  EXPECT_FALSE(d == b);
  EXPECT_TRUE(d != b);
  EXPECT_FALSE(d < b);
  EXPECT_TRUE(d > b);
  EXPECT_FALSE(d <= b);
  EXPECT_TRUE(d >= b);
}

TEST(PacketBuffer, GetSpanSamples) {
  constexpr size_t kFrameSizeSamples = 10;
  constexpr int kPayloadSizeBytes = 1;
  constexpr uint32_t kStartTimeStamp = 0xFFFFFFFE;
  constexpr int kSampleRateHz = 48000;
  constexpr bool kCountWaitingTime = false;
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(3, &tick_timer, &mock_stats);
  PacketGenerator gen(0, kStartTimeStamp, 0, kFrameSizeSamples);
  MockDecoderDatabase decoder_database;

  Packet packet_1 = gen.NextPacket(kPayloadSizeBytes, nullptr);

  std::unique_ptr<MockEncodedAudioFrame> mock_audio_frame =
      std::make_unique<MockEncodedAudioFrame>();
  EXPECT_CALL(*mock_audio_frame, Duration())
      .WillRepeatedly(Return(kFrameSizeSamples));
  Packet packet_2 =
      gen.NextPacket(kPayloadSizeBytes, std::move(mock_audio_frame));

  RTC_DCHECK_GT(packet_1.timestamp,
                packet_2.timestamp);

  EXPECT_EQ(PacketBuffer::kOK,
            buffer.InsertPacket(           std::move(packet_1)));

  constexpr size_t kLastDecodedSizeSamples = 2;

  EXPECT_EQ(kLastDecodedSizeSamples,
            buffer.GetSpanSamples(kLastDecodedSizeSamples, kSampleRateHz,
                                  kCountWaitingTime));

  EXPECT_EQ(PacketBuffer::kOK,
            buffer.InsertPacket(           std::move(packet_2)));

  EXPECT_EQ(kFrameSizeSamples * 2,
            buffer.GetSpanSamples(0, kSampleRateHz, kCountWaitingTime));

  EXPECT_EQ(kFrameSizeSamples * 2,
            buffer.GetSpanSamples(kLastDecodedSizeSamples, kSampleRateHz,
                                  kCountWaitingTime));
}

TEST(PacketBuffer, GetSpanSamplesCountWaitingTime) {
  constexpr size_t kFrameSizeSamples = 10;
  constexpr int kPayloadSizeBytes = 1;
  constexpr uint32_t kStartTimeStamp = 0xFFFFFFFE;
  constexpr int kSampleRateHz = 48000;
  constexpr bool kCountWaitingTime = true;
  constexpr size_t kLastDecodedSizeSamples = 0;
  TickTimer tick_timer;
  StrictMock<MockStatisticsCalculator> mock_stats;
  PacketBuffer buffer(3, &tick_timer, &mock_stats);
  PacketGenerator gen(0, kStartTimeStamp, 0, kFrameSizeSamples);
  MockDecoderDatabase decoder_database;

  Packet packet = gen.NextPacket(kPayloadSizeBytes, nullptr);

  EXPECT_EQ(PacketBuffer::kOK,
            buffer.InsertPacket(           std::move(packet)));

  EXPECT_EQ(0u, buffer.GetSpanSamples(kLastDecodedSizeSamples, kSampleRateHz,
                                      kCountWaitingTime));

  tick_timer.Increment();
  EXPECT_EQ(480u, buffer.GetSpanSamples(0, kSampleRateHz, kCountWaitingTime));

  tick_timer.Increment();
  EXPECT_EQ(960u, buffer.GetSpanSamples(0, kSampleRateHz, kCountWaitingTime));
}

namespace {
void TestIsObsoleteTimestamp(uint32_t limit_timestamp) {

  static const uint32_t kZeroHorizon = 0;
  static const uint32_t k2Pow31Minus1 = 0x7FFFFFFF;

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp, limit_timestamp, kZeroHorizon));

  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp - 1,
                                                limit_timestamp, kZeroHorizon));

  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp - k2Pow31Minus1,
                                                limit_timestamp, kZeroHorizon));

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp + 1, limit_timestamp, kZeroHorizon));

  uint32_t other_timestamp = limit_timestamp + (1 << 31);
  uint32_t lowest_timestamp = std::min(limit_timestamp, other_timestamp);
  uint32_t highest_timestamp = std::max(limit_timestamp, other_timestamp);
  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(
      lowest_timestamp, highest_timestamp, kZeroHorizon));
  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      highest_timestamp, lowest_timestamp, kZeroHorizon));

  static const uint32_t kHorizon = 10;

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp,
                                                 limit_timestamp, kHorizon));

  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp - 1,
                                                limit_timestamp, kHorizon));

  EXPECT_TRUE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp - 9,
                                                limit_timestamp, kHorizon));

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp - 10,
                                                 limit_timestamp, kHorizon));

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(
      limit_timestamp - k2Pow31Minus1, limit_timestamp, kHorizon));

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp + 1,
                                                 limit_timestamp, kHorizon));

  EXPECT_FALSE(PacketBuffer::IsObsoleteTimestamp(limit_timestamp + (1 << 31),
                                                 limit_timestamp, kHorizon));
}
}

TEST(PacketBuffer, IsObsoleteTimestamp) {
  TestIsObsoleteTimestamp(0);
  TestIsObsoleteTimestamp(1);
  TestIsObsoleteTimestamp(0xFFFFFFFF);
  TestIsObsoleteTimestamp(0x80000000);
  TestIsObsoleteTimestamp(0x80000001);
  TestIsObsoleteTimestamp(0x7FFFFFFF);
}

}
