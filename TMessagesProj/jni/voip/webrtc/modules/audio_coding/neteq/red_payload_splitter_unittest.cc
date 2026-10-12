/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Unit tests for RedPayloadSplitter class.

#include "modules/audio_coding/neteq/red_payload_splitter.h"

#include <memory>
#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/packet.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"

using ::testing::Return;
using ::testing::ReturnNull;

namespace webrtc {

static const int kRedPayloadType = 100;
static const size_t kPayloadLength = 10;
static const uint16_t kSequenceNumber = 0;
static const uint32_t kBaseTimestamp = 0x12345678;

void CreateOpusFecPayload(uint8_t* payload,
                          size_t payload_length,
                          uint8_t payload_value) {
  if (payload_length < 2) {
    return;
  }
  payload[0] = 0x08;
  payload[1] = 0x40;
  memset(&payload[2], payload_value, payload_length - 2);
}

Packet CreateRedPayload(size_t num_payloads,
                        uint8_t* payload_types,
                        int timestamp_offset,
                        bool embed_opus_fec = false) {
  Packet packet;
  packet.payload_type = kRedPayloadType;
  packet.timestamp = kBaseTimestamp;
  packet.sequence_number = kSequenceNumber;
  packet.payload.SetSize((kPayloadLength + 1) +
                         (num_payloads - 1) *
                             (kPayloadLength + kRedHeaderLength));
  uint8_t* payload_ptr = packet.payload.data();
  for (size_t i = 0; i < num_payloads; ++i) {

    if (i == num_payloads - 1) {

      *payload_ptr = payload_types[i] & 0x7F;
      ++payload_ptr;
      break;
    }
    *payload_ptr = payload_types[i] & 0x7F;

    *payload_ptr |= 0x80;
    ++payload_ptr;
    int this_offset =
        rtc::checked_cast<int>((num_payloads - i - 1) * timestamp_offset);
    *payload_ptr = this_offset >> 6;
    ++payload_ptr;
    RTC_DCHECK_LE(kPayloadLength, 1023);
    *payload_ptr = ((this_offset & 0x3F) << 2) | (kPayloadLength >> 8);
    ++payload_ptr;
    *payload_ptr = kPayloadLength & 0xFF;
    ++payload_ptr;
  }
  for (size_t i = 0; i < num_payloads; ++i) {

    if (embed_opus_fec) {
      CreateOpusFecPayload(payload_ptr, kPayloadLength,
                           static_cast<uint8_t>(i));
    } else {
      memset(payload_ptr, static_cast<int>(i), kPayloadLength);
    }
    payload_ptr += kPayloadLength;
  }
  return packet;
}

Packet CreatePacket(uint8_t payload_type,
                    size_t payload_length,
                    uint8_t payload_value,
                    bool opus_fec = false) {
  Packet packet;
  packet.payload_type = payload_type;
  packet.timestamp = kBaseTimestamp;
  packet.sequence_number = kSequenceNumber;
  packet.payload.SetSize(payload_length);
  if (opus_fec) {
    CreateOpusFecPayload(packet.payload.data(), packet.payload.size(),
                         payload_value);
  } else {
    memset(packet.payload.data(), payload_value, packet.payload.size());
  }
  return packet;
}

void VerifyPacket(const Packet& packet,
                  size_t payload_length,
                  uint8_t payload_type,
                  uint16_t sequence_number,
                  uint32_t timestamp,
                  uint8_t payload_value,
                  Packet::Priority priority) {
  EXPECT_EQ(payload_length, packet.payload.size());
  EXPECT_EQ(payload_type, packet.payload_type);
  EXPECT_EQ(sequence_number, packet.sequence_number);
  EXPECT_EQ(timestamp, packet.timestamp);
  EXPECT_EQ(priority, packet.priority);
  ASSERT_FALSE(packet.payload.empty());
  for (size_t i = 0; i < packet.payload.size(); ++i) {
    ASSERT_EQ(payload_value, packet.payload.data()[i]);
  }
}

void VerifyPacket(const Packet& packet,
                  size_t payload_length,
                  uint8_t payload_type,
                  uint16_t sequence_number,
                  uint32_t timestamp,
                  uint8_t payload_value,
                  bool primary) {
  return VerifyPacket(packet, payload_length, payload_type, sequence_number,
                      timestamp, payload_value,
                      Packet::Priority{0, primary ? 0 : 1});
}

TEST(RedPayloadSplitter, CreateAndDestroy) {
  RedPayloadSplitter* splitter = new RedPayloadSplitter;
  delete splitter;
}

TEST(RedPayloadSplitter, OnePacketTwoPayloads) {
  uint8_t payload_types[] = {0, 0};
  const int kTimestampOffset = 160;
  PacketList packet_list;
  packet_list.push_back(CreateRedPayload(2, payload_types, kTimestampOffset));
  RedPayloadSplitter splitter;
  EXPECT_TRUE(splitter.SplitRed(&packet_list));
  ASSERT_EQ(2u, packet_list.size());

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[1],
               kSequenceNumber, kBaseTimestamp, 1, true);
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber, kBaseTimestamp - kTimestampOffset, 0, false);
}

TEST(RedPayloadSplitter, TwoPacketsOnePayload) {
  uint8_t payload_types[] = {0};
  const int kTimestampOffset = 160;

  PacketList packet_list;
  packet_list.push_back(CreateRedPayload(1, payload_types, kTimestampOffset));

  {
    Packet packet = CreateRedPayload(1, payload_types, kTimestampOffset);

    packet.timestamp += kTimestampOffset;
    packet.sequence_number++;
    packet_list.push_back(std::move(packet));
  }
  RedPayloadSplitter splitter;
  EXPECT_TRUE(splitter.SplitRed(&packet_list));
  ASSERT_EQ(2u, packet_list.size());

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber, kBaseTimestamp, 0, true);
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber + 1, kBaseTimestamp + kTimestampOffset, 0, true);
}

TEST(RedPayloadSplitter, TwoPacketsThreePayloads) {
  uint8_t payload_types[] = {2, 1, 0};
  const int kTimestampOffset = 160;

  PacketList packet_list;
  packet_list.push_back(CreateRedPayload(3, payload_types, kTimestampOffset));

  {
    Packet packet = CreateRedPayload(3, payload_types, kTimestampOffset);

    packet.timestamp += kTimestampOffset;
    packet.sequence_number++;
    packet_list.push_back(std::move(packet));
  }
  RedPayloadSplitter splitter;
  EXPECT_TRUE(splitter.SplitRed(&packet_list));
  ASSERT_EQ(6u, packet_list.size());

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[2],
               kSequenceNumber, kBaseTimestamp, 2, {0, 0});
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[1],
               kSequenceNumber, kBaseTimestamp - kTimestampOffset, 1, {0, 1});
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber, kBaseTimestamp - 2 * kTimestampOffset, 0,
               {0, 2});
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[2],
               kSequenceNumber + 1, kBaseTimestamp + kTimestampOffset, 2,
               {0, 0});
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[1],
               kSequenceNumber + 1, kBaseTimestamp, 1, {0, 1});
  packet_list.pop_front();

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber + 1, kBaseTimestamp - kTimestampOffset, 0,
               {0, 2});
}

TEST(RedPayloadSplitter, CheckRedPayloads) {
  PacketList packet_list;
  for (uint8_t i = 0; i <= 3; ++i) {

    packet_list.push_back(CreatePacket(i, 10, 0));
  }

  DecoderDatabase decoder_database(
      rtc::make_ref_counted<MockAudioDecoderFactory>(), absl::nullopt);
  decoder_database.RegisterPayload(0, SdpAudioFormat("cn", 8000, 1));
  decoder_database.RegisterPayload(1, SdpAudioFormat("pcmu", 8000, 1));
  decoder_database.RegisterPayload(2,
                                   SdpAudioFormat("telephone-event", 8000, 1));
  decoder_database.RegisterPayload(3, SdpAudioFormat("ilbc", 8000, 1));

  RedPayloadSplitter splitter;
  splitter.CheckRedPayloads(&packet_list, decoder_database);

  ASSERT_EQ(3u, packet_list.size());

  for (int i = 0; i <= 2; ++i) {
    VerifyPacket(packet_list.front(), 10, i, kSequenceNumber, kBaseTimestamp, 0,
                 true);
    packet_list.pop_front();
  }
  EXPECT_TRUE(packet_list.empty());
}

TEST(RedPayloadSplitter, CheckRedPayloadsRecursiveRed) {
  PacketList packet_list;
  for (uint8_t i = 0; i <= 3; ++i) {

    packet_list.push_back(CreatePacket(kRedPayloadType, 10, 0));
  }

  DecoderDatabase decoder_database(
      rtc::make_ref_counted<MockAudioDecoderFactory>(), absl::nullopt);
  decoder_database.RegisterPayload(kRedPayloadType,
                                   SdpAudioFormat("red", 8000, 1));

  RedPayloadSplitter splitter;
  splitter.CheckRedPayloads(&packet_list, decoder_database);

  EXPECT_TRUE(packet_list.empty());
}

TEST(RedPayloadSplitter, WrongPayloadLength) {
  uint8_t payload_types[] = {0, 0, 0};
  const int kTimestampOffset = 160;
  PacketList packet_list;
  {
    Packet packet = CreateRedPayload(3, payload_types, kTimestampOffset);

    packet.payload.SetSize(packet.payload.size() - (kPayloadLength + 1));
    packet_list.push_back(std::move(packet));
  }
  RedPayloadSplitter splitter;
  EXPECT_FALSE(splitter.SplitRed(&packet_list));
  ASSERT_EQ(1u, packet_list.size());

  VerifyPacket(packet_list.front(), kPayloadLength, payload_types[0],
               kSequenceNumber, kBaseTimestamp - 2 * kTimestampOffset, 0,
               {0, 2});
  packet_list.pop_front();
}

TEST(RedPayloadSplitter, RejectsIncompleteHeaders) {
  RedPayloadSplitter splitter;

  uint8_t payload_types[] = {0, 0};
  const int kTimestampOffset = 160;

  PacketList packet_list;

  packet_list.push_back(CreateRedPayload(2, payload_types, kTimestampOffset));
  packet_list.front().payload.SetSize(4);
  EXPECT_FALSE(splitter.SplitRed(&packet_list));
  EXPECT_FALSE(packet_list.empty());

  packet_list.front().payload.SetSize(3);
  EXPECT_FALSE(splitter.SplitRed(&packet_list));
  EXPECT_FALSE(packet_list.empty());
}

}
