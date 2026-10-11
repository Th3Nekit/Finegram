/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender.h"

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_timing.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/packet_sequencer.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender_video.h"
#include "modules/rtp_rtcp/source/video_fec_generator.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "rtc_base/rate_limiter.h"
#include "rtc_base/strings/string_builder.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {
enum : int {
  kAbsoluteSendTimeExtensionId = 1,
  kAudioLevelExtensionId,
  kGenericDescriptorId,
  kMidExtensionId,
  kRepairedRidExtensionId,
  kRidExtensionId,
  kTransmissionTimeOffsetExtensionId,
  kTransportSequenceNumberExtensionId,
  kVideoRotationExtensionId,
  kVideoTimingExtensionId,
};

const int kPayload = 100;
const int kRtxPayload = 98;
const uint32_t kTimestamp = 10;
const uint16_t kSeqNum = 33;
const uint32_t kSsrc = 725242;
const uint32_t kRtxSsrc = 12345;
const uint32_t kFlexFecSsrc = 45678;
const uint64_t kStartTime = 123456789;
const uint8_t kPayloadData[] = {47, 11, 32, 93, 89};
constexpr TimeDelta kDefaultExpectedRetransmissionTime = TimeDelta::Millis(125);
constexpr Frequency kRtpClockRate = Frequency::Hertz(90'000);
constexpr absl::string_view kMid = "mid";
constexpr absl::string_view kRid = "f";
constexpr bool kMarkerBit = true;

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;

class MockRtpPacketPacer : public RtpPacketSender {
 public:
  MockRtpPacketPacer() {}
  virtual ~MockRtpPacketPacer() {}

  MOCK_METHOD(void,
              EnqueuePackets,
              (std::vector<std::unique_ptr<RtpPacketToSend>>),
              (override));
  MOCK_METHOD(void, RemovePacketsForSsrc, (uint32_t), (override));
};

uint32_t ToRtpTimestamp(Timestamp time) {
  return static_cast<int64_t>((time - Timestamp::Zero()) * kRtpClockRate) &
         0xFFFF'FFFF;
}

}

class RtpSenderTest : public ::testing::Test {
 protected:
  RtpSenderTest()
      : time_controller_(Timestamp::Millis(kStartTime)),
        clock_(time_controller_.GetClock()),
        retransmission_rate_limiter_(clock_, 1000),
        flexfec_sender_(0,
                        kFlexFecSsrc,
                        kSsrc,
                        "",
                        std::vector<RtpExtension>(),
                        std::vector<RtpExtensionSize>(),
                        nullptr,
                        clock_) {}

  void SetUp() override { SetUpRtpSender(true, false, nullptr); }

  void SetUpRtpSender(bool populate_network2,
                      bool always_send_mid_and_rid,
                      VideoFecGenerator* fec_generator) {
    RtpRtcpInterface::Configuration config = GetDefaultConfig();
    config.fec_generator = fec_generator;
    config.populate_network2_timestamp = populate_network2;
    config.always_send_mid_and_rid = always_send_mid_and_rid;
    CreateSender(config);
  }

  RtpRtcpInterface::Configuration GetDefaultConfig() {
    RtpRtcpInterface::Configuration config;
    config.clock = clock_;
    config.local_media_ssrc = kSsrc;
    config.rtx_send_ssrc = kRtxSsrc;
    config.event_log = &mock_rtc_event_log_;
    config.retransmission_rate_limiter = &retransmission_rate_limiter_;
    config.paced_sender = &mock_paced_sender_;
    config.field_trials = &field_trials_;

    config.rid = std::string(kRid);
    return config;
  }

  void CreateSender(const RtpRtcpInterface::Configuration& config) {
    packet_history_ = std::make_unique<RtpPacketHistory>(
        config.clock, RtpPacketHistory::PaddingMode::kPriority);
    sequencer_.emplace(kSsrc, kRtxSsrc,
                                                               !config.audio,
                       clock_);
    rtp_sender_ = std::make_unique<RTPSender>(config, packet_history_.get(),
                                              config.paced_sender);
    sequencer_->set_media_sequence_number(kSeqNum);
    rtp_sender_->SetTimestampOffset(0);
  }

  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  NiceMock<MockRtcEventLog> mock_rtc_event_log_;
  MockRtpPacketPacer mock_paced_sender_;
  RateLimiter retransmission_rate_limiter_;
  FlexfecSender flexfec_sender_;

  absl::optional<PacketSequencer> sequencer_;
  std::unique_ptr<RtpPacketHistory> packet_history_;
  std::unique_ptr<RTPSender> rtp_sender_;

  const test::ExplicitKeyValueConfig field_trials_{""};

  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(int payload_type,
                                                  bool marker_bit,
                                                  uint32_t rtp_timestamp,
                                                  Timestamp capture_time) {
    auto packet = rtp_sender_->AllocatePacket();
    packet->SetPayloadType(payload_type);
    packet->set_packet_type(RtpPacketMediaType::kVideo);
    packet->SetMarker(marker_bit);
    packet->SetTimestamp(rtp_timestamp);
    packet->set_capture_time(capture_time);
    return packet;
  }

  std::unique_ptr<RtpPacketToSend> SendPacket(Timestamp capture_time,
                                              int payload_length) {
    uint32_t rtp_timestamp = ToRtpTimestamp(capture_time);
    auto packet =
        BuildRtpPacket(kPayload, kMarkerBit, rtp_timestamp, capture_time);
    packet->AllocatePayload(payload_length);
    packet->set_allow_retransmission(true);

    std::vector<std::unique_ptr<RtpPacketToSend>> packets(1);
    packets[0] = std::make_unique<RtpPacketToSend>(*packet);
    rtp_sender_->EnqueuePackets(std::move(packets));
    return packet;
  }

  std::unique_ptr<RtpPacketToSend> SendGenericPacket() {

    return SendPacket(                 clock_->CurrentTime(),
                                         rtp_sender_->MaxRtpPacketSize() -
                          rtp_sender_->ExpectedPerPacketOverhead());
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) {
    return rtp_sender_->GeneratePadding(
        target_size_bytes,                         true,
        sequencer_->CanSendPaddingOnMediaSsrc());
  }

  std::vector<std::unique_ptr<RtpPacketToSend>> Sequence(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
    for (auto& packet : packets) {
      sequencer_->Sequence(*packet);
    }
    return packets;
  }

  size_t GenerateAndSendPadding(size_t target_size_bytes) {
    size_t generated_bytes = 0;
    std::vector<std::unique_ptr<RtpPacketToSend>> packets;
    for (auto& packet : GeneratePadding(target_size_bytes)) {
      generated_bytes += packet->payload_size() + packet->padding_size();
      packets.push_back(std::move(packet));
    }
    rtp_sender_->EnqueuePackets(std::move(packets));
    return generated_bytes;
  }

  void EnableRtx() {

    constexpr uint16_t kNumberOfPacketsToStore = 100;
    packet_history_->SetStorePacketsStatus(
        RtpPacketHistory::StorageMode::kStoreAndCull, kNumberOfPacketsToStore);
    rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);
    rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  }

  void EnableMidSending(absl::string_view mid) {
    rtp_sender_->RegisterRtpHeaderExtension(RtpMid::Uri(), kMidExtensionId);
    rtp_sender_->SetMid(mid);
  }

  void EnableRidSending() {
    rtp_sender_->RegisterRtpHeaderExtension(RtpStreamId::Uri(),
                                            kRidExtensionId);
    rtp_sender_->RegisterRtpHeaderExtension(RepairedRtpStreamId::Uri(),
                                            kRepairedRidExtensionId);
  }
};

TEST_F(RtpSenderTest, AllocatePacketSetCsrcs) {

  uint32_t csrcs[] = {0x23456789};

  auto packet = rtp_sender_->AllocatePacket(csrcs);

  ASSERT_TRUE(packet);
  EXPECT_EQ(rtp_sender_->SSRC(), packet->Ssrc());
  EXPECT_THAT(packet->Csrcs(), ElementsAreArray(csrcs));
}

TEST_F(RtpSenderTest, AllocatePacketReserveExtensions) {

  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransmissionOffset::Uri(), kTransmissionTimeOffsetExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      AbsoluteSendTime::Uri(), kAbsoluteSendTimeExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(AudioLevel::Uri(),
                                                      kAudioLevelExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      VideoOrientation::Uri(), kVideoRotationExtensionId));

  auto packet = rtp_sender_->AllocatePacket();

  ASSERT_TRUE(packet);

  EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());

  EXPECT_FALSE(packet->HasExtension<AudioLevel>());
  EXPECT_FALSE(packet->HasExtension<VideoOrientation>());
}

TEST_F(RtpSenderTest, PaddingAlwaysAllowedOnAudio) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.audio = true;
  CreateSender(config);

  std::unique_ptr<RtpPacketToSend> audio_packet = rtp_sender_->AllocatePacket();

  audio_packet->SetMarker(false);
  audio_packet->SetPayloadType(kPayload);
  sequencer_->Sequence(*audio_packet);

  const size_t kPaddingSize = 59;

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(AllOf(
          Pointee(Property(&RtpPacketToSend::packet_type,
                           RtpPacketMediaType::kPadding)),
          Pointee(Property(&RtpPacketToSend::padding_size, kPaddingSize))))));
  EXPECT_EQ(kPaddingSize, GenerateAndSendPadding(kPaddingSize));

  const size_t kMinPaddingSize = 50;
  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(
                  AllOf(Pointee(Property(&RtpPacketToSend::packet_type,
                                         RtpPacketMediaType::kPadding)),
                        Pointee(Property(&RtpPacketToSend::padding_size,
                                         kMinPaddingSize))))));
  EXPECT_EQ(kMinPaddingSize, GenerateAndSendPadding(kMinPaddingSize - 5));
}

TEST_F(RtpSenderTest, SendToNetworkForwardsPacketsToPacer) {
  std::vector<std::unique_ptr<RtpPacketToSend>> packets(1);
  packets[0] =
      BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, Timestamp::Zero());
  Timestamp now = clock_->CurrentTime();

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(AllOf(
                  Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
                  Pointee(Property(&RtpPacketToSend::capture_time, now))))));

  rtp_sender_->EnqueuePackets(std::move(packets));
}

TEST_F(RtpSenderTest, ReSendPacketForwardsPacketsToPacer) {
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  Timestamp now = clock_->CurrentTime();
  auto packet = BuildRtpPacket(kPayload, kMarkerBit, kTimestamp, now);
  packet->SetSequenceNumber(kSeqNum);
  packet->set_allow_retransmission(true);
  packet_history_->PutRtpPacket(std::move(packet), now);

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(AllOf(
                  Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc)),
                  Pointee(Property(&RtpPacketToSend::SequenceNumber, kSeqNum)),
                  Pointee(Property(&RtpPacketToSend::capture_time, now)),
                  Pointee(Property(&RtpPacketToSend::packet_type,
                                   RtpPacketMediaType::kRetransmission))))));
  EXPECT_TRUE(rtp_sender_->ReSendPacket(kSeqNum));
}

TEST_F(RtpSenderTest, SendPadding) {
  constexpr int kNumPaddingPackets = 4;
  EXPECT_CALL(mock_paced_sender_, EnqueuePackets);
  std::unique_ptr<RtpPacketToSend> media_packet =
      SendPacket(                 clock_->CurrentTime(),
                                  100);
  sequencer_->Sequence(*media_packet);

  for (int i = 0; i < kNumPaddingPackets; ++i) {
    time_controller_.AdvanceTime(TimeDelta::Millis(50));
    const size_t kPaddingTargetBytes = 100;

    EXPECT_CALL(mock_paced_sender_,
                EnqueuePackets(ElementsAre(Pointee(AllOf(
                    Property(&RtpPacketToSend::Ssrc, kSsrc),
                    Property(&RtpPacketToSend::padding_size, kMaxPaddingLength),
                    Property(&RtpPacketToSend::SequenceNumber,
                             media_packet->SequenceNumber() + i + 1),
                    Property(&RtpPacketToSend::Timestamp,
                             media_packet->Timestamp()))))));
    std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
        Sequence(GeneratePadding(kPaddingTargetBytes));
    ASSERT_THAT(padding_packets, SizeIs(1));
    rtp_sender_->EnqueuePackets(std::move(padding_packets));
  }

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(Property(
          &RtpPacketToSend::Timestamp, Gt(media_packet->Timestamp()))))));

  std::unique_ptr<RtpPacketToSend> next_media_packet =
      SendPacket(                 clock_->CurrentTime(),
                                  100);
}

TEST_F(RtpSenderTest, NoPaddingAsFirstPacketWithoutBweExtensions) {
  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              IsEmpty());

  EnableRtx();
  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              IsEmpty());
}

TEST_F(RtpSenderTest, RequiresRtxSsrcToEnableRtx) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = absl::nullopt;
  RTPSender rtp_sender(config, packet_history_.get(), config.paced_sender);
  rtp_sender.SetRtxPayloadType(kRtxPayload, kPayload);

  rtp_sender.SetRtxStatus(kRtxRetransmitted);

  EXPECT_EQ(rtp_sender.RtxStatus(), kRtxOff);
}

TEST_F(RtpSenderTest, RequiresRtxPayloadTypesToEnableRtx) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = kRtxSsrc;
  RTPSender rtp_sender(config, packet_history_.get(), config.paced_sender);

  rtp_sender.SetRtxStatus(kRtxRetransmitted);

  EXPECT_EQ(rtp_sender.RtxStatus(), kRtxOff);
}

TEST_F(RtpSenderTest, CanEnableRtxWhenRtxSsrcAndPayloadTypeAreConfigured) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = kRtxSsrc;
  RTPSender rtp_sender(config, packet_history_.get(), config.paced_sender);
  rtp_sender.SetRtxPayloadType(kRtxPayload, kPayload);

  ASSERT_EQ(rtp_sender.RtxStatus(), kRtxOff);
  rtp_sender.SetRtxStatus(kRtxRetransmitted);

  EXPECT_EQ(rtp_sender.RtxStatus(), kRtxRetransmitted);
}

TEST_F(RtpSenderTest, AllowPaddingAsFirstPacketOnRtxWithTransportCc) {
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));

  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              IsEmpty());

  EnableRtx();
  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              Not(IsEmpty()));
}

TEST_F(RtpSenderTest, AllowPaddingAsFirstPacketOnRtxWithAbsSendTime) {
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      AbsoluteSendTime::Uri(), kAbsoluteSendTimeExtensionId));

  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              IsEmpty());

  EnableRtx();
  EXPECT_THAT(rtp_sender_->GeneratePadding(
                                        100,
                                          false,
                                                     false),
              Not(IsEmpty()));
}

TEST_F(RtpSenderTest, UpdatesTimestampsOnPlainRtxPadding) {
  EnableRtx();

  const Timestamp start_time = clock_->CurrentTime();
  const uint32_t start_timestamp = ToRtpTimestamp(start_time);

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(AllOf(
          Pointee(Property(&RtpPacketToSend::padding_size, 0u)),
          Pointee(Property(&RtpPacketToSend::Timestamp, start_timestamp)),
          Pointee(Property(&RtpPacketToSend::capture_time, start_time))))));
  std::unique_ptr<RtpPacketToSend> media_packet =
      SendPacket(start_time,                  600);
  sequencer_->Sequence(*media_packet);

  const TimeDelta kTimeDiff = TimeDelta::Millis(17);
  time_controller_.AdvanceTime(kTimeDiff);

  EXPECT_THAT(
      Sequence(GeneratePadding(                      100)),
      Each(Pointee(AllOf(
          Property(&RtpPacketToSend::padding_size, kMaxPaddingLength),
          Property(&RtpPacketToSend::Timestamp,
                   start_timestamp + kRtpClockRate * kTimeDiff),
          Property(&RtpPacketToSend::capture_time, start_time + kTimeDiff)))));
}

TEST_F(RtpSenderTest, KeepsTimestampsOnPayloadPadding) {
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));
  EnableRtx();

  const Timestamp start_time = clock_->CurrentTime();
  const uint32_t start_timestamp = ToRtpTimestamp(start_time);
  const size_t kPayloadSize = 200;
  const size_t kRtxHeaderSize = 2;

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(AllOf(
          Pointee(Property(&RtpPacketToSend::padding_size, 0u)),
          Pointee(Property(&RtpPacketToSend::Timestamp, start_timestamp)),
          Pointee(Property(&RtpPacketToSend::capture_time, start_time))))));
  std::unique_ptr<RtpPacketToSend> media_packet =
      SendPacket(start_time, kPayloadSize);
  packet_history_->PutRtpPacket(std::move(media_packet), start_time);

  const TimeDelta kTimeDiff = TimeDelta::Millis(17);
  time_controller_.AdvanceTime(kTimeDiff);

  EXPECT_THAT(
      GeneratePadding(                      100),
      Each(AllOf(
          Pointee(Property(&RtpPacketToSend::padding_size, 0u)),
          Pointee(Property(&RtpPacketToSend::payload_size,
                           kPayloadSize + kRtxHeaderSize)),
          Pointee(Property(&RtpPacketToSend::Timestamp, start_timestamp)),
          Pointee(Property(&RtpPacketToSend::capture_time, start_time)))));
}

TEST_F(RtpSenderTest, MidIncludedOnSentPackets) {
  EnableMidSending(kMid);

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(
                  Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid)))))
      .Times(2);
  SendGenericPacket();
  SendGenericPacket();
}

TEST_F(RtpSenderTest, RidIncludedOnSentPackets) {
  EnableRidSending();

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(Property(
                  &RtpPacketToSend::GetExtension<RtpStreamId>, kRid)))));
  SendGenericPacket();
}

TEST_F(RtpSenderTest, RidIncludedOnRtxSentPackets) {
  EnableRtx();
  EnableRidSending();

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(AllOf(
                  Property(&RtpPacketToSend::GetExtension<RtpStreamId>, kRid),
                  Property(&RtpPacketToSend::HasExtension<RepairedRtpStreamId>,
                           false))))))
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        sequencer_->Sequence(*packets[0]);
        packet_history_->PutRtpPacket(std::move(packets[0]),
                                      clock_->CurrentTime());
      });
  SendGenericPacket();

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(AllOf(
          Property(&RtpPacketToSend::GetExtension<RepairedRtpStreamId>, kRid),
          Property(&RtpPacketToSend::HasExtension<RtpStreamId>, false))))));
  rtp_sender_->ReSendPacket(kSeqNum);
}

TEST_F(RtpSenderTest, MidAndRidNotIncludedOnSentPacketsAfterAck) {
  EnableMidSending(kMid);
  EnableRidSending();

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(AllOf(
          Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid),
          Property(&RtpPacketToSend::GetExtension<RtpStreamId>, kRid))))));
  auto first_built_packet = SendGenericPacket();
  rtp_sender_->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(AllOf(
          Property(&RtpPacketToSend::HasExtension<RtpMid>, false),
          Property(&RtpPacketToSend::HasExtension<RtpStreamId>, false))))));
  SendGenericPacket();
}

TEST_F(RtpSenderTest, MidAndRidAlwaysIncludedOnSentPacketsWhenConfigured) {
  SetUpRtpSender(false,                             true, nullptr);
  EnableMidSending(kMid);
  EnableRidSending();

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(
          AllOf(Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid),
                Property(&RtpPacketToSend::GetExtension<RtpStreamId>, kRid))))))
      .Times(2);
  auto first_built_packet = SendGenericPacket();
  rtp_sender_->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());
  SendGenericPacket();
}

TEST_F(RtpSenderTest, MidAndRidIncludedOnFirstRtxPacket) {
  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending();

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets);
  auto first_built_packet = SendGenericPacket();
  rtp_sender_->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(SizeIs(1)))
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        packet_history_->PutRtpPacket(std::move(packets[0]),
                                      clock_->CurrentTime());
      });
  auto second_built_packet = SendGenericPacket();

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(AllOf(
                  Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid),
                  Property(&RtpPacketToSend::GetExtension<RepairedRtpStreamId>,
                           kRid))))));
  rtp_sender_->ReSendPacket(second_built_packet->SequenceNumber());
}

TEST_F(RtpSenderTest, MidAndRidNotIncludedOnRtxPacketsAfterAck) {
  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending();

  auto first_built_packet = SendGenericPacket();
  sequencer_->Sequence(*first_built_packet);
  packet_history_->PutRtpPacket(
      std::make_unique<RtpPacketToSend>(*first_built_packet),
                    clock_->CurrentTime());
  rtp_sender_->OnReceivedAckOnSsrc(first_built_packet->SequenceNumber());

  auto second_built_packet = SendGenericPacket();
  sequencer_->Sequence(*second_built_packet);
  packet_history_->PutRtpPacket(
      std::make_unique<RtpPacketToSend>(*second_built_packet),
                    clock_->CurrentTime());

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(SizeIs(1)))
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        rtp_sender_->OnReceivedAckOnRtxSsrc(packets[0]->SequenceNumber());
        packet_history_->MarkPacketAsSent(
            *packets[0]->retransmitted_sequence_number());
      });
  rtp_sender_->ReSendPacket(second_built_packet->SequenceNumber());

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(AllOf(
                  Property(&RtpPacketToSend::HasExtension<RtpMid>, false),
                  Property(&RtpPacketToSend::HasExtension<RepairedRtpStreamId>,
                           false))))))
      .Times(2);
  rtp_sender_->ReSendPacket(first_built_packet->SequenceNumber());
  rtp_sender_->ReSendPacket(second_built_packet->SequenceNumber());
}

TEST_F(RtpSenderTest, MidAndRidAlwaysIncludedOnRtxPacketsWhenConfigured) {
  SetUpRtpSender(false,                             true, nullptr);
  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending();

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(
          AllOf(Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid),
                Property(&RtpPacketToSend::GetExtension<RtpStreamId>, kRid))))))
      .Times(2)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
            packet_history_->PutRtpPacket(std::move(packets[0]),
                                          clock_->CurrentTime());
          });
  auto media_packet1 = SendGenericPacket();
  rtp_sender_->OnReceivedAckOnSsrc(media_packet1->SequenceNumber());
  auto media_packet2 = SendGenericPacket();

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(AllOf(
                  Property(&RtpPacketToSend::GetExtension<RtpMid>, kMid),
                  Property(&RtpPacketToSend::GetExtension<RepairedRtpStreamId>,
                           kRid))))))
      .Times(3)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
            rtp_sender_->OnReceivedAckOnRtxSsrc(packets[0]->SequenceNumber());
            packet_history_->MarkPacketAsSent(
                *packets[0]->retransmitted_sequence_number());
          });
  rtp_sender_->ReSendPacket(media_packet2->SequenceNumber());
  rtp_sender_->ReSendPacket(media_packet1->SequenceNumber());
  rtp_sender_->ReSendPacket(media_packet2->SequenceNumber());
}

TEST_F(RtpSenderTest, MidAndRidNotIncludedOnSentPacketsAfterRtpStateRestored) {
  EnableMidSending(kMid);
  EnableRidSending();

  RtpState state = rtp_sender_->GetRtpState();
  EXPECT_FALSE(state.ssrc_has_acked);
  state.ssrc_has_acked = true;
  rtp_sender_->SetRtpState(state);

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(AllOf(
          Property(&RtpPacketToSend::HasExtension<RtpMid>, false),
          Property(&RtpPacketToSend::HasExtension<RtpStreamId>, false))))));
  SendGenericPacket();
}

TEST_F(RtpSenderTest, MidAndRridNotIncludedOnRtxPacketsAfterRtpStateRestored) {
  EnableRtx();
  EnableMidSending(kMid);
  EnableRidSending();

  RtpState rtx_state = rtp_sender_->GetRtxRtpState();
  EXPECT_FALSE(rtx_state.ssrc_has_acked);
  rtx_state.ssrc_has_acked = true;
  rtp_sender_->SetRtxRtpState(rtx_state);

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(SizeIs(1)))
      .WillOnce([&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
        packet_history_->PutRtpPacket(std::move(packets[0]),
                                      clock_->CurrentTime());
      });
  auto built_packet = SendGenericPacket();

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(Pointee(AllOf(
          Property(&RtpPacketToSend::HasExtension<RtpMid>, false),
          Property(&RtpPacketToSend::HasExtension<RtpStreamId>, false))))));
  ASSERT_LT(0, rtp_sender_->ReSendPacket(built_packet->SequenceNumber()));
}

TEST_F(RtpSenderTest, RespectsNackBitrateLimit) {
  const int32_t kPacketSize = 1400;
  const int32_t kNumPackets = 30;
  retransmission_rate_limiter_.SetMaxRate(kPacketSize * kNumPackets * 8);
  EnableRtx();

  std::vector<uint16_t> sequence_numbers;
  for (int32_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<RtpPacketToSend> packet =
        BuildRtpPacket(kPayload,                true,                   0,
                                        clock_->CurrentTime());
    packet->set_allow_retransmission(true);
    sequencer_->Sequence(*packet);
    sequence_numbers.push_back(packet->SequenceNumber());
    packet_history_->PutRtpPacket(std::move(packet),
                                                clock_->CurrentTime());
    time_controller_.AdvanceTime(TimeDelta::Millis(1));
  }

  time_controller_.AdvanceTime(TimeDelta::Millis(1000 - kNumPackets));

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets(ElementsAre(Pointee(Property(
                                      &RtpPacketToSend::packet_type,
                                      RtpPacketMediaType::kRetransmission)))))
      .Times(kNumPackets)
      .WillRepeatedly(
          [&](std::vector<std::unique_ptr<RtpPacketToSend>> packets) {
            for (const auto& packet : packets) {
              packet_history_->MarkPacketAsSent(
                  *packet->retransmitted_sequence_number());
            }
          });
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);

  time_controller_.AdvanceTime(TimeDelta::Millis(5));

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets).Times(0);
  rtp_sender_->OnReceivedNack(sequence_numbers, 0);
}

TEST_F(RtpSenderTest, UpdatingCsrcsUpdatedOverhead) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = {};
  CreateSender(config);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);

  uint32_t csrcs[] = {1, 2};
  rtp_sender_->AllocatePacket(csrcs);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 20u);
}

TEST_F(RtpSenderTest, OnOverheadChanged) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = {};
  CreateSender(config);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);

  rtp_sender_->RegisterRtpHeaderExtension(TransmissionOffset::Uri(),
                                          kTransmissionTimeOffsetExtensionId);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 20u);
}

TEST_F(RtpSenderTest, CountMidOnlyUntilAcked) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = {};
  CreateSender(config);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);

  rtp_sender_->RegisterRtpHeaderExtension(RtpMid::Uri(), kMidExtensionId);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);
  rtp_sender_->SetMid("foo");
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 36u);
  rtp_sender_->RegisterRtpHeaderExtension(RtpStreamId::Uri(), kRidExtensionId);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 52u);

  rtp_sender_->OnReceivedAckOnSsrc(0);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);
}

TEST_F(RtpSenderTest, CountMidRidRridUntilAcked) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  CreateSender(config);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 14u);

  rtp_sender_->RegisterRtpHeaderExtension(RtpMid::Uri(), kMidExtensionId);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 14u);
  rtp_sender_->SetMid("foo");
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 38u);

  rtp_sender_->RegisterRtpHeaderExtension(RtpStreamId::Uri(), kRidExtensionId);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 54u);

  rtp_sender_->RegisterRtpHeaderExtension(RepairedRtpStreamId::Uri(),
                                          kRepairedRidExtensionId);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 54u);

  rtp_sender_->OnReceivedAckOnSsrc(0);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 54u);

  rtp_sender_->OnReceivedAckOnRtxSsrc(0);
  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 14u);
}

TEST_F(RtpSenderTest, DontCountVolatileExtensionsIntoOverhead) {
  RtpRtcpInterface::Configuration config = GetDefaultConfig();
  config.rtx_send_ssrc = {};
  CreateSender(config);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);

  rtp_sender_->RegisterRtpHeaderExtension(InbandComfortNoiseExtension::Uri(),
                                          1);
  rtp_sender_->RegisterRtpHeaderExtension(AbsoluteCaptureTimeExtension::Uri(),
                                          2);
  rtp_sender_->RegisterRtpHeaderExtension(VideoOrientation::Uri(), 3);
  rtp_sender_->RegisterRtpHeaderExtension(PlayoutDelayLimits::Uri(), 4);
  rtp_sender_->RegisterRtpHeaderExtension(VideoContentTypeExtension::Uri(), 5);
  rtp_sender_->RegisterRtpHeaderExtension(VideoTimingExtension::Uri(), 6);
  rtp_sender_->RegisterRtpHeaderExtension(RepairedRtpStreamId::Uri(), 7);
  rtp_sender_->RegisterRtpHeaderExtension(ColorSpaceExtension::Uri(), 8);

  EXPECT_EQ(rtp_sender_->ExpectedPerPacketOverhead(), 12u);
}

TEST_F(RtpSenderTest, SendPacketHandlesRetransmissionHistory) {
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  EXPECT_CALL(mock_paced_sender_, EnqueuePackets).WillRepeatedly(Return());

  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  const uint16_t media_sequence_number = packet->SequenceNumber();
  packet->set_allow_retransmission(true);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  time_controller_.AdvanceTime(TimeDelta::Millis(30));
  EXPECT_THAT(rtp_sender_->ReSendPacket(media_sequence_number), Gt(0));

  time_controller_.AdvanceTime(TimeDelta::Millis(30));
  EXPECT_THAT(rtp_sender_->ReSendPacket(media_sequence_number), Eq(0));

  packet_history_->MarkPacketAsSent(media_sequence_number);

  time_controller_.AdvanceTime(TimeDelta::Millis(30));
  EXPECT_THAT(rtp_sender_->ReSendPacket(media_sequence_number), Gt(0));
}

TEST_F(RtpSenderTest, MarksRetransmittedPackets) {
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);

  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  const uint16_t media_sequence_number = packet->SequenceNumber();
  packet->set_allow_retransmission(true);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  EXPECT_CALL(
      mock_paced_sender_,
      EnqueuePackets(ElementsAre(AllOf(
          Pointee(Property(&RtpPacketToSend::packet_type,
                           RtpPacketMediaType::kRetransmission)),
          Pointee(Property(&RtpPacketToSend::retransmitted_sequence_number,
                           Eq(media_sequence_number)))))));
  EXPECT_THAT(rtp_sender_->ReSendPacket(media_sequence_number), Gt(0));
}

TEST_F(RtpSenderTest, GeneratedPaddingHasBweExtensions) {

  const size_t kMinPaddingSize = 50;
  EnableRtx();

  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransmissionOffset::Uri(), kTransmissionTimeOffsetExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      AbsoluteSendTime::Uri(), kAbsoluteSendTimeExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));

  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kMinPaddingSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  std::vector<std::unique_ptr<RtpPacketToSend>> generated_packets =
      GeneratePadding(                      1);
  ASSERT_THAT(generated_packets, SizeIs(1));
  auto& plain_padding = generated_packets.front();
  EXPECT_GT(plain_padding->padding_size(), 0u);
  EXPECT_TRUE(plain_padding->HasExtension<TransportSequenceNumber>());
  EXPECT_TRUE(plain_padding->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(plain_padding->HasExtension<TransmissionOffset>());
  EXPECT_GT(plain_padding->padding_size(), 0u);

  generated_packets = GeneratePadding(kMinPaddingSize);
  ASSERT_EQ(generated_packets.size(), 1u);
  auto& payload_padding = generated_packets.front();
  EXPECT_EQ(payload_padding->padding_size(), 0u);
  EXPECT_TRUE(payload_padding->HasExtension<TransportSequenceNumber>());
  EXPECT_TRUE(payload_padding->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(payload_padding->HasExtension<TransmissionOffset>());
  EXPECT_GT(payload_padding->payload_size(), 0u);
}

TEST_F(RtpSenderTest, GeneratePaddingResendsOldPacketsWithRtx) {

  const size_t kMinPaddingSize = 50;

  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);

  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));

  const size_t kPayloadPacketSize = kMinPaddingSize;
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kPayloadPacketSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  std::vector<std::unique_ptr<RtpPacketToSend>> generated_packets =
      GeneratePadding(kMinPaddingSize);
  ASSERT_EQ(generated_packets.size(), 1u);
  auto& padding_packet = generated_packets.front();
  EXPECT_EQ(padding_packet->packet_type(), RtpPacketMediaType::kPadding);
  EXPECT_EQ(padding_packet->Ssrc(), kRtxSsrc);
  EXPECT_EQ(padding_packet->payload_size(),
            kPayloadPacketSize + kRtxHeaderSize);

  const size_t kPaddingBytesRequested = kMinPaddingSize - 1;

  size_t padding_bytes_generated = 0;
  generated_packets = GeneratePadding(kPaddingBytesRequested);
  EXPECT_EQ(generated_packets.size(), 1u);
  for (auto& packet : generated_packets) {
    EXPECT_EQ(packet->packet_type(), RtpPacketMediaType::kPadding);
    EXPECT_EQ(packet->Ssrc(), kRtxSsrc);
    EXPECT_EQ(packet->payload_size(), 0u);
    EXPECT_GT(packet->padding_size(), 0u);
    padding_bytes_generated += packet->padding_size();
  }

  EXPECT_EQ(padding_bytes_generated, kMaxPaddingLength);
}

TEST_F(RtpSenderTest, LimitsPayloadPaddingSize) {

  const double kFactor = 3.0;
  SetUpRtpSender(false, false, nullptr);
  rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);
  rtp_sender_->SetRtxStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);

  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));

  const size_t kPayloadPacketSize = 1234u;
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kPayloadPacketSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  const size_t kMinTargerSizeForPayload =
      (kPayloadPacketSize + kRtxHeaderSize) / kFactor;

  EXPECT_THAT(
      GeneratePadding(kMinTargerSizeForPayload),
      AllOf(Not(IsEmpty()),
            Each(Pointee(Property(&RtpPacketToSend::padding_size, Eq(0u))))));

  EXPECT_THAT(
      GeneratePadding(kMinTargerSizeForPayload - 1),
      AllOf(Not(IsEmpty()),
            Each(Pointee(Property(&RtpPacketToSend::padding_size, Gt(0u))))));
}

TEST_F(RtpSenderTest, GeneratePaddingCreatesPurePaddingWithoutRtx) {
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 1);
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransmissionOffset::Uri(), kTransmissionTimeOffsetExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      AbsoluteSendTime::Uri(), kAbsoluteSendTimeExtensionId));
  ASSERT_TRUE(rtp_sender_->RegisterRtpHeaderExtension(
      TransportSequenceNumber::Uri(), kTransportSequenceNumberExtensionId));

  const size_t kPayloadPacketSize = 1234;

  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, true, 0, clock_->CurrentTime());
  packet->set_allow_retransmission(true);
  packet->SetPayloadSize(kPayloadPacketSize);
  packet->set_packet_type(RtpPacketMediaType::kVideo);
  sequencer_->Sequence(*packet);
  packet_history_->PutRtpPacket(std::move(packet), clock_->CurrentTime());

  const size_t kPaddingBytesRequested = kPayloadPacketSize + kRtxHeaderSize;
  const size_t kExpectedNumPaddingPackets =
      (kPaddingBytesRequested + kMaxPaddingLength - 1) / kMaxPaddingLength;
  size_t padding_bytes_generated = 0;
  std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
      GeneratePadding(kPaddingBytesRequested);
  EXPECT_EQ(padding_packets.size(), kExpectedNumPaddingPackets);
  for (auto& packet : padding_packets) {
    EXPECT_EQ(packet->packet_type(), RtpPacketMediaType::kPadding);
    EXPECT_EQ(packet->Ssrc(), kSsrc);
    EXPECT_EQ(packet->payload_size(), 0u);
    EXPECT_GT(packet->padding_size(), 0u);
    padding_bytes_generated += packet->padding_size();
    EXPECT_TRUE(packet->HasExtension<TransportSequenceNumber>());
    EXPECT_TRUE(packet->HasExtension<AbsoluteSendTime>());
    EXPECT_TRUE(packet->HasExtension<TransmissionOffset>());
  }

  EXPECT_EQ(padding_bytes_generated,
            kExpectedNumPaddingPackets * kMaxPaddingLength);
}

TEST_F(RtpSenderTest, SupportsPadding) {
  bool kSendingMediaStats[] = {true, false};
  bool kEnableRedundantPayloads[] = {true, false};
  absl::string_view kBweExtensionUris[] = {
      TransportSequenceNumber::Uri(), TransportSequenceNumberV2::Uri(),
      AbsoluteSendTime::Uri(), TransmissionOffset::Uri()};
  const int kExtensionsId = 7;

  for (bool sending_media : kSendingMediaStats) {
    rtp_sender_->SetSendingMediaStatus(sending_media);
    for (bool redundant_payloads : kEnableRedundantPayloads) {
      int rtx_mode = kRtxRetransmitted;
      if (redundant_payloads) {
        rtx_mode |= kRtxRedundantPayloads;
      }
      rtp_sender_->SetRtxPayloadType(kRtxPayload, kPayload);
      rtp_sender_->SetRtxStatus(rtx_mode);

      for (auto extension_uri : kBweExtensionUris) {
        EXPECT_FALSE(rtp_sender_->SupportsPadding());
        rtp_sender_->RegisterRtpHeaderExtension(extension_uri, kExtensionsId);
        if (!sending_media) {
          EXPECT_FALSE(rtp_sender_->SupportsPadding());
        } else {
          EXPECT_TRUE(rtp_sender_->SupportsPadding());
          if (redundant_payloads) {
            EXPECT_TRUE(rtp_sender_->SupportsRtxPayloadPadding());
          } else {
            EXPECT_FALSE(rtp_sender_->SupportsRtxPayloadPadding());
          }
        }
        rtp_sender_->DeregisterRtpHeaderExtension(extension_uri);
        EXPECT_FALSE(rtp_sender_->SupportsPadding());
      }
    }
  }
}

TEST_F(RtpSenderTest, SetsCaptureTimeOnRtxRetransmissions) {
  EnableRtx();

  const Timestamp start_time = clock_->CurrentTime();
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, kMarkerBit,                   0,
                                      start_time);
  packet->set_allow_retransmission(true);
  sequencer_->Sequence(*packet);
  packet_history_->PutRtpPacket(std::move(packet), start_time);

  time_controller_.AdvanceTime(TimeDelta::Millis(10));

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(
                  Property(&RtpPacketToSend::capture_time, start_time)))));
  EXPECT_GT(rtp_sender_->ReSendPacket(kSeqNum), 0);
}

TEST_F(RtpSenderTest, IgnoresNackAfterDisablingMedia) {
  const TimeDelta kRtt = TimeDelta::Millis(10);

  EnableRtx();
  packet_history_->SetRtt(kRtt);

  const Timestamp start_time = clock_->CurrentTime();
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, kMarkerBit, 0,                  start_time);
  packet->set_allow_retransmission(true);
  sequencer_->Sequence(*packet);
  packet_history_->PutRtpPacket(std::move(packet), start_time);

  rtp_sender_->SetSendingMediaStatus(false);
  time_controller_.AdvanceTime(kRtt);
  EXPECT_LT(rtp_sender_->ReSendPacket(kSeqNum), 0);
}

TEST_F(RtpSenderTest, DoesntFecProtectRetransmissions) {

  const TimeDelta kRtt = TimeDelta::Millis(10);
  rtp_sender_->SetSendingMediaStatus(true);
  rtp_sender_->SetRtxStatus(kRtxOff);
  packet_history_->SetStorePacketsStatus(
      RtpPacketHistory::StorageMode::kStoreAndCull, 10);
  packet_history_->SetRtt(kRtt);

  const Timestamp start_time = clock_->CurrentTime();
  std::unique_ptr<RtpPacketToSend> packet =
      BuildRtpPacket(kPayload, kMarkerBit, 0, start_time);
  packet->set_allow_retransmission(true);
  packet->set_fec_protect_packet(true);
  sequencer_->Sequence(*packet);
  packet_history_->PutRtpPacket(std::move(packet), start_time);

  EXPECT_CALL(mock_paced_sender_,
              EnqueuePackets(ElementsAre(Pointee(
                  Property(&RtpPacketToSend::fec_protect_packet, false)))));

  time_controller_.AdvanceTime(kRtt);
  EXPECT_GT(rtp_sender_->ReSendPacket(kSeqNum), 0);
}

TEST_F(RtpSenderTest, MarksPacketsWithKeyframeStatus) {
  RTPSenderVideo::Config video_config;
  video_config.clock = clock_;
  video_config.rtp_sender = rtp_sender_.get();
  video_config.field_trials = &field_trials_;
  RTPSenderVideo rtp_sender_video(video_config);

  const uint8_t kPayloadType = 127;
  const absl::optional<VideoCodecType> kCodecType =
      VideoCodecType::kVideoCodecGeneric;

  const uint32_t kCaptureTimeMsToRtpTimestamp = 90;

  {
    EXPECT_CALL(mock_paced_sender_,
                EnqueuePackets(Each(
                    Pointee(Property(&RtpPacketToSend::is_key_frame, true)))))
        .Times(AtLeast(1));
    RTPVideoHeader video_header;
    video_header.frame_type = VideoFrameType::kVideoFrameKey;
    Timestamp capture_time = clock_->CurrentTime();
    EXPECT_TRUE(rtp_sender_video.SendVideo(
        kPayloadType, kCodecType,
        capture_time.ms() * kCaptureTimeMsToRtpTimestamp, capture_time,
        kPayloadData, sizeof(kPayloadData), video_header,
        kDefaultExpectedRetransmissionTime, {}));

    time_controller_.AdvanceTime(TimeDelta::Millis(33));
  }

  {
    EXPECT_CALL(mock_paced_sender_,
                EnqueuePackets(Each(
                    Pointee(Property(&RtpPacketToSend::is_key_frame, false)))))
        .Times(AtLeast(1));
    RTPVideoHeader video_header;
    video_header.frame_type = VideoFrameType::kVideoFrameDelta;
    Timestamp capture_time = clock_->CurrentTime();
    EXPECT_TRUE(rtp_sender_video.SendVideo(
        kPayloadType, kCodecType,
        capture_time.ms() * kCaptureTimeMsToRtpTimestamp, capture_time,
        kPayloadData, sizeof(kPayloadData), video_header,
        kDefaultExpectedRetransmissionTime, {}));

    time_controller_.AdvanceTime(TimeDelta::Millis(33));
  }
}

}
