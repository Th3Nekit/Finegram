/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/nack_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

const int kSampleRateHz = 16000;
const int kPacketSizeMs = 30;
const uint32_t kTimestampIncrement = 480;
const int64_t kShortRoundTripTimeMs = 1;

bool IsNackListCorrect(const std::vector<uint16_t>& nack_list,
                       const uint16_t* lost_sequence_numbers,
                       size_t num_lost_packets) {
  if (nack_list.size() != num_lost_packets)
    return false;

  if (num_lost_packets == 0)
    return true;

  for (size_t k = 0; k < nack_list.size(); ++k) {
    int seq_num = nack_list[k];
    bool seq_num_matched = false;
    for (size_t n = 0; n < num_lost_packets; ++n) {
      if (seq_num == lost_sequence_numbers[n]) {
        seq_num_matched = true;
        break;
      }
    }
    if (!seq_num_matched)
      return false;
  }
  return true;
}

}

TEST(NackTrackerTest, EmptyListWhenNoPacketLoss) {
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);

  int seq_num = 1;
  uint32_t timestamp = 0;

  std::vector<uint16_t> nack_list;
  for (int n = 0; n < 100; n++) {
    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    seq_num++;
    timestamp += kTimestampIncrement;
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(nack_list.empty());
  }
}

TEST(NackTrackerTest, LatePacketsMovedToNackThenNackListDoesNotChange) {
  const uint16_t kSequenceNumberLostPackets[] = {2, 3, 4, 5, 6, 7, 8, 9};
  static const int kNumAllLostPackets = sizeof(kSequenceNumberLostPackets) /
                                        sizeof(kSequenceNumberLostPackets[0]);

  for (int k = 0; k < 2; k++) {
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t sequence_num_lost_packets[kNumAllLostPackets];
    for (int n = 0; n < kNumAllLostPackets; n++) {
      sequence_num_lost_packets[n] =
          kSequenceNumberLostPackets[n] +
          k * 65531;
    }
    uint16_t seq_num = sequence_num_lost_packets[0] - 1;

    uint32_t timestamp = 0;
    std::vector<uint16_t> nack_list;

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(nack_list.empty());

    seq_num = sequence_num_lost_packets[kNumAllLostPackets - 1] + 1;
    timestamp += kTimestampIncrement * (kNumAllLostPackets + 1);
    int num_lost_packets = std::max(0, kNumAllLostPackets);

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(IsNackListCorrect(nack_list, sequence_num_lost_packets,
                                  num_lost_packets));
    seq_num++;
    timestamp += kTimestampIncrement;
    num_lost_packets++;

    for (int n = 0; n < 100; ++n) {
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
      nack_list = nack.GetNackList(kShortRoundTripTimeMs);
      EXPECT_TRUE(IsNackListCorrect(nack_list, sequence_num_lost_packets,
                                    kNumAllLostPackets));
      seq_num++;
      timestamp += kTimestampIncrement;
    }
  }
}

TEST(NackTrackerTest, ArrivedPacketsAreRemovedFromNackList) {
  const uint16_t kSequenceNumberLostPackets[] = {2, 3, 4, 5, 6, 7, 8, 9};
  static const int kNumAllLostPackets = sizeof(kSequenceNumberLostPackets) /
                                        sizeof(kSequenceNumberLostPackets[0]);

  for (int k = 0; k < 2; ++k) {
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t sequence_num_lost_packets[kNumAllLostPackets];
    for (int n = 0; n < kNumAllLostPackets; ++n) {
      sequence_num_lost_packets[n] = kSequenceNumberLostPackets[n] +
                                     k * 65531;
    }

    uint16_t seq_num = sequence_num_lost_packets[0] - 1;
    uint32_t timestamp = 0;

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(nack_list.empty());

    size_t index_retransmitted_rtp = 0;
    uint32_t timestamp_retransmitted_rtp = timestamp + kTimestampIncrement;

    seq_num = sequence_num_lost_packets[kNumAllLostPackets - 1] + 1;
    timestamp += kTimestampIncrement * (kNumAllLostPackets + 1);
    size_t num_lost_packets = kNumAllLostPackets;
    for (int n = 0; n < kNumAllLostPackets; ++n) {

      if (n >= 1)
        num_lost_packets--;

      nack.UpdateLastReceivedPacket(seq_num, timestamp);
      nack_list = nack.GetNackList(kShortRoundTripTimeMs);
      EXPECT_TRUE(IsNackListCorrect(
          nack_list, &sequence_num_lost_packets[index_retransmitted_rtp],
          num_lost_packets));
      seq_num++;
      timestamp += kTimestampIncrement;

      nack.UpdateLastReceivedPacket(
          sequence_num_lost_packets[index_retransmitted_rtp],
          timestamp_retransmitted_rtp);
      index_retransmitted_rtp++;
      timestamp_retransmitted_rtp += kTimestampIncrement;

      nack_list = nack.GetNackList(kShortRoundTripTimeMs);
      EXPECT_TRUE(IsNackListCorrect(
          nack_list, &sequence_num_lost_packets[index_retransmitted_rtp],
          num_lost_packets - 1));
    }
    ASSERT_TRUE(nack_list.empty());
  }
}

TEST(NackTrackerTest, EstimateTimestampAndTimeToPlay) {
  const uint16_t kLostPackets[] = {2, 3,  4,  5,  6,  7,  8,
                                   9, 10, 11, 12, 13, 14, 15};
  static const int kNumAllLostPackets =
      sizeof(kLostPackets) / sizeof(kLostPackets[0]);

  for (int k = 0; k < 4; ++k) {
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);

    int seq_num_offset = (k < 2) ? 0 : 65531;

    uint32_t timestamp_offset =
        (k & 0x1) ? static_cast<uint32_t>(0xffffffff) - 6 : 0;

    uint32_t timestamp_lost_packets[kNumAllLostPackets];
    uint16_t seq_num_lost_packets[kNumAllLostPackets];
    for (int n = 0; n < kNumAllLostPackets; ++n) {
      timestamp_lost_packets[n] =
          timestamp_offset + kLostPackets[n] * kTimestampIncrement;
      seq_num_lost_packets[n] = seq_num_offset + kLostPackets[n];
    }

    uint16_t seq_num = seq_num_lost_packets[0] - 2;
    uint32_t timestamp = timestamp_lost_packets[0] - 2 * kTimestampIncrement;

    const uint16_t first_seq_num = seq_num;
    const uint32_t first_timestamp = timestamp;

    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    seq_num++;
    timestamp += kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    seq_num = seq_num_lost_packets[kNumAllLostPackets - 1] + 1;
    timestamp =
        timestamp_lost_packets[kNumAllLostPackets - 1] + kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    NackTracker::NackList nack_list = nack.GetNackList();
    EXPECT_EQ(static_cast<size_t>(kNumAllLostPackets), nack_list.size());

    nack.UpdateLastDecodedPacket(first_seq_num, first_timestamp);
    nack_list = nack.GetNackList();

    NackTracker::NackList::iterator it = nack_list.begin();
    while (it != nack_list.end()) {
      seq_num = it->first - seq_num_offset;
      int index = seq_num - kLostPackets[0];
      EXPECT_EQ(timestamp_lost_packets[index], it->second.estimated_timestamp);
      EXPECT_EQ((index + 2) * kPacketSizeMs, it->second.time_to_play_ms);
      ++it;
    }
  }
}

TEST(NackTrackerTest,
     MissingPacketsPriorToLastDecodedRtpShouldNotBeInNackList) {
  for (int m = 0; m < 2; ++m) {
    uint16_t seq_num_offset = (m == 0) ? 0 : 65531;
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t seq_num = 0;
    nack.UpdateLastReceivedPacket(seq_num_offset + seq_num,
                                  seq_num * kTimestampIncrement);
    seq_num++;
    nack.UpdateLastReceivedPacket(seq_num_offset + seq_num,
                                  seq_num * kTimestampIncrement);

    const int kNumLostPackets = 10;
    seq_num += kNumLostPackets + 1;
    nack.UpdateLastReceivedPacket(seq_num_offset + seq_num,
                                  seq_num * kTimestampIncrement);

    const size_t kExpectedListSize = kNumLostPackets;
    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_EQ(kExpectedListSize, nack_list.size());

    for (int k = 0; k < 2; ++k) {

      for (int n = 0; n < kPacketSizeMs / 10; ++n) {
        nack.UpdateLastDecodedPacket(seq_num_offset + k,
                                     k * kTimestampIncrement);
        nack_list = nack.GetNackList(kShortRoundTripTimeMs);
        EXPECT_EQ(kExpectedListSize, nack_list.size());
      }
    }

    nack.UpdateLastDecodedPacket(seq_num + seq_num_offset,
                                 seq_num * kTimestampIncrement);
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(nack_list.empty());

    for (int n = 0; n < 10; ++n) {
      seq_num++;
      nack.UpdateLastReceivedPacket(seq_num_offset + seq_num,
                                    seq_num * kTimestampIncrement);
      nack_list = nack.GetNackList(kShortRoundTripTimeMs);
      EXPECT_TRUE(nack_list.empty());
    }
  }
}

TEST(NackTrackerTest, Reset) {
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);

  uint16_t seq_num = 0;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);
  seq_num++;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);

  const int kNumLostPackets = 10;
  seq_num += kNumLostPackets + 1;
  nack.UpdateLastReceivedPacket(seq_num, seq_num * kTimestampIncrement);

  const size_t kExpectedListSize = kNumLostPackets;
  std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTimeMs);
  EXPECT_EQ(kExpectedListSize, nack_list.size());

  nack.Reset();
  nack_list = nack.GetNackList(kShortRoundTripTimeMs);
  EXPECT_TRUE(nack_list.empty());
}

TEST(NackTrackerTest, ListSizeAppliedFromBeginning) {
  const size_t kNackListSize = 10;
  for (int m = 0; m < 2; ++m) {
    uint16_t seq_num_offset = (m == 0) ? 0 : 65525;
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);
    nack.SetMaxNackListSize(kNackListSize);

    uint16_t seq_num = seq_num_offset;
    uint32_t timestamp = 0x12345678;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    uint16_t num_lost_packets = kNackListSize + 5;

    seq_num += num_lost_packets + 1;
    timestamp += (num_lost_packets + 1) * kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_EQ(kNackListSize, nack_list.size());
  }
}

TEST(NackTrackerTest, ChangeOfListSizeAppliedAndOldElementsRemoved) {
  const size_t kNackListSize = 10;
  for (int m = 0; m < 2; ++m) {
    uint16_t seq_num_offset = (m == 0) ? 0 : 65525;
    NackTracker nack;
    nack.UpdateSampleRate(kSampleRateHz);

    uint16_t seq_num = seq_num_offset;
    uint32_t timestamp = 0x87654321;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);

    uint16_t num_lost_packets = kNackListSize + 5;

    std::unique_ptr<uint16_t[]> seq_num_lost(new uint16_t[num_lost_packets]);
    for (int n = 0; n < num_lost_packets; ++n) {
      seq_num_lost[n] = ++seq_num;
    }

    ++seq_num;
    timestamp += (num_lost_packets + 1) * kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    size_t expected_size = num_lost_packets;

    std::vector<uint16_t> nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_EQ(expected_size, nack_list.size());

    nack.SetMaxNackListSize(kNackListSize);
    expected_size = kNackListSize;
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(IsNackListCorrect(
        nack_list, &seq_num_lost[num_lost_packets - kNackListSize],
        expected_size));

    for (size_t n = 1; n < kNackListSize; ++n) {
      ++seq_num;
      timestamp += kTimestampIncrement;
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
      --expected_size;
      nack_list = nack.GetNackList(kShortRoundTripTimeMs);
      EXPECT_TRUE(IsNackListCorrect(
          nack_list, &seq_num_lost[num_lost_packets - kNackListSize + n],
          expected_size));
    }

    ++seq_num;
    timestamp += kTimestampIncrement;
    nack.UpdateLastReceivedPacket(seq_num, timestamp);
    nack_list = nack.GetNackList(kShortRoundTripTimeMs);
    EXPECT_TRUE(nack_list.empty());
  }
}

TEST(NackTrackerTest, RoudTripTimeIsApplied) {
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);

  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  uint16_t kNumLostPackets = 5;

  seq_num += (1 + kNumLostPackets);
  timestamp += (1 + kNumLostPackets) * kTimestampIncrement;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  std::vector<uint16_t> nack_list = nack.GetNackList(100);
  ASSERT_EQ(2u, nack_list.size());
  EXPECT_EQ(4, nack_list[0]);
  EXPECT_EQ(5, nack_list[1]);
}

TEST(NackTrackerTest, DoNotNackMultipleTimes) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-NetEqNackTrackerConfig/"
      "packet_loss_forget_factor:0.996,ms_per_loss_percent:20,"
      "never_nack_multiple_times:true/");
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);

  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  uint16_t kNumLostPackets = 3;

  seq_num += (1 + kNumLostPackets);
  timestamp += (1 + kNumLostPackets) * kTimestampIncrement;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);

  std::vector<uint16_t> nack_list = nack.GetNackList(10);
  ASSERT_EQ(3u, nack_list.size());
  EXPECT_EQ(1, nack_list[0]);
  EXPECT_EQ(2, nack_list[1]);
  EXPECT_EQ(3, nack_list[2]);

  std::vector<uint16_t> nack_list2 = nack.GetNackList(10);
  EXPECT_TRUE(nack_list2.empty());
}

TEST(NackTrackerTest, PacketLossRateCorrect) {
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  auto add_packet = [&nack, &seq_num, &timestamp](bool received) {
    if (received) {
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
    }
    seq_num++;
    timestamp += kTimestampIncrement;
  };

  for (int i = 0; i < 300; i++) {
    add_packet(true);
    add_packet(true);
    add_packet(true);
    add_packet(false);
  }

  EXPECT_NEAR(nack.GetPacketLossRateForTest(), 1 << 28, (1 << 30) / 100);
}

TEST(NackTrackerTest, DoNotNackAfterDtx) {
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  nack.UpdateLastReceivedPacket(seq_num, timestamp);
  EXPECT_TRUE(nack.GetNackList(0).empty());
  constexpr int kDtxPeriod = 400;
  nack.UpdateLastReceivedPacket(seq_num + 2,
                                timestamp + kDtxPeriod * kSampleRateHz / 1000);
  EXPECT_TRUE(nack.GetNackList(0).empty());
}

TEST(NackTrackerTest, DoNotNackIfLossRateIsTooHigh) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-NetEqNackTrackerConfig/max_loss_rate:0.4/");
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  auto add_packet = [&nack, &seq_num, &timestamp](bool received) {
    if (received) {
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
    }
    seq_num++;
    timestamp += kTimestampIncrement;
  };
  for (int i = 0; i < 500; i++) {
    add_packet(true);
    add_packet(false);
  }

  EXPECT_NEAR(nack.GetPacketLossRateForTest(), 1 << 29, (1 << 30) / 100);
  EXPECT_TRUE(nack.GetNackList(0).empty());
}

TEST(NackTrackerTest, OnlyNackIfRttIsValid) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Audio-NetEqNackTrackerConfig/require_valid_rtt:true/");
  const int kNackListSize = 200;
  NackTracker nack;
  nack.UpdateSampleRate(kSampleRateHz);
  nack.SetMaxNackListSize(kNackListSize);
  uint16_t seq_num = 0;
  uint32_t timestamp = 0x87654321;
  auto add_packet = [&nack, &seq_num, &timestamp](bool received) {
    if (received) {
      nack.UpdateLastReceivedPacket(seq_num, timestamp);
    }
    seq_num++;
    timestamp += kTimestampIncrement;
  };
  add_packet(true);
  add_packet(false);
  add_packet(true);
  EXPECT_TRUE(nack.GetNackList(0).empty());
  EXPECT_FALSE(nack.GetNackList(10).empty());
}

}
