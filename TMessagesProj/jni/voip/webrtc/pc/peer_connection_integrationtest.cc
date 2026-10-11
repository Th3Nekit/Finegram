/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Integration tests for PeerConnection.
// These tests exercise a full stack over a simulated network.
//
// NOTE: If your test takes a while (guideline: more than 5 seconds),
// do NOT add it here, but instead add it to the file
// slow_peer_connection_integrationtest.cc

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/dtmf_sender_interface.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/test/mock_async_dns_resolver.h"
#include "api/test/mock_encoder_selector.h"
#include "api/transport/rtp/rtp_source.h"
#include "api/uma_metrics.h"
#include "api/units/time_delta.h"
#include "api/video/video_rotation.h"
#include "logging/rtc_event_log/fake_rtc_event_log.h"
#include "logging/rtc_event_log/fake_rtc_event_log_factory.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/stream_params.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/test_stun_server.h"
#include "p2p/base/test_turn_customizer.h"
#include "p2p/base/test_turn_server.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/channel.h"
#include "pc/media_session.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_factory.h"
#include "pc/rtp_transceiver.h"
#include "pc/session_description.h"
#include "pc/test/fake_periodic_video_source.h"
#include "pc/test/integration_test_helpers.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_mdns_responder.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/test_certificate_verifier.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;

class PeerConnectionIntegrationTest
    : public PeerConnectionIntegrationBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionIntegrationTest()
      : PeerConnectionIntegrationBaseTest(GetParam()) {}
};

class FakeClockForTest : public rtc::ScopedFakeClock {
 protected:
  FakeClockForTest() {

    AdvanceTime(TimeDelta::Seconds(1));
  }

  ScopedFakeClock& FakeClock() { return *this; }
};

class PeerConnectionIntegrationTestWithFakeClock
    : public FakeClockForTest,
      public PeerConnectionIntegrationTest {};

class PeerConnectionIntegrationTestPlanB
    : public PeerConnectionIntegrationBaseTest {
 protected:
  PeerConnectionIntegrationTestPlanB()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kPlanB_DEPRECATED) {}
};

class PeerConnectionIntegrationTestUnifiedPlan
    : public PeerConnectionIntegrationBaseTest {
 protected:
  PeerConnectionIntegrationTestUnifiedPlan()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
};

TEST_P(PeerConnectionIntegrationTest,
       RtpReceiverObserverOnFirstPacketReceived) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ(2U, caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2U, callee()->rtp_receiver_observers().size());

  EXPECT_TRUE_WAIT(
      absl::c_all_of(caller()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }),
      kMaxWaitForFramesMs);
  EXPECT_TRUE_WAIT(
      absl::c_all_of(callee()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }),
      kMaxWaitForFramesMs);

  caller()->ResetRtpReceiverObservers();
  callee()->ResetRtpReceiverObservers();
  EXPECT_EQ(2U, caller()->rtp_receiver_observers().size());
  EXPECT_EQ(2U, callee()->rtp_receiver_observers().size());
  EXPECT_TRUE(
      absl::c_all_of(caller()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }));
  EXPECT_TRUE(
      absl::c_all_of(callee()->rtp_receiver_observers(),
                     [](const std::unique_ptr<MockRtpReceiverObserver>& o) {
                       return o->first_packet_received();
                     }));
}

class DummyDtmfObserver : public DtmfSenderObserverInterface {
 public:
  DummyDtmfObserver() : completed_(false) {}

  void OnToneChange(const std::string& tone) override {
    tones_.push_back(tone);
    if (tone.empty()) {
      completed_ = true;
    }
  }

  const std::vector<std::string>& tones() const { return tones_; }
  bool completed() const { return completed_; }

 private:
  bool completed_;
  std::vector<std::string> tones_;
};

void TestDtmfFromSenderToReceiver(PeerConnectionIntegrationWrapper* sender,
                                  PeerConnectionIntegrationWrapper* receiver) {

  rtc::scoped_refptr<DtmfSenderInterface> dtmf_sender =
      sender->pc()->GetSenders().at(0)->GetDtmfSender();
  ASSERT_TRUE(dtmf_sender);
  DummyDtmfObserver observer;
  dtmf_sender->RegisterObserver(&observer);

  EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
  EXPECT_TRUE(dtmf_sender->InsertDtmf("1a", 100, 50));

  EXPECT_TRUE_WAIT(observer.completed(), kDefaultTimeout);
  std::vector<std::string> tones = {"1", "a", ""};
  EXPECT_EQ(tones, observer.tones());
  dtmf_sender->UnregisterObserver();

}

TEST_P(PeerConnectionIntegrationTest, DtmfSenderObserver) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  TestDtmfFromSenderToReceiver(caller(), callee());
  TestDtmfFromSenderToReceiver(callee(), caller());
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithDtls) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallWithEncryptedRtpHeaderExtensions) {
  CryptoOptions crypto_options;
  crypto_options.srtp.enable_encrypted_rtp_header_extensions = true;
  PeerConnectionInterface::RTCConfiguration config;
  config.crypto_options = crypto_options;

  config.offer_extmap_allow_mixed = true;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       Send1280By720ResolutionAndReceive16To9AspectRatio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  FakePeriodicVideoSource::Config config;
  config.width = 1280;
  config.height = 720;
  config.timestamp_offset_ms = rtc::TimeMillis();
  caller()->AddTrack(caller()->CreateLocalVideoTrackWithConfig(config));
  callee()->AddTrack(callee()->CreateLocalVideoTrackWithConfig(config));

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  EXPECT_EQ(16.0 / 9, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, caller()->rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(16.0 / 9, callee()->rendered_aspect_ratio());
}

TEST_P(PeerConnectionIntegrationTest, OneWayMediaCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  media_expectations.CallerExpectsNoAudio();
  media_expectations.CallerExpectsNoVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithSendOnlyVideo) {
  ASSERT_TRUE(
      CreateOneDirectionalPeerConnectionWrappers(                     true));
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  caller()->AddTrack(caller_track);
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 0;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsNoVideo();
  media_expectations.CalleeExpectsSomeVideo();

  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithReceiveOnlyVideo) {
  ASSERT_TRUE(
      CreateOneDirectionalPeerConnectionWrappers(                     false));
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();
  callee()->AddTrack(callee_track);
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(caller()->pc()->GetReceivers().size(), 1u);

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsSomeVideo();
  media_expectations.CalleeExpectsNoVideo();

  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallAddReceiveVideoToSendOnlyCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  caller()->AddTrack(caller_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();
  callee()->AddTrack(callee_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallAddSendVideoToReceiveOnlyCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();
  callee()->AddTrack(callee_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  caller()->AddTrack(caller_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallRemoveReceiveVideoFromSendReceiveCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  rtc::scoped_refptr<RtpSenderInterface> caller_sender =
      caller()->AddTrack(caller_track);

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();

  rtc::scoped_refptr<RtpSenderInterface> callee_sender =
      callee()->AddTrack(callee_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  callee()->pc()->RemoveTrackOrError(callee_sender);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsNoVideo();
  media_expectations.CalleeExpectsSomeVideo();

  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallRemoveSendVideoFromSendReceiveCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  rtc::scoped_refptr<RtpSenderInterface> caller_sender =
      caller()->AddTrack(caller_track);

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();

  rtc::scoped_refptr<RtpSenderInterface> callee_sender =
      callee()->AddTrack(callee_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->pc()->RemoveTrackOrError(caller_sender);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsNoVideo();
  media_expectations.CallerExpectsSomeVideo();

  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, AudioToVideoUpgrade) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioTrack();
  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    callee()->SetOfferAnswerOptions(options);
  } else {
    callee()->SetRemoteOfferHandler([this] {
      callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->StopInternal();
    });
  }

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudio();
    media_expectations.ExpectNoVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }

  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);

  callee()->AddVideoTrack();
  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 1;
    callee()->SetOfferAnswerOptions(options);
  } else {
    callee()->SetRemoteOfferHandler(nullptr);
    caller()->SetRemoteOfferHandler([this] {

      auto transceivers = caller()->pc()->GetTransceivers();
      ASSERT_EQ(2U, transceivers.size());
      ASSERT_EQ(cricket::MEDIA_TYPE_VIDEO,
                transceivers[1]->receiver()->media_type());
      transceivers[1]->sender()->SetTrack(
          caller()->CreateLocalVideoTrack().get());
      transceivers[1]->SetDirectionWithError(
          RtpTransceiverDirection::kSendRecv);
    });
  }
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {

    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }
}

TEST_P(PeerConnectionIntegrationTest, AddAudioToVideoOnlyCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, BundlingEnabledWhileIceRestartOccurs) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    desc->RemoveGroupByName("BUNDLE");
  });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }

  callee()->SetReceivedSdpMunger(nullptr);
  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }
}

TEST_P(PeerConnectionIntegrationTest, RotatedVideoWithCVOExtension) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddTrack(
      caller()->CreateLocalVideoTrackWithRotation(kVideoRotation_90));
  callee()->AddTrack(
      callee()->CreateLocalVideoTrackWithRotation(kVideoRotation_270));

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  EXPECT_EQ(4.0 / 3, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, caller()->rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(4.0 / 3, callee()->rendered_aspect_ratio());

  EXPECT_EQ(kVideoRotation_270, caller()->rendered_rotation());
  EXPECT_EQ(kVideoRotation_90, callee()->rendered_rotation());
}

TEST_P(PeerConnectionIntegrationTest, RotatedVideoWithoutCVOExtension) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddTrack(
      caller()->CreateLocalVideoTrackWithRotation(kVideoRotation_90));
  callee()->AddTrack(
      callee()->CreateLocalVideoTrackWithRotation(kVideoRotation_270));

  callee()->SetReceivedSdpMunger([](cricket::SessionDescription* desc) {
    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(desc);
    video->ClearRtpHeaderExtensions();
  });

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->min_video_frames_received_per_track() > 0 &&
                       callee()->min_video_frames_received_per_track() > 0,
                   kMaxWaitForFramesMs);

  EXPECT_EQ(3.0 / 4, caller()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, caller()->rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, callee()->local_rendered_aspect_ratio());
  EXPECT_EQ(3.0 / 4, callee()->rendered_aspect_ratio());

  EXPECT_EQ(kVideoRotation_0, caller()->rendered_rotation());
  EXPECT_EQ(kVideoRotation_0, callee()->rendered_rotation());
}

TEST_P(PeerConnectionIntegrationTest, AnswererRejectsAudioSection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    callee()->SetOfferAnswerOptions(options);
  } else {

    callee()->SetRemoteOfferHandler([this] {
      callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO)
          ->StopInternal();
    });
  }
  callee()->AddTrack(callee()->CreateLocalVideoTrack());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  media_expectations.ExpectNoAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_audio_content =
      GetFirstAudioContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    EXPECT_EQ(nullptr,
              caller()->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_AUDIO));
  }
}

TEST_P(PeerConnectionIntegrationTest, AnswererRejectsVideoSection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    callee()->SetOfferAnswerOptions(options);
  } else {

    callee()->SetRemoteOfferHandler([this] {
      callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->StopInternal();
    });
  }
  callee()->AddTrack(callee()->CreateLocalAudioTrack());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  media_expectations.ExpectNoVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    EXPECT_EQ(nullptr,
              caller()->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO));
  }
}

TEST_P(PeerConnectionIntegrationTest, AnswererRejectsAudioAndVideoSections) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    options.offer_to_receive_video = 0;
    callee()->SetOfferAnswerOptions(options);
  } else {
    callee()->SetRemoteOfferHandler([this] {

      for (const auto& transceiver : callee()->pc()->GetTransceivers()) {
        transceiver->StopInternal();
      }
    });
  }

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  ASSERT_NE(nullptr, callee()->pc()->local_description());
  const ContentInfo* callee_audio_content =
      GetFirstAudioContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_audio_content);
  EXPECT_TRUE(callee_audio_content->rejected);
  const ContentInfo* callee_video_content =
      GetFirstVideoContent(callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, callee_video_content);
  EXPECT_TRUE(callee_video_content->rejected);
}

TEST_P(PeerConnectionIntegrationTest, VideoRejectedInSubsequentOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudioAndVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {
    caller()->SetGeneratedSdpMunger(
        [](cricket::SessionDescription* description) {
          for (cricket::ContentInfo& content : description->contents()) {
            if (cricket::IsVideoContent(&content)) {
              content.rejected = true;
            }
          }
        });
  } else {
    caller()
        ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
        ->StopInternal();
  }
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  ASSERT_NE(nullptr, caller()->pc()->local_description());
  const ContentInfo* caller_video_content =
      GetFirstVideoContent(caller()->pc()->local_description()->description());
  ASSERT_NE(nullptr, caller_video_content);
  EXPECT_TRUE(caller_video_content->rejected);

  {
    MediaExpectations media_expectations;
    media_expectations.ExpectBidirectionalAudio();
    media_expectations.ExpectNoVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }
}

TEST_F(PeerConnectionIntegrationTestPlanB, EnableAudioAfterRejecting) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<AudioTrackInterface> track =
      caller()->CreateLocalAudioTrack();
  rtc::scoped_refptr<RtpSenderInterface> sender =
      caller()->pc()->AddTrack(track, {"stream"}).MoveValue();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->pc()->RemoveTrackOrError(sender);
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 0;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  sender = caller()->pc()->AddTrack(track, {"stream"}).MoveValue();
  options.offer_to_receive_audio = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithoutSsrcOrMsidSignaling) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithoutSsrcSignaling) {
  const char kStreamId[] = "streamId";
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddTrack(caller()->CreateLocalAudioTrack(), {kStreamId});
  callee()->AddAudioTrack();

  callee()->SetReceivedSdpMunger(RemoveSsrcsAndKeepMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallAddReceiveVideoToSendOnlyCall) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> track =
      caller()->CreateLocalVideoTrack();

  RtpTransceiverInit video_transceiver_init;
  video_transceiver_init.stream_ids = {"video1"};
  video_transceiver_init.direction = RtpTransceiverDirection::kSendOnly;
  auto video_sender =
      caller()->pc()->AddTransceiver(track, video_transceiver_init).MoveValue();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  video_sender->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);

  rtc::scoped_refptr<VideoTrackInterface> callee_track =
      callee()->CreateLocalVideoTrack();

  callee()->AddTrack(callee_track);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithTwoVideoTracksAndNoSignaledSsrc) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  callee()->AddVideoTrack();

  caller()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  callee()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(2u, caller()->pc()->GetReceivers().size());
  ASSERT_EQ(2u, callee()->pc()->GetReceivers().size());

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

void RemoveBundleGroupSsrcsAndMidExtension(cricket::SessionDescription* desc) {
  RemoveSsrcsAndKeepMsids(desc);
  desc->RemoveGroupByName("BUNDLE");
  for (ContentInfo& content : desc->contents()) {
    cricket::MediaContentDescription* media = content.media_description();
    cricket::RtpHeaderExtensions extensions = media->rtp_header_extensions();
    extensions.erase(std::remove_if(extensions.begin(), extensions.end(),
                                    [](const RtpExtension& extension) {
                                      return extension.uri ==
                                             RtpExtension::kMidUri;
                                    }),
                     extensions.end());
    media->set_rtp_header_extensions(extensions);
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithTwoVideoTracksNoBundleNoSignaledSsrcAndNoMid) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  callee()->AddVideoTrack();
  caller()->SetReceivedSdpMunger(&RemoveBundleGroupSsrcsAndMidExtension);
  callee()->SetReceivedSdpMunger(&RemoveBundleGroupSsrcsAndMidExtension);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(2u, caller()->pc()->GetReceivers().size());
  ASSERT_EQ(2u, callee()->pc()->GetReceivers().size());

  ASSERT_NE(caller()->pc()->GetSenders()[0]->dtls_transport(),
            caller()->pc()->GetSenders()[1]->dtls_transport());

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

void ModifyPayloadTypesAndRemoveMidExtension(
    cricket::SessionDescription* desc) {
  int pt = 96;
  for (ContentInfo& content : desc->contents()) {
    cricket::MediaContentDescription* media = content.media_description();
    cricket::RtpHeaderExtensions extensions = media->rtp_header_extensions();
    extensions.erase(std::remove_if(extensions.begin(), extensions.end(),
                                    [](const RtpExtension& extension) {
                                      return extension.uri ==
                                             RtpExtension::kMidUri;
                                    }),
                     extensions.end());
    media->set_rtp_header_extensions(extensions);
    media->set_codecs({cricket::CreateVideoCodec(pt++, "VP8")});
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       EndToEndCallWithTwoVideoTracksDemultiplexedByPayloadType) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  callee()->AddVideoTrack();
  caller()->SetGeneratedSdpMunger(&ModifyPayloadTypesAndRemoveMidExtension);
  callee()->SetGeneratedSdpMunger(&ModifyPayloadTypesAndRemoveMidExtension);

  caller()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  callee()->SetReceivedSdpMunger(&RemoveSsrcsAndKeepMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(2u, caller()->pc()->GetReceivers().size());
  ASSERT_EQ(2u, callee()->pc()->GetReceivers().size());

  ASSERT_EQ(caller()->pc()->GetSenders()[0]->dtls_transport(),
            caller()->pc()->GetSenders()[1]->dtls_transport());

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan, NoStreamsMsidLinePresent) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  auto callee_receivers = callee()->pc()->GetReceivers();
  ASSERT_EQ(2u, callee_receivers.size());
  EXPECT_TRUE(callee_receivers[0]->stream_ids().empty());
  EXPECT_TRUE(callee_receivers[1]->stream_ids().empty());
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan, NoStreamsMsidLineMissing) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  caller()->AddVideoTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  auto callee_receivers = callee()->pc()->GetReceivers();
  ASSERT_EQ(2u, callee_receivers.size());
  ASSERT_EQ(1u, callee_receivers[0]->stream_ids().size());
  ASSERT_EQ(1u, callee_receivers[1]->stream_ids().size());
  EXPECT_EQ(callee_receivers[0]->stream_ids()[0],
            callee_receivers[1]->stream_ids()[0]);
  EXPECT_EQ(callee_receivers[0]->streams()[0],
            callee_receivers[1]->streams()[0]);
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithTwoVideoTracks) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(3u, callee()->pc()->GetReceivers().size());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

static void MakeSpecCompliantMaxBundleOffer(cricket::SessionDescription* desc) {
  bool first = true;
  for (cricket::ContentInfo& content : desc->contents()) {
    if (first) {
      first = false;
      continue;
    }
    content.bundle_only = true;
  }
  first = true;
  for (cricket::TransportInfo& transport : desc->transport_infos()) {
    if (first) {
      first = false;
      continue;
    }
    transport.description.ice_ufrag.clear();
    transport.description.ice_pwd.clear();
    transport.description.connection_role = cricket::CONNECTIONROLE_NONE;
    transport.description.identity_fingerprint.reset(nullptr);
  }
}

TEST_P(PeerConnectionIntegrationTest,
       EndToEndCallWithSpecCompliantMaxBundleOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  callee()->SetReceivedSdpMunger(MakeSpecCompliantMaxBundleOffer);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, GetAudioOutputLevelStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_TRUE_WAIT(callee()->OldGetStats()->AudioOutputLevel() > 0,
                   kMaxWaitForFramesMs);
}

TEST_P(PeerConnectionIntegrationTest, GetAudioInputLevelStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_TRUE_WAIT(caller()->OldGetStats()->AudioInputLevel() > 0,
                   kMaxWaitForStatsMs);
}

TEST_P(PeerConnectionIntegrationTest, GetBytesReceivedStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  for (const auto& receiver : callee()->pc()->GetReceivers()) {

    EXPECT_GT(
        callee()->OldGetStatsForTrack(receiver->track().get())->BytesReceived(),
        0);
  }
}

TEST_P(PeerConnectionIntegrationTest, GetBytesSentStatsWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto audio_track = caller()->CreateLocalAudioTrack();
  auto video_track = caller()->CreateLocalVideoTrack();
  caller()->AddTrack(audio_track);
  caller()->AddTrack(video_track);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  EXPECT_GT(caller()->OldGetStatsForTrack(audio_track.get())->BytesSent(), 0);
  EXPECT_GT(caller()->OldGetStatsForTrack(video_track.get())->BytesSent(), 0);
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       OldGetStatsAssociatesTrackIdForManyMediaSections) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto audio_sender_1 = caller()->AddAudioTrack();
  auto video_sender_1 = caller()->AddVideoTrack();
  auto audio_sender_2 = caller()->AddAudioTrack();
  auto video_sender_2 = caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE_WAIT(ExpectNewFrames(media_expectations), kDefaultTimeout);

  std::vector<std::string> track_ids = {
      audio_sender_1->track()->id(), video_sender_1->track()->id(),
      audio_sender_2->track()->id(), video_sender_2->track()->id()};

  auto caller_stats = caller()->OldGetStats();
  EXPECT_THAT(caller_stats->TrackIds(), UnorderedElementsAreArray(track_ids));
  auto callee_stats = callee()->OldGetStats();
  EXPECT_THAT(callee_stats->TrackIds(), UnorderedElementsAreArray(track_ids));
}

TEST_P(PeerConnectionIntegrationTest, NewGetStatsManyAudioAndManyVideoStreams) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto audio_sender_1 = caller()->AddAudioTrack();
  auto video_sender_1 = caller()->AddVideoTrack();
  auto audio_sender_2 = caller()->AddAudioTrack();
  auto video_sender_2 = caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE_WAIT(ExpectNewFrames(media_expectations), kDefaultTimeout);

  std::vector<std::string> track_ids = {
      audio_sender_1->track()->id(), video_sender_1->track()->id(),
      audio_sender_2->track()->id(), video_sender_2->track()->id()};

  rtc::scoped_refptr<const RTCStatsReport> caller_report =
      caller()->NewGetStats();
  ASSERT_TRUE(caller_report);
  auto outbound_stream_stats =
      caller_report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  ASSERT_EQ(outbound_stream_stats.size(), 4u);
  std::vector<std::string> outbound_track_ids;
  for (const auto& stat : outbound_stream_stats) {
    ASSERT_TRUE(stat->bytes_sent.has_value());
    EXPECT_LT(0u, *stat->bytes_sent);
    if (*stat->kind == "video") {
      ASSERT_TRUE(stat->key_frames_encoded.has_value());
      EXPECT_GT(*stat->key_frames_encoded, 0u);
      ASSERT_TRUE(stat->frames_encoded.has_value());
      EXPECT_GE(*stat->frames_encoded, *stat->key_frames_encoded);
    }
    ASSERT_TRUE(stat->media_source_id.has_value());
    const RTCMediaSourceStats* media_source =
        static_cast<const RTCMediaSourceStats*>(
            caller_report->Get(*stat->media_source_id));
    ASSERT_TRUE(media_source);
    outbound_track_ids.push_back(*media_source->track_identifier);
  }
  EXPECT_THAT(outbound_track_ids, UnorderedElementsAreArray(track_ids));

  rtc::scoped_refptr<const RTCStatsReport> callee_report =
      callee()->NewGetStats();
  ASSERT_TRUE(callee_report);
  auto inbound_stream_stats =
      callee_report->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_EQ(4u, inbound_stream_stats.size());
  std::vector<std::string> inbound_track_ids;
  for (const auto& stat : inbound_stream_stats) {
    ASSERT_TRUE(stat->bytes_received.has_value());
    EXPECT_LT(0u, *stat->bytes_received);
    if (*stat->kind == "video") {
      ASSERT_TRUE(stat->key_frames_decoded.has_value());
      EXPECT_GT(*stat->key_frames_decoded, 0u);
      ASSERT_TRUE(stat->frames_decoded.has_value());
      EXPECT_GE(*stat->frames_decoded, *stat->key_frames_decoded);
    }
    inbound_track_ids.push_back(*stat->track_identifier);
  }
  EXPECT_THAT(inbound_track_ids, UnorderedElementsAreArray(track_ids));
}

TEST_P(PeerConnectionIntegrationTest,
       GetStatsForUnsignaledStreamWithNewStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();

  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  rtc::scoped_refptr<const RTCStatsReport> report = callee()->NewGetStats();
  ASSERT_NE(nullptr, report);
  auto inbound_stream_stats =
      report->GetStatsOfType<RTCInboundRtpStreamStats>();
  ASSERT_EQ(1U, inbound_stream_stats.size());
  ASSERT_TRUE(inbound_stream_stats[0]->bytes_received.has_value());
  ASSERT_GT(*inbound_stream_stats[0]->bytes_received, 0U);
}

TEST_P(PeerConnectionIntegrationTest,
       GetStatsForUnsignaledStreamWithOldStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();

  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_TRUE_WAIT(callee()->OldGetStats()->BytesReceived() > 0,
                   kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest,
       GetMediaStatsForUnsignaledStreamWithNewStatsApi) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();

  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  media_expectations.CalleeExpectsSomeVideo(1);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  rtc::scoped_refptr<const RTCStatsReport> report = callee()->NewGetStats();
  ASSERT_NE(nullptr, report);

  auto inbound_rtps = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  auto index = FindFirstMediaStatsIndexByKind("audio", inbound_rtps);
  ASSERT_GE(index, 0);
  EXPECT_TRUE(inbound_rtps[index]->audio_level.has_value());
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithDtls10) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                      dtls_10_options));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, Dtls10CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_10_options;
  dtls_10_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_10_options,
                                                      dtls_10_options));
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(rtc::SSLStreamAdapter::IsAcceptableCipher(
                       caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
                   kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest, Dtls12CipherStatsAndUmaMetrics) {
  PeerConnectionFactory::Options dtls_12_options;
  dtls_12_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(dtls_12_options,
                                                      dtls_12_options));
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(rtc::SSLStreamAdapter::IsAcceptableCipher(
                       caller()->OldGetStats()->DtlsCipher(), rtc::KT_DEFAULT),
                   kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(kDefaultSrtpCryptoSuite),
                 caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest, CallerDtls12ToCalleeDtls10) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(caller_options, callee_options));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, CallerDtls10ToCalleeDtls12) {
  PeerConnectionFactory::Options caller_options;
  caller_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_10;
  PeerConnectionFactory::Options callee_options;
  callee_options.ssl_max_version = rtc::SSL_PROTOCOL_DTLS_12;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(caller_options, callee_options));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       Aes128Sha1_32_CipherNotUsedWhenOnlyCallerSupported) {
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher =
      false;
  int expected_cipher_suite = rtc::kSrtpAes128CmSha1_80;
  TestNegotiatedCipherSuite(caller_options, callee_options,
                            expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest,
       Aes128Sha1_32_CipherNotUsedWhenOnlyCalleeSupported) {
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher =
      false;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  int expected_cipher_suite = rtc::kSrtpAes128CmSha1_80;
  TestNegotiatedCipherSuite(caller_options, callee_options,
                            expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest, Aes128Sha1_32_CipherUsedWhenSupported) {
  PeerConnectionFactory::Options caller_options;
  caller_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  PeerConnectionFactory::Options callee_options;
  callee_options.crypto_options.srtp.enable_aes128_sha1_32_crypto_cipher = true;
  int expected_cipher_suite = rtc::kSrtpAes128CmSha1_32;
  TestNegotiatedCipherSuite(caller_options, callee_options,
                            expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest, NonGcmCipherUsedWhenGcmNotSupported) {
  bool local_gcm_enabled = false;
  bool remote_gcm_enabled = false;
  bool aes_ctr_enabled = true;
  int expected_cipher_suite = kDefaultSrtpCryptoSuite;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    aes_ctr_enabled, expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest, GcmCipherUsedWhenOnlyGcmSupported) {
  bool local_gcm_enabled = true;
  bool remote_gcm_enabled = true;
  bool aes_ctr_enabled = false;
  int expected_cipher_suite = kDefaultSrtpCryptoSuiteGcm;
  TestGcmNegotiationUsesCipherSuite(local_gcm_enabled, remote_gcm_enabled,
                                    aes_ctr_enabled, expected_cipher_suite);
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithGcmCipher) {
  PeerConnectionFactory::Options gcm_options;
  gcm_options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  gcm_options.crypto_options.srtp.enable_aes128_sha1_80_crypto_cipher = false;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithOptions(gcm_options, gcm_options));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, IceStatesReachCompletion) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 caller()->ice_gathering_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 callee()->ice_gathering_state(), kMaxWaitForFramesMs);

  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kDefaultTimeout);
}

constexpr int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                                cricket::PORTALLOCATOR_DISABLE_RELAY |
                                cricket::PORTALLOCATOR_DISABLE_TCP;

TEST_P(PeerConnectionIntegrationTest,
       IceStatesReachCompletionWithRemoteHostname) {
  auto caller_resolver_factory =
      std::make_unique<NiceMock<MockAsyncDnsResolverFactory>>();
  auto callee_resolver_factory =
      std::make_unique<NiceMock<MockAsyncDnsResolverFactory>>();
  auto callee_async_resolver =
      std::make_unique<NiceMock<MockAsyncDnsResolver>>();
  auto caller_async_resolver =
      std::make_unique<NiceMock<MockAsyncDnsResolver>>();

  auto* callee_resolver_ptr = callee_async_resolver.get();
  auto* caller_resolver_ptr = caller_async_resolver.get();

  EXPECT_CALL(*caller_resolver_factory, Create())
      .WillOnce(Return(ByMove(std::move(caller_async_resolver))));
  PeerConnectionDependencies caller_deps(nullptr);
  caller_deps.async_dns_resolver_factory = std::move(caller_resolver_factory);

  EXPECT_CALL(*callee_resolver_factory, Create())
      .WillOnce(Return(ByMove(std::move(callee_async_resolver))));
  PeerConnectionDependencies callee_deps(nullptr);
  callee_deps.async_dns_resolver_factory = std::move(callee_resolver_factory);

  PeerConnectionInterface::RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;

  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfigAndDeps(
      config, std::move(caller_deps), config, std::move(callee_deps)));

  RTC_LOG(LS_ERROR) << "callee async resolver is "
                    << callee_async_resolver.get();
  caller()->SetRemoteAsyncResolver(callee_resolver_ptr);
  callee()->SetRemoteAsyncResolver(caller_resolver_ptr);

  caller()->SetMdnsResponder(
      std::make_unique<FakeMdnsResponder>(network_thread()));
  callee()->SetMdnsResponder(
      std::make_unique<FakeMdnsResponder>(network_thread()));

  SetPortAllocatorFlags(kOnlyLocalPorts, kOnlyLocalPorts);

  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kDefaultTimeout);

  SendTask(network_thread(), [] {});

  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.PeerConnection.CandidatePairType_UDP",
                            kIceCandidatePairHostNameHostName));
  DestroyPeerConnections();
}

class PeerConnectionIntegrationIceStatesTest
    : public PeerConnectionIntegrationBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, std::tuple<std::string, uint32_t>>> {
 protected:
  PeerConnectionIntegrationIceStatesTest()
      : PeerConnectionIntegrationBaseTest(std::get<0>(GetParam())) {
    port_allocator_flags_ = std::get<1>(std::get<1>(GetParam()));
  }

  void StartStunServer(const SocketAddress& server_address) {
    stun_server_ = cricket::TestStunServer::Create(firewall(), server_address,
                                                   *network_thread());
  }

  bool TestIPv6() {
    return (port_allocator_flags_ & cricket::PORTALLOCATOR_ENABLE_IPV6);
  }

  void SetPortAllocatorFlags() {
    PeerConnectionIntegrationBaseTest::SetPortAllocatorFlags(
        port_allocator_flags_, port_allocator_flags_);
  }

  std::vector<SocketAddress> CallerAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("1.1.1.1", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("1111:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  std::vector<SocketAddress> CalleeAddresses() {
    std::vector<SocketAddress> addresses;
    addresses.push_back(SocketAddress("2.2.2.2", 0));
    if (TestIPv6()) {
      addresses.push_back(SocketAddress("2222:0:a:b:c:d:e:f", 0));
    }
    return addresses;
  }

  void SetUpNetworkInterfaces() {

    caller()->network_manager()->RemoveInterface(kDefaultLocalAddress);
    callee()->network_manager()->RemoveInterface(kDefaultLocalAddress);

    for (const auto& caller_address : CallerAddresses()) {
      caller()->network_manager()->AddInterface(caller_address);
    }
    for (const auto& callee_address : CalleeAddresses()) {
      callee()->network_manager()->AddInterface(callee_address);
    }
  }

 private:
  uint32_t port_allocator_flags_;
  cricket::TestStunServer::StunServerPtr stun_server_;
};

class PeerConnectionIntegrationIceStatesTestWithFakeClock
    : public FakeClockForTest,
      public PeerConnectionIntegrationIceStatesTest {};

#if !defined(THREAD_SANITIZER)

TEST_P(PeerConnectionIntegrationIceStatesTestWithFakeClock,
       IceStateSetupFailure) {

  for (const auto& caller_address : CallerAddresses()) {
    firewall()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY, caller_address);
  }

  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  SetPortAllocatorFlags();
  SetUpNetworkInterfaces();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();

  constexpr int kConsentTimeout = 30000;
  ASSERT_EQ_SIMULATED_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                           caller()->standardized_ice_connection_state(),
                           kConsentTimeout, FakeClock());
}

#endif

#if defined(WEBRTC_WIN)
#define MAYBE_VerifyBestConnection DISABLED_VerifyBestConnection
#else
#define MAYBE_VerifyBestConnection VerifyBestConnection
#endif
TEST_P(PeerConnectionIntegrationIceStatesTest, MAYBE_VerifyBestConnection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  SetPortAllocatorFlags();
  SetUpNetworkInterfaces();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();

  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kDefaultTimeout);

  SendTask(network_thread(), [] {});

  const int num_best_ipv4 = metrics::NumEvents(
      "WebRTC.PeerConnection.IPMetrics", kBestConnections_IPv4);
  const int num_best_ipv6 = metrics::NumEvents(
      "WebRTC.PeerConnection.IPMetrics", kBestConnections_IPv6);
  if (TestIPv6()) {

    EXPECT_METRIC_EQ(0, num_best_ipv4);
    EXPECT_METRIC_EQ(1, num_best_ipv6);
  } else {
    EXPECT_METRIC_EQ(1, num_best_ipv4);
    EXPECT_METRIC_EQ(0, num_best_ipv6);
  }

  EXPECT_METRIC_EQ(
      0, metrics::NumEvents("WebRTC.PeerConnection.CandidatePairType_UDP",
                            kIceCandidatePairHostHost));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.PeerConnection.CandidatePairType_UDP",
                            kIceCandidatePairHostPublicHostPublic));
}

constexpr uint32_t kFlagsIPv4NoStun = cricket::PORTALLOCATOR_DISABLE_TCP |
                                      cricket::PORTALLOCATOR_DISABLE_STUN |
                                      cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv6NoStun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_STUN |
    cricket::PORTALLOCATOR_ENABLE_IPV6 | cricket::PORTALLOCATOR_DISABLE_RELAY;
constexpr uint32_t kFlagsIPv4Stun =
    cricket::PORTALLOCATOR_DISABLE_TCP | cricket::PORTALLOCATOR_DISABLE_RELAY;

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionIntegrationTest,
    PeerConnectionIntegrationIceStatesTest,
    Combine(Values(SdpSemantics::kPlanB_DEPRECATED, SdpSemantics::kUnifiedPlan),
            Values(std::make_pair("IPv4 no STUN", kFlagsIPv4NoStun),
                   std::make_pair("IPv6 no STUN", kFlagsIPv6NoStun),
                   std::make_pair("IPv4 with STUN", kFlagsIPv4Stun))));

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionIntegrationTest,
    PeerConnectionIntegrationIceStatesTestWithFakeClock,
    Combine(Values(SdpSemantics::kPlanB_DEPRECATED, SdpSemantics::kUnifiedPlan),
            Values(std::make_pair("IPv4 no STUN", kFlagsIPv4NoStun),
                   std::make_pair("IPv6 no STUN", kFlagsIPv6NoStun),
                   std::make_pair("IPv4 with STUN", kFlagsIPv4Stun))));

TEST_P(PeerConnectionIntegrationTest, MediaContinuesFlowingAfterIceRestart) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  const IceCandidateCollection* audio_candidates_caller =
      caller()->pc()->local_description()->candidates(0);
  const IceCandidateCollection* audio_candidates_callee =
      callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_pre_restart));
  std::string callee_candidate_pre_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_pre_restart));
  const cricket::SessionDescription* desc =
      caller()->pc()->local_description()->description();
  std::string caller_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = callee()->pc()->local_description()->description();
  std::string callee_ufrag_pre_restart =
      desc->transport_infos()[0].description.ice_ufrag;

  EXPECT_EQ(caller()->ice_candidate_pair_change_history().size(), 1u);

  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  audio_candidates_caller = caller()->pc()->local_description()->candidates(0);
  audio_candidates_callee = callee()->pc()->local_description()->candidates(0);
  ASSERT_GT(audio_candidates_caller->count(), 0u);
  ASSERT_GT(audio_candidates_callee->count(), 0u);
  std::string caller_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_caller->at(0)->ToString(&caller_candidate_post_restart));
  std::string callee_candidate_post_restart;
  ASSERT_TRUE(
      audio_candidates_callee->at(0)->ToString(&callee_candidate_post_restart));
  desc = caller()->pc()->local_description()->description();
  std::string caller_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;
  desc = callee()->pc()->local_description()->description();
  std::string callee_ufrag_post_restart =
      desc->transport_infos()[0].description.ice_ufrag;

  ASSERT_NE(caller_candidate_pre_restart, caller_candidate_post_restart);
  ASSERT_NE(callee_candidate_pre_restart, callee_candidate_post_restart);
  ASSERT_NE(caller_ufrag_pre_restart, caller_ufrag_post_restart);
  ASSERT_NE(callee_ufrag_pre_restart, callee_ufrag_post_restart);
  EXPECT_GT(caller()->ice_candidate_pair_change_history().size(), 1u);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, EndToEndCallWithIceRenomination) {
  PeerConnectionInterface::RTCConfiguration config;
  config.enable_ice_renomination = true;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  const cricket::SessionDescription* desc =
      caller()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_THAT(info.description.transport_options, Contains("renomination"));
  }
  desc = callee()->pc()->local_description()->description();
  for (const cricket::TransportInfo& info : desc->transport_infos()) {
    ASSERT_THAT(info.description.transport_options, Contains("renomination"));
  }
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

#if defined(THREAD_SANITIZER)
#define MAYBE_AddMediaToConnectedBundleDoesNotRestartIce \
  DISABLED_AddMediaToConnectedBundleDoesNotRestartIce
#else
#define MAYBE_AddMediaToConnectedBundleDoesNotRestartIce \
  AddMediaToConnectedBundleDoesNotRestartIce
#endif
TEST_P(PeerConnectionIntegrationTest,
       MAYBE_AddMediaToConnectedBundleDoesNotRestartIce) {
  PeerConnectionInterface::RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(
      config, PeerConnectionInterface::RTCConfiguration()));
  ConnectFakeSignaling();

  caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kDefaultTimeout);

  caller()->clear_ice_connection_state_history();

  caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ(0u, caller()->ice_connection_state_history().size());
}

TEST_P(PeerConnectionIntegrationTest,
       VideoFlowsAfterMediaSectionIsRejectedAndRecycled) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 0;
    callee()->SetOfferAnswerOptions(options);
  } else {
    callee()->SetRemoteOfferHandler([this] {
      callee()
          ->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO)
          ->StopInternal();
    });
  }
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  const ContentInfo* answer_video_content = cricket::GetFirstVideoContent(
      callee()->pc()->local_description()->description());
  ASSERT_NE(nullptr, answer_video_content);
  ASSERT_TRUE(answer_video_content->rejected);

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {
    PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_video = 1;
    callee()->SetOfferAnswerOptions(options);
  } else {

    auto caller_transceiver =
        caller()->GetFirstTransceiverOfType(cricket::MEDIA_TYPE_VIDEO);
    EXPECT_EQ(nullptr, caller_transceiver.get());
    caller()->AddVideoTrack();
  }
  callee()->AddVideoTrack();
  callee()->SetRemoteOfferHandler(nullptr);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestPlanB,
       MediaFlowsAfterEarlyWarmupWithCreateSender) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto caller_audio_sender =
      caller()->pc()->CreateSender("audio", "caller_stream");
  auto caller_video_sender =
      caller()->pc()->CreateSender("video", "caller_stream");
  auto callee_audio_sender =
      callee()->pc()->CreateSender("audio", "callee_stream");
  auto callee_video_sender =
      callee()->pc()->CreateSender("video", "callee_stream");
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  EXPECT_TRUE(
      caller_audio_sender->SetTrack(caller()->CreateLocalAudioTrack().get()));
  EXPECT_TRUE(
      caller_video_sender->SetTrack(caller()->CreateLocalVideoTrack().get()));
  EXPECT_TRUE(
      callee_audio_sender->SetTrack(callee()->CreateLocalAudioTrack().get()));
  EXPECT_TRUE(
      callee_video_sender->SetTrack(callee()->CreateLocalVideoTrack().get()));
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       MediaFlowsAfterEarlyWarmupWithAddTransceiver) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  auto audio_result = caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_EQ(RTCErrorType::NONE, audio_result.error().type());
  auto caller_audio_sender = audio_result.MoveValue()->sender();
  auto video_result = caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  ASSERT_EQ(RTCErrorType::NONE, video_result.error().type());
  auto caller_video_sender = video_result.MoveValue()->sender();
  callee()->SetRemoteOfferHandler([this] {
    ASSERT_EQ(2u, callee()->pc()->GetTransceivers().size());
    callee()->pc()->GetTransceivers()[0]->SetDirectionWithError(
        RtpTransceiverDirection::kSendRecv);
    callee()->pc()->GetTransceivers()[1]->SetDirectionWithError(
        RtpTransceiverDirection::kSendRecv);
  });
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 caller()->ice_connection_state(), kMaxWaitForFramesMs);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  auto callee_audio_sender = callee()->pc()->GetSenders()[0];
  auto callee_video_sender = callee()->pc()->GetSenders()[1];
  ASSERT_TRUE(
      caller_audio_sender->SetTrack(caller()->CreateLocalAudioTrack().get()));
  ASSERT_TRUE(
      caller_video_sender->SetTrack(caller()->CreateLocalVideoTrack().get()));
  ASSERT_TRUE(
      callee_audio_sender->SetTrack(callee()->CreateLocalAudioTrack().get()));
  ASSERT_TRUE(
      callee_video_sender->SetTrack(callee()->CreateLocalVideoTrack().get()));
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestPlanB, CanSendRemoteVideoTrack) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);
  ASSERT_EQ(1U, callee()->remote_streams()->count());

  callee()->pc()->AddStream(callee()->remote_streams()->at(0));
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kMaxWaitForActivationMs);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

#if !defined(THREAD_SANITIZER)

TEST_P(PeerConnectionIntegrationTestWithFakeClock,
       EndToEndConnectionTimeWithTurnTurnPair) {
  static constexpr int media_hop_delay_ms = 50;
  static constexpr int signaling_trip_delay_ms = 500;

  static constexpr int required_media_hops = 9;
  static constexpr int required_signaling_trips = 2;

  static constexpr int allowed_internal_delay_ms = 20;
  static constexpr int total_connection_time_ms =
      media_hop_delay_ms * required_media_hops +
      signaling_trip_delay_ms * required_signaling_trips +
      allowed_internal_delay_ms;

  static const rtc::SocketAddress turn_server_1_internal_address{"88.88.88.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_1_external_address{"88.88.88.1",
                                                                 0};
  static const rtc::SocketAddress turn_server_2_internal_address{"99.99.99.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_2_external_address{"99.99.99.1",
                                                                 0};
  cricket::TestTurnServer* turn_server_1 = CreateTurnServer(
      turn_server_1_internal_address, turn_server_1_external_address);

  cricket::TestTurnServer* turn_server_2 = CreateTurnServer(
      turn_server_2_internal_address, turn_server_2_external_address);

  SendTask(network_thread(), [turn_server_1] {
    turn_server_1->set_enable_permission_checks(false);
  });
  SendTask(network_thread(), [turn_server_2] {
    turn_server_2->set_enable_permission_checks(false);
  });

  PeerConnectionInterface::RTCConfiguration client_1_config;
  PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = PeerConnectionInterface::kRelay;
  client_1_config.presume_writable_when_fully_relayed = true;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = PeerConnectionInterface::kRelay;
  client_2_config.presume_writable_when_fully_relayed = true;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(client_1_config, client_2_config));

  SetSignalingDelayMs(signaling_trip_delay_ms);
  ConnectFakeSignaling();
  virtual_socket_server()->set_delay_mean(media_hop_delay_ms);
  virtual_socket_server()->UpdateDelayDistribution();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  EXPECT_TRUE_SIMULATED_WAIT(DtlsConnected(), total_connection_time_ms,
                             FakeClock());

  ClosePeerConnections();
}

TEST_P(PeerConnectionIntegrationTestWithFakeClock,
       OnIceCandidateFlushesGetStatsCache) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();

  rtc::scoped_refptr<const RTCStatsReport> first_report =
      caller()->NewGetStats();
  ASSERT_TRUE(first_report);
  auto first_candidate_stats =
      first_report->GetStatsOfType<RTCLocalIceCandidateStats>();
  ASSERT_EQ(first_candidate_stats.size(), 0u);

  caller()->CreateAndSetAndSignalOffer();

  rtc::scoped_refptr<const RTCStatsReport> second_report =
      caller()->NewGetStats();
  ASSERT_TRUE(second_report);
  auto second_candidate_stats =
      second_report->GetStatsOfType<RTCLocalIceCandidateStats>();
  ASSERT_NE(second_candidate_stats.size(), 0u);

  EXPECT_EQ(first_report->timestamp(), second_report->timestamp());
}

TEST_P(PeerConnectionIntegrationTestWithFakeClock,
       AddIceCandidateFlushesGetStatsCache) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignalingForSdpOnly();
  caller()->AddAudioTrack();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_SIMULATED_WAIT(caller()->IceGatheringStateComplete(),
                             kDefaultTimeout, FakeClock());

  rtc::scoped_refptr<const RTCStatsReport> first_report =
      caller()->NewGetStats();
  ASSERT_TRUE(first_report);
  auto first_candidate_stats =
      first_report->GetStatsOfType<RTCRemoteIceCandidateStats>();
  ASSERT_EQ(first_candidate_stats.size(), 0u);

  absl::optional<RTCError> result;
  caller()->pc()->AddIceCandidate(
      absl::WrapUnique(CreateIceCandidate(
          "", 0,
          "candidate:2214029314 1 udp 2122260223 127.0.0.1 49152 typ host",
          nullptr)),
      [&result](RTCError r) { result = r; });
  ASSERT_TRUE_WAIT(result.has_value(), kDefaultTimeout);
  ASSERT_TRUE(result.value().ok());

  rtc::scoped_refptr<const RTCStatsReport> second_report =
      caller()->NewGetStats();
  ASSERT_TRUE(second_report);
  auto second_candidate_stats =
      second_report->GetStatsOfType<RTCRemoteIceCandidateStats>();
  ASSERT_EQ(second_candidate_stats.size(), 1u);

  EXPECT_EQ(first_report->timestamp(), second_report->timestamp());
}

#endif

TEST_P(PeerConnectionIntegrationTest, TurnCustomizerUsedForTurnConnections) {
  static const rtc::SocketAddress turn_server_1_internal_address{"88.88.88.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_1_external_address{"88.88.88.1",
                                                                 0};
  static const rtc::SocketAddress turn_server_2_internal_address{"99.99.99.0",
                                                                 3478};
  static const rtc::SocketAddress turn_server_2_external_address{"99.99.99.1",
                                                                 0};
  CreateTurnServer(turn_server_1_internal_address,
                   turn_server_1_external_address);
  CreateTurnServer(turn_server_2_internal_address,
                   turn_server_2_external_address);

  PeerConnectionInterface::RTCConfiguration client_1_config;
  PeerConnectionInterface::IceServer ice_server_1;
  ice_server_1.urls.push_back("turn:88.88.88.0:3478");
  ice_server_1.username = "test";
  ice_server_1.password = "test";
  client_1_config.servers.push_back(ice_server_1);
  client_1_config.type = PeerConnectionInterface::kRelay;
  auto* customizer1 = CreateTurnCustomizer();
  client_1_config.turn_customizer = customizer1;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  PeerConnectionInterface::IceServer ice_server_2;
  ice_server_2.urls.push_back("turn:99.99.99.0:3478");
  ice_server_2.username = "test";
  ice_server_2.password = "test";
  client_2_config.servers.push_back(ice_server_2);
  client_2_config.type = PeerConnectionInterface::kRelay;
  auto* customizer2 = CreateTurnCustomizer();
  client_2_config.turn_customizer = customizer2;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(client_1_config, client_2_config));
  ConnectFakeSignaling();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);

  ExpectTurnCustomizerCountersIncremented(customizer1);
  ExpectTurnCustomizerCountersIncremented(customizer2);
}

TEST_P(PeerConnectionIntegrationTest, TCPUsedForTurnConnections) {
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  CreateTurnServer(turn_server_internal_address, turn_server_external_address,
                   cricket::PROTO_TCP);

  PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = PeerConnectionInterface::kRelay;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);
  client_2_config.type = PeerConnectionInterface::kRelay;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(client_1_config, client_2_config));

  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kMaxWaitForFramesMs);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       SSLCertificateVerifierUsedForTurnConnections) {
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  CreateTurnServer(turn_server_internal_address, turn_server_external_address,
                   cricket::PROTO_TLS, "88.88.88.0");

  PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turns:88.88.88.0:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration client_1_config;
  client_1_config.servers.push_back(ice_server);
  client_1_config.type = PeerConnectionInterface::kRelay;

  PeerConnectionInterface::RTCConfiguration client_2_config;
  client_2_config.servers.push_back(ice_server);

  client_2_config.type = PeerConnectionInterface::kRelay;

  rtc::TestCertificateVerifier* client_1_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_1_cert_verifier->verify_certificate_ = true;
  rtc::TestCertificateVerifier* client_2_cert_verifier =
      new rtc::TestCertificateVerifier();
  client_2_cert_verifier->verify_certificate_ = true;

  PeerConnectionDependencies client_1_deps(nullptr);
  client_1_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_1_cert_verifier);
  PeerConnectionDependencies client_2_deps(nullptr);
  client_2_deps.tls_cert_verifier =
      std::unique_ptr<rtc::TestCertificateVerifier>(client_2_cert_verifier);

  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfigAndDeps(
      client_1_config, std::move(client_1_deps), client_2_config,
      std::move(client_2_deps)));
  ConnectFakeSignaling();

  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  options.offer_to_receive_video = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);

  EXPECT_GT(client_1_cert_verifier->call_count_, 0u);
  EXPECT_GT(client_2_cert_verifier->call_count_, 0u);
}

TEST_P(PeerConnectionIntegrationTest, IceTransportFactoryUsedForConnections) {
  PeerConnectionInterface::RTCConfiguration default_config;
  PeerConnectionDependencies dependencies(nullptr);
  auto ice_transport_factory = std::make_unique<MockIceTransportFactory>();
  EXPECT_CALL(*ice_transport_factory, RecordIceTransportCreated()).Times(1);
  dependencies.ice_transport_factory = std::move(ice_transport_factory);
  auto wrapper = CreatePeerConnectionWrapper("Caller", nullptr, &default_config,
                                             std::move(dependencies), nullptr,
                                                                       false,
                                                                       false);
  ASSERT_TRUE(wrapper);
  wrapper->CreateDataChannel();
  auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
  wrapper->pc()->SetLocalDescription(observer.get(),
                                     wrapper->CreateOfferAndWait().release());
}

TEST_P(PeerConnectionIntegrationTest, CodecNamesAreCaseInsensitive) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* description) {
    cricket::AudioContentDescription* audio =
        GetFirstAudioContentDescription(description);
    ASSERT_NE(nullptr, audio);
    auto audio_codecs = audio->codecs();
    audio_codecs.erase(std::remove_if(audio_codecs.begin(), audio_codecs.end(),
                                      [](const cricket::AudioCodec& codec) {
                                        return codec.name != "opus";
                                      }),
                       audio_codecs.end());
    ASSERT_EQ(1u, audio_codecs.size());
    audio_codecs[0].name = "OpUs";
    audio->set_codecs(audio_codecs);

    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(description);
    ASSERT_NE(nullptr, video);
    auto video_codecs = video->codecs();
    video_codecs.erase(std::remove_if(video_codecs.begin(), video_codecs.end(),
                                      [](const cricket::VideoCodec& codec) {
                                        return codec.name != "VP8";
                                      }),
                       video_codecs.end());
    ASSERT_EQ(1u, video_codecs.size());
    video_codecs[0].name = "vP8";
    video->set_codecs(video_codecs);
  });

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, GetSourcesAudio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  ASSERT_EQ(receiver->media_type(), cricket::MEDIA_TYPE_AUDIO);
  auto sources = receiver->GetSources();
  ASSERT_GT(receiver->GetParameters().encodings.size(), 0u);
  EXPECT_EQ(receiver->GetParameters().encodings[0].ssrc,
            sources[0].source_id());
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest, GetSourcesVideo) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo(1);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  ASSERT_EQ(receiver->media_type(), cricket::MEDIA_TYPE_VIDEO);
  auto sources = receiver->GetSources();
  ASSERT_GT(receiver->GetParameters().encodings.size(), 0u);
  ASSERT_GT(sources.size(), 0u);
  EXPECT_EQ(receiver->GetParameters().encodings[0].ssrc,
            sources[0].source_id());
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest, UnsignaledSsrcGetSourcesAudio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  std::vector<RtpSource> sources;
  EXPECT_TRUE_WAIT(([&receiver, &sources]() {
                     sources = receiver->GetSources();
                     return !sources.empty();
                   })(),
                   kDefaultTimeout);
  ASSERT_GT(sources.size(), 0u);
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest, UnsignaledSsrcGetSourcesVideo) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  std::vector<RtpSource> sources;
  EXPECT_TRUE_WAIT(([&receiver, &sources]() {
                     sources = receiver->GetSources();
                     return !sources.empty();
                   })(),
                   kDefaultTimeout);
  ASSERT_GT(sources.size(), 0u);
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest,
       UnsignaledSsrcGetSourcesNonEmptyIfMediaFlowing) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo(1);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  std::vector<RtpSource> sources = receiver->GetSources();

  ASSERT_GT(sources.size(), 0u);
  EXPECT_EQ(RtpSourceType::SSRC, sources[0].source_type());
}

TEST_P(PeerConnectionIntegrationTest, UnsignaledSsrcGetParametersAudio) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  RtpParameters parameters;
  EXPECT_TRUE_WAIT(([&receiver, &parameters]() {
                     parameters = receiver->GetParameters();
                     return !parameters.encodings.empty() &&
                            parameters.encodings[0].ssrc.has_value();
                   })(),
                   kDefaultTimeout);
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_TRUE(parameters.encodings[0].ssrc.has_value());
}

TEST_P(PeerConnectionIntegrationTest, UnsignaledSsrcGetParametersVideo) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  callee()->SetReceivedSdpMunger(RemoveSsrcsAndMsids);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);
  auto receiver = callee()->pc()->GetReceivers()[0];
  RtpParameters parameters;
  EXPECT_TRUE_WAIT(([&receiver, &parameters]() {
                     parameters = receiver->GetParameters();
                     return !parameters.encodings.empty() &&
                            parameters.encodings[0].ssrc.has_value();
                   })(),
                   kDefaultTimeout);
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_TRUE(parameters.encodings[0].ssrc.has_value());
}

TEST_P(PeerConnectionIntegrationTest,
       GetParametersHasEncodingsBeforeNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  auto sender = caller()->AddTrack(caller()->CreateLocalVideoTrack());
  auto parameters = sender->GetParameters();
  EXPECT_EQ(parameters.encodings.size(), 1u);
}

TEST_F(PeerConnectionIntegrationTestPlanB, RemoveAndAddTrackWithNewStreamId) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  rtc::scoped_refptr<AudioTrackInterface> track =
      caller()->CreateLocalAudioTrack();
  rtc::scoped_refptr<RtpSenderInterface> sender =
      caller()->AddTrack(track, {"stream_1"});
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio(1);
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }

  caller()->pc()->RemoveTrackOrError(sender);
  sender = caller()->AddTrack(track, {"stream_2"});
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }
}

TEST_P(PeerConnectionIntegrationTest, RtcEventLogOutputWriteCalled) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  auto output = std::make_unique<NiceMock<MockRtcEventLogOutput>>();
  ON_CALL(*output, IsActive).WillByDefault(Return(true));
  ON_CALL(*output, Write).WillByDefault(Return(true));
  EXPECT_CALL(*output, Write).Times(AtLeast(1));
  EXPECT_TRUE(caller()->pc()->StartRtcEventLog(std::move(output),
                                               RtcEventLog::kImmediateOutput));

  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest, RtcEventLogOutputWriteCalledOnStop) {

  MockFunction<void()> test_is_complete;
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  auto output = std::make_unique<NiceMock<MockRtcEventLogOutput>>();
  ON_CALL(*output, IsActive).WillByDefault(Return(true));
  ON_CALL(*output, Write).WillByDefault(Return(true));
  InSequence s;
  EXPECT_CALL(*output, Write).Times(AtLeast(1));
  EXPECT_CALL(test_is_complete, Call);

  EXPECT_TRUE(caller()->pc()->StartRtcEventLog(std::move(output),
                                                                    100'000));

  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->pc()->StopRtcEventLog();
  test_is_complete.Call();
}

TEST_P(PeerConnectionIntegrationTest, RtcEventLogOutputWriteCalledOnClose) {

  MockFunction<void()> test_is_complete;
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  auto output = std::make_unique<NiceMock<MockRtcEventLogOutput>>();
  ON_CALL(*output, IsActive).WillByDefault(Return(true));
  ON_CALL(*output, Write).WillByDefault(Return(true));
  InSequence s;
  EXPECT_CALL(*output, Write).Times(AtLeast(1));
  EXPECT_CALL(test_is_complete, Call);

  EXPECT_TRUE(caller()->pc()->StartRtcEventLog(std::move(output),
                                                                    100'000));

  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->pc()->Close();
  test_is_complete.Call();
}

TEST_P(PeerConnectionIntegrationTest, MediaFlowsWhenCandidatesSetOnlyInSdp) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());

  ConnectFakeSignalingForSdpOnly();

  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();

  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 caller()->ice_gathering_state(), kDefaultTimeout);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceGatheringComplete,
                 callee()->ice_gathering_state(), kDefaultTimeout);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

#if !defined(THREAD_SANITIZER)

TEST_P(PeerConnectionIntegrationTest, DisableAndEnableAudioPlayout) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->pc()->SetAudioPlayout(false);
  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  WAIT(false, 1000);

  EXPECT_EQ(0, caller()->audio_frames_received());

  ASSERT_GT(callee()->audio_frames_received(), 0);

  caller()->pc()->SetAudioPlayout(true);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

double GetAudioEnergyStat(PeerConnectionIntegrationWrapper* pc) {
  auto report = pc->NewGetStats();
  auto inbound_rtps = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  RTC_CHECK(!inbound_rtps.empty());
  auto* inbound_rtp = inbound_rtps[0];
  if (!inbound_rtp->total_audio_energy.has_value()) {
    return 0.0;
  }
  return *inbound_rtp->total_audio_energy;
}

TEST_P(PeerConnectionIntegrationTest,
       DisableAudioPlayoutStillGeneratesAudioStats) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->pc()->SetAudioPlayout(false);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_TRUE_WAIT(GetAudioEnergyStat(caller()) > 0, kMaxWaitForFramesMs);
}

#endif

TEST_P(PeerConnectionIntegrationTest, DisableAndEnableAudioRecording) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  caller()->pc()->SetAudioRecording(false);
  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  WAIT(false, 1000);

  EXPECT_EQ(0, callee()->audio_frames_received());

  ASSERT_GT(caller()->audio_frames_received(), 0);

  caller()->pc()->SetAudioRecording(true);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest,
       IceEventsGeneratedAndLoggedInRtcEventLog) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithFakeRtcEventLog());
  ConnectFakeSignaling();
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = 1;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->event_log_factory());
  ASSERT_NE(nullptr, callee()->event_log_factory());
  FakeRtcEventLog* caller_event_log =
      caller()->event_log_factory()->last_log_created();
  FakeRtcEventLog* callee_event_log =
      callee()->event_log_factory()->last_log_created();
  ASSERT_NE(nullptr, caller_event_log);
  ASSERT_NE(nullptr, callee_event_log);
  int caller_ice_config_count =
      caller_event_log->GetEventCount(RtcEvent::Type::IceCandidatePairConfig);
  int caller_ice_event_count =
      caller_event_log->GetEventCount(RtcEvent::Type::IceCandidatePairEvent);
  int callee_ice_config_count =
      callee_event_log->GetEventCount(RtcEvent::Type::IceCandidatePairConfig);
  int callee_ice_event_count =
      callee_event_log->GetEventCount(RtcEvent::Type::IceCandidatePairEvent);
  EXPECT_LT(0, caller_ice_config_count);
  EXPECT_LT(0, caller_ice_event_count);
  EXPECT_LT(0, callee_ice_config_count);
  EXPECT_LT(0, callee_ice_event_count);
}

TEST_P(PeerConnectionIntegrationTest, RegatherAfterChangingIceTransportType) {
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  CreateTurnServer(turn_server_internal_address, turn_server_external_address);

  PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.servers.push_back(ice_server);
  caller_config.type = PeerConnectionInterface::kRelay;
  caller_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;
  caller_config.surface_ice_candidates_on_ice_transport_type_changed = true;

  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.servers.push_back(ice_server);
  callee_config.type = PeerConnectionInterface::kRelay;
  callee_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;
  callee_config.surface_ice_candidates_on_ice_transport_type_changed = true;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(caller_config, callee_config));

  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 caller()->ice_connection_state(), kDefaultTimeout);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 callee()->ice_connection_state(), kDefaultTimeout);

  EXPECT_TRUE(caller()->last_candidate_gathered().is_relay());
  EXPECT_TRUE(callee()->last_candidate_gathered().is_relay());

  caller_config = caller()->pc()->GetConfiguration();
  caller_config.type = PeerConnectionInterface::kAll;
  caller()->pc()->SetConfiguration(caller_config);

  EXPECT_TRUE_WAIT(caller()->last_candidate_gathered().is_local(),
                   kDefaultTimeout);

  callee_config = callee()->pc()->GetConfiguration();
  callee_config.type = PeerConnectionInterface::kAll;
  callee()->pc()->SetConfiguration(callee_config);
  EXPECT_TRUE_WAIT(callee()->last_candidate_gathered().is_local(),
                   kDefaultTimeout);

  std::string caller_ufrag_pre_offer = caller()
                                           ->pc()
                                           ->local_description()
                                           ->description()
                                           ->transport_infos()[0]
                                           .description.ice_ufrag;
  caller()->CreateAndSetAndSignalOffer();
  std::string caller_ufrag_post_offer = caller()
                                            ->pc()
                                            ->local_description()
                                            ->description()
                                            ->transport_infos()[0]
                                            .description.ice_ufrag;
  EXPECT_EQ(caller_ufrag_pre_offer, caller_ufrag_post_offer);
}

TEST_P(PeerConnectionIntegrationTest, OnIceCandidateError) {
  static const rtc::SocketAddress turn_server_internal_address{"88.88.88.0",
                                                               3478};
  static const rtc::SocketAddress turn_server_external_address{"88.88.88.1", 0};

  CreateTurnServer(turn_server_internal_address, turn_server_external_address);

  PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:88.88.88.0:3478");
  ice_server.username = "test";
  ice_server.password = "123";

  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.servers.push_back(ice_server);
  caller_config.type = PeerConnectionInterface::kRelay;
  caller_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.servers.push_back(ice_server);
  callee_config.type = PeerConnectionInterface::kRelay;
  callee_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(caller_config, callee_config));

  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(401, caller()->error_event().error_code, kDefaultTimeout);
  EXPECT_EQ("Unauthorized", caller()->error_event().error_text);
  EXPECT_EQ("turn:88.88.88.0:3478?transport=udp", caller()->error_event().url);
  EXPECT_NE(caller()->error_event().address, "");
}

TEST_P(PeerConnectionIntegrationTest, OnIceCandidateErrorWithEmptyAddress) {
  PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.push_back("turn:127.0.0.1:3478?transport=tcp");
  ice_server.username = "test";
  ice_server.password = "test";

  PeerConnectionInterface::RTCConfiguration caller_config;
  caller_config.servers.push_back(ice_server);
  caller_config.type = PeerConnectionInterface::kRelay;
  caller_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  PeerConnectionInterface::RTCConfiguration callee_config;
  callee_config.servers.push_back(ice_server);
  callee_config.type = PeerConnectionInterface::kRelay;
  callee_config.continual_gathering_policy = PeerConnection::GATHER_CONTINUALLY;

  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(caller_config, callee_config));

  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ_WAIT(701, caller()->error_event().error_code, kDefaultTimeout);
  EXPECT_EQ(caller()->error_event().address, "");
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       DISABLED_AudioKeepsFlowingAfterImplicitRollback) {
  PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  SetSignalIceCandidates(false);
  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
  callee()->pc()->SetLocalDescription(observer.get(),
                                      callee()->CreateOfferAndWait().release());
  EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       ImplicitRollbackVisitsStableState) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;

  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));

  auto sld_observer =
      rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
  callee()->pc()->SetLocalDescription(sld_observer.get(),
                                      callee()->CreateOfferAndWait().release());
  EXPECT_TRUE_WAIT(sld_observer->called(), kDefaultTimeout);
  EXPECT_EQ(sld_observer->error(), "");

  auto srd_observer =
      rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
  callee()->pc()->SetRemoteDescription(
      srd_observer.get(), caller()->CreateOfferAndWait().release());
  EXPECT_TRUE_WAIT(srd_observer->called(), kDefaultTimeout);
  EXPECT_EQ(srd_observer->error(), "");

  EXPECT_THAT(callee()->peer_connection_signaling_state_history(),
              ElementsAre(PeerConnectionInterface::kHaveLocalOffer,
                          PeerConnectionInterface::kStable,
                          PeerConnectionInterface::kHaveRemoteOffer));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       H264FmtpSpsPpsIdrInKeyframeParameterUsage) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddVideoTrack();
  callee()->AddVideoTrack();
  auto munger = [](cricket::SessionDescription* desc) {
    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(desc);
    auto codecs = video->codecs();
    for (auto&& codec : codecs) {
      if (codec.name == "H264") {
        std::string value;

        EXPECT_FALSE(
            codec.GetParam(cricket::kH264FmtpSpsPpsIdrInKeyframe, &value));
        codec.SetParam(std::string(cricket::kH264FmtpSpsPpsIdrInKeyframe),
                       std::string(""));
      }
    }
    video->set_codecs(codecs);
  };

  caller()->SetGeneratedSdpMunger(munger);

  caller()->SetReceivedSdpMunger(munger);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* desc) {
    cricket::VideoContentDescription* video =
        GetFirstVideoContentDescription(desc);
    for (auto&& codec : video->codecs()) {
      if (codec.name == "H264") {
        std::string value;
        EXPECT_TRUE(
            codec.GetParam(cricket::kH264FmtpSpsPpsIdrInKeyframe, &value));
      }
    }
  });
  caller()->CreateOfferAndWait();
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       RenegotiateManyAudioTransceivers) {
  PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  int current_size = caller()->pc()->GetTransceivers().size();

  while (current_size < 8) {

    for (int i = 0; i < current_size; i++) {
      caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
    }
    current_size = caller()->pc()->GetTransceivers().size();
    RTC_LOG(LS_INFO) << "Renegotiating with " << current_size << " tracks";
    auto start_time_ms = rtc::TimeMillis();
    caller()->CreateAndSetAndSignalOffer();

    ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
    auto elapsed_time_ms = rtc::TimeMillis() - start_time_ms;
    RTC_LOG(LS_INFO) << "Renegotiating took " << elapsed_time_ms << " ms";
    ASSERT_GT(1000, elapsed_time_ms)
        << "Audio transceivers: Negotiation took too long after "
        << current_size << " tracks added";
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       RenegotiateManyVideoTransceivers) {
  PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  int current_size = caller()->pc()->GetTransceivers().size();

  while (current_size < 8) {

    for (int i = 0; i < current_size; i++) {
      caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
    }
    current_size = caller()->pc()->GetTransceivers().size();
    RTC_LOG(LS_INFO) << "Renegotiating with " << current_size << " tracks";
    auto start_time_ms = rtc::TimeMillis();
    caller()->CreateAndSetAndSignalOffer();

    ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
    auto elapsed_time_ms = rtc::TimeMillis() - start_time_ms;
    RTC_LOG(LS_INFO) << "Renegotiating took " << elapsed_time_ms << " ms";
    ASSERT_GT(1000, elapsed_time_ms)
        << "Video transceivers: Negotiation took too long after "
        << current_size << " tracks added";
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       RenegotiateManyVideoTransceiversAndWatchAudioDelay) {
  PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  callee()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));

  caller()->StartWatchingDelayStats();
  callee()->StartWatchingDelayStats();

  int current_size = caller()->pc()->GetTransceivers().size();

  while (current_size < 16) {

    for (int i = 0; i < current_size; i++) {
      caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
    }
    current_size = caller()->pc()->GetTransceivers().size();
    RTC_LOG(LS_INFO) << "Renegotiating with " << current_size << " tracks";
    auto start_time_ms = rtc::TimeMillis();
    caller()->CreateAndSetAndSignalOffer();

    ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
    auto elapsed_time_ms = rtc::TimeMillis() - start_time_ms;
    RTC_LOG(LS_INFO) << "Renegotiating took " << elapsed_time_ms << " ms";

    ASSERT_GT(5000, elapsed_time_ms)
        << "Video transceivers: Negotiation took too long after "
        << current_size << " tracks added";
    caller()->UpdateDelayStats("caller reception", current_size);
    callee()->UpdateDelayStats("callee reception", current_size);
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       GetParametersHasEncodingsBeforeNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  auto result = caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto transceiver = result.MoveValue();
  auto parameters = transceiver->sender()->GetParameters();
  EXPECT_EQ(parameters.encodings.size(), 1u);
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       GetParametersHasInitEncodingsBeforeNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  RtpTransceiverInit init;
  init.send_encodings.push_back({});
  init.send_encodings[0].max_bitrate_bps = 12345;
  auto result = caller()->pc()->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto transceiver = result.MoveValue();
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  EXPECT_EQ(parameters.encodings[0].max_bitrate_bps, 12345);
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionIntegrationTest,
                         PeerConnectionIntegrationTest,
                         Values(SdpSemantics::kPlanB_DEPRECATED,
                                SdpSemantics::kUnifiedPlan));

INSTANTIATE_TEST_SUITE_P(PeerConnectionIntegrationTest,
                         PeerConnectionIntegrationTestWithFakeClock,
                         Values(SdpSemantics::kPlanB_DEPRECATED,
                                SdpSemantics::kUnifiedPlan));

class PeerConnectionIntegrationInteropTest
    : public PeerConnectionIntegrationBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<SdpSemantics, SdpSemantics>> {
 protected:

  PeerConnectionIntegrationInteropTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kPlanB_DEPRECATED),
        caller_semantics_(std::get<0>(GetParam())),
        callee_semantics_(std::get<1>(GetParam())) {}

  bool CreatePeerConnectionWrappersWithSemantics() {
    return CreatePeerConnectionWrappersWithSdpSemantics(caller_semantics_,
                                                        callee_semantics_);
  }

  const SdpSemantics caller_semantics_;
  const SdpSemantics callee_semantics_;
};

TEST_P(PeerConnectionIntegrationInteropTest, NoMediaLocalToNoMediaRemote) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSemantics());
  ConnectFakeSignaling();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationInteropTest, OneAudioLocalToNoMediaRemote) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSemantics());
  ConnectFakeSignaling();
  auto audio_sender = caller()->AddAudioTrack();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  auto receivers = callee()->pc()->GetReceivers();
  ASSERT_EQ(1u, receivers.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, receivers[0]->media_type());
  EXPECT_EQ(receivers[0]->track()->id(), audio_sender->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest, OneAudioOneVideoToNoMediaRemote) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSemantics());
  ConnectFakeSignaling();
  auto video_sender = caller()->AddVideoTrack();
  auto audio_sender = caller()->AddAudioTrack();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  auto audio_receivers =
      callee()->GetReceiversOfType(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_EQ(1u, audio_receivers.size());
  EXPECT_EQ(audio_receivers[0]->track()->id(), audio_sender->track()->id());
  auto video_receivers =
      callee()->GetReceiversOfType(cricket::MEDIA_TYPE_VIDEO);
  ASSERT_EQ(1u, video_receivers.size());
  EXPECT_EQ(video_receivers[0]->track()->id(), video_sender->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest,
       OneAudioOneVideoLocalToOneAudioOneVideoRemote) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSemantics());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationInteropTest,
       ReverseRolesOneAudioLocalToOneVideoRemote) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSemantics());
  ConnectFakeSignaling();
  caller()->AddAudioTrack();
  callee()->AddVideoTrack();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  EXPECT_EQ(0u, caller()->GetReceiversOfType(cricket::MEDIA_TYPE_VIDEO).size());

  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsSomeVideo();
  media_expectations.CalleeExpectsSomeAudio();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_P(PeerConnectionIntegrationTest, NewTracksDoNotCauseNewCandidates) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
  caller()->ExpectCandidates(0);
  callee()->ExpectCandidates(0);
  caller()->AddAudioTrack();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
}

TEST_P(PeerConnectionIntegrationTest, MediaCallWithoutMediaEngineFails) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithoutMediaEngine());

  EXPECT_FALSE(
      caller()->pc()->AddTrack(caller()->CreateLocalAudioTrack(), {}).ok());
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionIntegrationTest,
    PeerConnectionIntegrationInteropTest,
    Values(std::make_tuple(SdpSemantics::kPlanB_DEPRECATED,
                           SdpSemantics::kUnifiedPlan),
           std::make_tuple(SdpSemantics::kUnifiedPlan,
                           SdpSemantics::kPlanB_DEPRECATED)));

TEST_F(PeerConnectionIntegrationTestPlanB, TwoVideoUnifiedPlanToNoMediaPlanB) {
  ASSERT_TRUE(CreatePeerConnectionWrappersWithSdpSemantics(
      SdpSemantics::kUnifiedPlan, SdpSemantics::kPlanB_DEPRECATED));
  ConnectFakeSignaling();
  auto first_sender = caller()->AddVideoTrack();
  caller()->AddVideoTrack();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  auto receivers = callee()->pc()->GetReceivers();
  ASSERT_EQ(1u, receivers.size());
  EXPECT_TRUE(receivers[0]->track()->enabled());
  EXPECT_EQ(first_sender->track()->id(), receivers[0]->track()->id());

  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       ReOfferWithStoppedBundleTaggedTransceiver) {
  RTCConfiguration config;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto audio_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeAudio();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }

  audio_transceiver->StopInternal();
  caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  {
    MediaExpectations media_expectations;
    media_expectations.CalleeExpectsSomeVideo();
    ASSERT_TRUE(ExpectNewFrames(media_expectations));
  }
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       StopTransceiverRemovesDtlsTransports) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto audio_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  audio_transceiver->StopStandard();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(0U, caller()->pc()->GetTransceivers().size());
  EXPECT_EQ(PeerConnectionInterface::kIceGatheringNew,
            caller()->pc()->ice_gathering_state());
  EXPECT_THAT(caller()->ice_gathering_state_history(),
              ElementsAre(PeerConnectionInterface::kIceGatheringGathering,
                          PeerConnectionInterface::kIceGatheringComplete,
                          PeerConnectionInterface::kIceGatheringNew));
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       StopTransceiverStopsAndRemovesTransceivers) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto caller_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  caller_transceiver->StopStandard();

  auto callee_transceiver = callee()->pc()->GetTransceivers()[0];
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ(0U, caller()->pc()->GetTransceivers().size());
  EXPECT_EQ(0U, callee()->pc()->GetTransceivers().size());
  EXPECT_EQ(0U, caller()->pc()->GetSenders().size());
  EXPECT_EQ(0U, callee()->pc()->GetSenders().size());
  EXPECT_EQ(0U, caller()->pc()->GetReceivers().size());
  EXPECT_EQ(0U, callee()->pc()->GetReceivers().size());
  EXPECT_TRUE(caller_transceiver->stopped());
  EXPECT_TRUE(callee_transceiver->stopped());
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       StopTransceiverEndsIncomingAudioTrack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto audio_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  auto caller_track = audio_transceiver->receiver()->track();
  auto callee_track = callee()->pc()->GetReceivers()[0]->track();
  audio_transceiver->StopStandard();
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kEnded,
            caller_track->state());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kEnded,
            callee_track->state());
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       StopTransceiverEndsIncomingVideoTrack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto audio_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  auto caller_track = audio_transceiver->receiver()->track();
  auto callee_track = callee()->pc()->GetReceivers()[0]->track();
  audio_transceiver->StopStandard();
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kEnded,
            caller_track->state());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kEnded,
            callee_track->state());
}

TEST_P(PeerConnectionIntegrationTest, EndToEndRtpSenderVideoEncoderSelector) {
  ASSERT_TRUE(
      CreateOneDirectionalPeerConnectionWrappers(                     true));
  ConnectFakeSignaling();

  rtc::scoped_refptr<VideoTrackInterface> caller_track =
      caller()->CreateLocalVideoTrack();
  auto sender = caller()->AddTrack(caller_track);
  PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_video = 0;
  caller()->SetOfferAnswerOptions(options);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_EQ(callee()->pc()->GetReceivers().size(), 1u);

  std::unique_ptr<MockEncoderSelector> encoder_selector =
      std::make_unique<MockEncoderSelector>();
  EXPECT_CALL(*encoder_selector, OnCurrentEncoder);

  sender->SetEncoderSelector(std::move(encoder_selector));

  MediaExpectations media_expectations;
  media_expectations.CallerExpectsNoVideo();
  media_expectations.CalleeExpectsSomeVideo();

  EXPECT_TRUE(ExpectNewFrames(media_expectations));
}

int NacksReceivedCount(PeerConnectionIntegrationWrapper& pc) {
  rtc::scoped_refptr<const RTCStatsReport> report = pc.NewGetStats();
  auto sender_stats = report->GetStatsOfType<RTCOutboundRtpStreamStats>();
  if (sender_stats.size() != 1) {
    ADD_FAILURE();
    return 0;
  }
  if (!sender_stats[0]->nack_count.has_value()) {
    return 0;
  }
  return *sender_stats[0]->nack_count;
}

int NacksSentCount(PeerConnectionIntegrationWrapper& pc) {
  rtc::scoped_refptr<const RTCStatsReport> report = pc.NewGetStats();
  auto receiver_stats = report->GetStatsOfType<RTCInboundRtpStreamStats>();
  if (receiver_stats.size() != 1) {
    ADD_FAILURE();
    return 0;
  }
  if (!receiver_stats[0]->nack_count.has_value()) {
    return 0;
  }
  return *receiver_stats[0]->nack_count;
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan,
       DISABLED_AudioPacketLossCausesNack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto audio_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalAudioTrack());
  ASSERT_TRUE(audio_transceiver_or_error.ok());
  auto send_transceiver = audio_transceiver_or_error.MoveValue();

  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* desc) {
    for (ContentInfo& content : desc->contents()) {
      cricket::MediaContentDescription* media = content.media_description();
      std::vector<cricket::Codec> codecs = media->codecs();
      std::vector<cricket::Codec> codecs_out;
      for (cricket::Codec codec : codecs) {
        if (codec.name == "opus") {
          codec.AddFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamNack, cricket::kParamValueEmpty));
          codec.AddFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamRrtr, cricket::kParamValueEmpty));
          codecs_out.push_back(codec);
        }
      }
      EXPECT_FALSE(codecs_out.empty());
      media->set_codecs(codecs_out);
    }
  });

  caller()->CreateAndSetAndSignalOffer();

  ASSERT_FALSE(HasFailure());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudio(1);
  ExpectNewFrames(media_expectations);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.2);

  EXPECT_TRUE_WAIT(NacksSentCount(*callee()) > 0, kDefaultTimeout);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.0);

  EXPECT_TRUE_WAIT(NacksReceivedCount(*caller()) > 0, kDefaultTimeout);
}

TEST_F(PeerConnectionIntegrationTestUnifiedPlan, VideoPacketLossCausesNack) {
  RTCConfiguration config;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(config, config));
  ConnectFakeSignaling();
  auto video_transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack());
  ASSERT_TRUE(video_transceiver_or_error.ok());
  auto send_transceiver = video_transceiver_or_error.MoveValue();

  caller()->SetGeneratedSdpMunger([](cricket::SessionDescription* desc) {
    for (ContentInfo& content : desc->contents()) {
      cricket::MediaContentDescription* media = content.media_description();
      std::vector<cricket::Codec> codecs = media->codecs();
      std::vector<cricket::Codec> codecs_out;
      for (cricket::Codec codec : codecs) {
        if (codec.name == "VP8") {
          ASSERT_TRUE(codec.HasFeedbackParam(cricket::FeedbackParam(
              cricket::kRtcpFbParamNack, cricket::kParamValueEmpty)));
          codecs_out.push_back(codec);
        }
      }
      EXPECT_FALSE(codecs_out.empty());
      media->set_codecs(codecs_out);
    }
  });

  caller()->CreateAndSetAndSignalOffer();

  ASSERT_FALSE(HasFailure());
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeVideo(1);
  ExpectNewFrames(media_expectations);
  ASSERT_FALSE(HasFailure());

  virtual_socket_server()->set_drop_probability(0.2);

  EXPECT_TRUE_WAIT(NacksSentCount(*callee()) > 0, kDefaultTimeout);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE_WAIT(NacksReceivedCount(*caller()) > 0, kDefaultTimeout);
}

}

}
