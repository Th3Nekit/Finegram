/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/uma_metrics.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "media/base/stream_params.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/port_allocator.h"
#include "pc/media_session.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/sdp_utils.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/integration_test_helpers.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

template <typename MethodFunctor>
class OnSuccessObserver : public SetRemoteDescriptionObserverInterface {
 public:
  explicit OnSuccessObserver(MethodFunctor on_success)
      : on_success_(std::move(on_success)) {}

  void OnSetRemoteDescriptionComplete(RTCError error) override {
    RTC_CHECK(error.ok());
    on_success_();
  }

 private:
  MethodFunctor on_success_;
};

class PeerConnectionRtpBaseTest : public ::testing::Test {
 public:
  explicit PeerConnectionRtpBaseTest(SdpSemantics sdp_semantics)
      : sdp_semantics_(sdp_semantics),
        pc_factory_(CreatePeerConnectionFactory(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            FakeAudioCaptureModule::Create(),
            CreateBuiltinAudioEncoderFactory(),
            CreateBuiltinAudioDecoderFactory(),
            std::make_unique<
                VideoEncoderFactoryTemplate<LibvpxVp8EncoderTemplateAdapter,
                                            LibvpxVp9EncoderTemplateAdapter,
                                            OpenH264EncoderTemplateAdapter,
                                            LibaomAv1EncoderTemplateAdapter>>(),
            std::make_unique<
                VideoDecoderFactoryTemplate<LibvpxVp8DecoderTemplateAdapter,
                                            LibvpxVp9DecoderTemplateAdapter,
                                            OpenH264DecoderTemplateAdapter,
                                            Dav1dDecoderTemplateAdapter>>(),
            nullptr                  ,
            nullptr                       )) {
    metrics::Reset();
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithPlanB() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kPlanB_DEPRECATED;
    return CreatePeerConnectionInternal(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithUnifiedPlan() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnectionInternal(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config) {
    RTCConfiguration modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    return CreatePeerConnectionInternal(modified_config);
  }

 protected:
  const SdpSemantics sdp_semantics_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;

 private:

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionInternal(
      const RTCConfiguration& config) {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    auto result = pc_factory_->CreatePeerConnectionOrError(
        config, PeerConnectionDependencies(observer.get()));
    EXPECT_TRUE(result.ok());
    observer->SetPeerConnectionInterface(result.value().get());
    return std::make_unique<PeerConnectionWrapper>(
        pc_factory_, result.MoveValue(), std::move(observer));
  }

  rtc::AutoThread main_thread_;
};

class PeerConnectionRtpTest
    : public PeerConnectionRtpBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionRtpTest() : PeerConnectionRtpBaseTest(GetParam()) {}
};

class PeerConnectionRtpTestPlanB : public PeerConnectionRtpBaseTest {
 protected:
  PeerConnectionRtpTestPlanB()
      : PeerConnectionRtpBaseTest(SdpSemantics::kPlanB_DEPRECATED) {}
};

class PeerConnectionRtpTestUnifiedPlan : public PeerConnectionRtpBaseTest {
 protected:
  PeerConnectionRtpTestUnifiedPlan()
      : PeerConnectionRtpBaseTest(SdpSemantics::kUnifiedPlan) {}

  bool ExchangeOfferAnswerWhereRemoteStopsTransceiver(
      PeerConnectionWrapper* caller,
      PeerConnectionWrapper* callee,
      size_t mid_to_stop) {
    auto offer = caller->CreateOffer();
    caller->SetLocalDescription(CloneSessionDescription(offer.get()));
    callee->SetRemoteDescription(std::move(offer));
    EXPECT_LT(mid_to_stop, callee->pc()->GetTransceivers().size());

    callee->pc()->GetTransceivers()[mid_to_stop]->StopInternal();
    auto answer = callee->CreateAnswer();
    EXPECT_TRUE(answer);
    bool set_local_answer =
        callee->SetLocalDescription(CloneSessionDescription(answer.get()));
    EXPECT_TRUE(set_local_answer);
    bool set_remote_answer = caller->SetRemoteDescription(std::move(answer));
    EXPECT_TRUE(set_remote_answer);
    return set_remote_answer;
  }
};

TEST_P(PeerConnectionRtpTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddAudioTrack("audio_track"));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  const auto& add_track_event = callee->observer()->add_track_events_[0];
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    ASSERT_EQ(1u, add_track_event.streams.size());
    EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  } else {
    EXPECT_EQ(0u, add_track_event.streams.size());
  }
}

TEST_P(PeerConnectionRtpTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddAudioTrack("audio_track", {"audio_stream"}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_EQ("audio_stream", add_track_event.streams[0]->id());
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {"audio_stream"});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
  EXPECT_EQ(0u, callee->observer()->remote_streams()->count());
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamId[] = "shared_audio_stream";
  auto sender1 = caller->AddAudioTrack("audio_track1", {kSharedStreamId});
  auto sender2 = caller->AddAudioTrack("audio_track2", {kSharedStreamId});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender1).ok());
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);
  ASSERT_EQ(1u, callee->observer()->remote_streams()->count());
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender2).ok());
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
  EXPECT_EQ(0u, callee->observer()->remote_streams()->count());
}

TEST_F(PeerConnectionRtpTestPlanB,
       RemoteStreamIdChangesFiresOnRemoveAndOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddAudioTrack("audio_track1", {kStreamId1});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);

  auto offer = caller->CreateOfferAndSetAsLocal();
  auto* audio_desc =
      cricket::GetFirstAudioContentDescription(offer->description());
  ASSERT_EQ(audio_desc->mutable_streams().size(), 1u);
  audio_desc->mutable_streams()[0].set_stream_ids({kStreamId2});
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->add_track_events_[1].streams[0]->id(),
            kStreamId2);
  ASSERT_EQ(callee->observer()->remove_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_[0]->streams()[0]->id(),
            kStreamId1);
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTransceiverCallsOnTrack) {
  const std::string kStreamId1 = "video_stream1";
  const std::string kStreamId2 = "video_stream2";
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  RtpTransceiverInit video_transceiver_init;
  video_transceiver_init.stream_ids = {kStreamId1, kStreamId2};
  auto video_transceiver =
      caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, video_transceiver_init);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  ASSERT_EQ(2u, callee->observer()->on_track_transceivers_.size());
  EXPECT_EQ(audio_transceiver->mid(),
            callee->pc()->GetTransceivers()[0]->mid());
  EXPECT_EQ(video_transceiver->mid(),
            callee->pc()->GetTransceivers()[1]->mid());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> audio_streams =
      callee->pc()->GetTransceivers()[0]->receiver()->streams();
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> video_streams =
      callee->pc()->GetTransceivers()[1]->receiver()->streams();
  ASSERT_EQ(0u, audio_streams.size());
  ASSERT_EQ(2u, video_streams.size());
  EXPECT_EQ(kStreamId1, video_streams[0]->id());
  EXPECT_EQ(kStreamId2, video_streams[1]->id());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, ReofferDoesNotCallOnTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  caller->AddAudioTrack("audio");
  callee->AddAudioTrack("audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, SetDirectionCallsOnTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_TRUE(
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive)
          .ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(0u, callee->observer()->on_track_transceivers_.size());

  EXPECT_TRUE(
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendOnly)
          .ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  EXPECT_TRUE(
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv)
          .ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, SetDirectionHoldCallsOnTrackTwice) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  EXPECT_TRUE(callee->pc()
                  ->GetTransceivers()[0]
                  ->SetDirectionWithError(RtpTransceiverDirection::kInactive)
                  .ok());

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  EXPECT_TRUE(callee->pc()
                  ->GetTransceivers()[0]
                  ->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly)
                  .ok());

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(2u, callee->observer()->on_track_transceivers_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       ApplyTwoRemoteOffersWithNoAnswerResultsInOneAddTrackEvent) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  caller->AddAudioTrack("audio_track", {});

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(1u, callee->observer()->add_track_events_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       ApplyRemoteOfferAddThenRemoteOfferRemoveResultsInOneRemoveTrackEvent) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {});

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(0u, callee->observer()->remove_track_events_.size());

  caller->pc()->RemoveTrackOrError(sender);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(1u, callee->observer()->remove_track_events_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       ChangeDirectionInAnswerResultsInRemoveTrackEvent) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio_track", {});

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(0u, callee->observer()->remove_track_events_.size());

  auto callee_transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_TRUE(callee_transceiver
                  ->SetDirectionWithError(RtpTransceiverDirection::kSendOnly)
                  .ok());

  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
  EXPECT_EQ(1u, callee->observer()->add_track_events_.size());
  EXPECT_EQ(1u, callee->observer()->remove_track_events_.size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, ChangeMsidWhileReceiving) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio_track", {"stream1"});
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->observer()->on_track_transceivers_.size());
  auto transceiver = callee->observer()->on_track_transceivers_[0];
  ASSERT_EQ(1u, transceiver->receiver()->streams().size());
  EXPECT_EQ("stream1", transceiver->receiver()->streams()[0]->id());

  ASSERT_TRUE(callee->CreateAnswerAndSetAsLocal());

  caller->pc()->GetSenders()[0]->SetStreams({"stream2"});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(1u, transceiver->receiver()->streams().size());
  EXPECT_EQ("stream2", transceiver->receiver()->streams()[0]->id());
}

TEST_P(PeerConnectionRtpTest, AddTrackWithoutStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddAudioTrack("audio_track", {}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    ASSERT_EQ(1u, receiver_added->streams().size());
    EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
  } else {
    EXPECT_EQ(0u, receiver_added->streams().size());
  }
}

TEST_P(PeerConnectionRtpTest, AddTrackWithStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddAudioTrack("audio_track", {"audio_stream"}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_EQ("audio_stream", receiver_added->streams()[0]->id());
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithoutStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
    EXPECT_EQ(RtpTransceiverDirection::kInactive,
              callee->pc()->GetTransceivers()[0]->current_direction());
  } else {

    ASSERT_EQ(0u, callee->pc()->GetReceivers().size());
  }
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {"audio_stream"});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    EXPECT_EQ(1u, callee->pc()->GetReceivers().size());
    EXPECT_EQ(RtpTransceiverDirection::kInactive,
              callee->pc()->GetTransceivers()[0]->current_direction());
  } else {

    EXPECT_EQ(0u, callee->pc()->GetReceivers().size());
  }
}

TEST_P(PeerConnectionRtpTest, RemoveTrackWithSharedStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamId[] = "shared_audio_stream";
  auto sender1 = caller->AddAudioTrack("audio_track1", {kSharedStreamId});
  auto sender2 = caller->AddAudioTrack("audio_track2", {kSharedStreamId});
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  ASSERT_EQ(2u, callee->pc()->GetReceivers().size());

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender1).ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    ASSERT_EQ(2u, callee->pc()->GetReceivers().size());
    auto transceiver = callee->pc()->GetTransceivers()[0];
    EXPECT_EQ("audio_track1", transceiver->receiver()->track()->id());
    EXPECT_EQ(RtpTransceiverDirection::kInactive,
              transceiver->current_direction());
  } else {

    ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
    EXPECT_EQ("audio_track2", callee->pc()->GetReceivers()[0]->track()->id());
  }

  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender2).ok());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  if (sdp_semantics_ == SdpSemantics::kUnifiedPlan) {

    ASSERT_EQ(2u, callee->pc()->GetReceivers().size());
    auto transceiver = callee->pc()->GetTransceivers()[1];
    EXPECT_EQ("audio_track2", transceiver->receiver()->track()->id());
    EXPECT_EQ(RtpTransceiverDirection::kInactive,
              transceiver->current_direction());
  } else {

    ASSERT_EQ(0u, callee->pc()->GetReceivers().size());
  }
}

TEST_P(PeerConnectionRtpTest, AudioGetParametersHasHeaderExtensions) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto sender = caller->AddAudioTrack("audio_track");
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_GT(caller->pc()->GetSenders().size(), 0u);
  EXPECT_GT(sender->GetParameters().header_extensions.size(), 0u);

  ASSERT_GT(callee->pc()->GetReceivers().size(), 0u);
  auto receiver = callee->pc()->GetReceivers()[0];
  EXPECT_GT(receiver->GetParameters().header_extensions.size(), 0u);
}

TEST_P(PeerConnectionRtpTest, VideoGetParametersHasHeaderExtensions) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto sender = caller->AddVideoTrack("video_track");
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_GT(caller->pc()->GetSenders().size(), 0u);
  EXPECT_GT(sender->GetParameters().header_extensions.size(), 0u);

  ASSERT_GT(callee->pc()->GetReceivers().size(), 0u);
  auto receiver = callee->pc()->GetReceivers()[0];
  EXPECT_GT(receiver->GetParameters().header_extensions.size(), 0u);
}

TEST_F(PeerConnectionRtpTestPlanB,
       StatesCorrelateWithSetRemoteDescriptionCall) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("audio_track", {});
  auto srd1_sdp = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  auto srd2_sdp = caller->CreateOfferAndSetAsLocal();

  auto pc = callee->pc();
  bool srd1_callback_called = false;
  auto srd1_callback = [&srd1_callback_called, &pc]() {
    EXPECT_EQ(pc->GetReceivers().size(), 1u);
    srd1_callback_called = true;
  };

  bool srd2_callback_called = false;
  auto srd2_callback = [&srd2_callback_called, &pc]() {
    EXPECT_TRUE(pc->GetReceivers().empty());
    srd2_callback_called = true;
  };

  callee->pc()->SetRemoteDescription(
      std::move(srd1_sdp),
      rtc::make_ref_counted<OnSuccessObserver<decltype(srd1_callback)>>(
          srd1_callback));
  callee->pc()->SetRemoteDescription(
      std::move(srd2_sdp),
      rtc::make_ref_counted<OnSuccessObserver<decltype(srd2_callback)>>(
          srd2_callback));
  EXPECT_TRUE_WAIT(srd1_callback_called, kDefaultTimeout);
  EXPECT_TRUE_WAIT(srd2_callback_called, kDefaultTimeout);
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, UnsignaledSsrcCreatesReceiverStreams) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                   {kStreamId1, kStreamId2});

  auto offer = caller->CreateOfferAndSetAsLocal();

  auto contents = offer->description()->contents();
  ASSERT_TRUE(!contents.empty());
  ASSERT_TRUE(!contents[0].media_description()->streams().empty());
  std::vector<std::string> stream_ids =
      contents[0].media_description()->streams()[0].stream_ids();
  contents[0].media_description()->mutable_streams().clear();
  cricket::StreamParams new_stream;
  new_stream.set_stream_ids(stream_ids);
  contents[0].media_description()->AddStream(new_stream);

  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));
  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 1u);
  ASSERT_EQ(receivers[0]->streams().size(), 2u);
  EXPECT_EQ(receivers[0]->streams()[0]->id(), kStreamId1);
  EXPECT_EQ(receivers[0]->streams()[1]->id(), kStreamId2);
}
TEST_F(PeerConnectionRtpTestUnifiedPlan, TracksDoNotEndWhenSsrcChanges) {
  constexpr uint32_t kFirstMungedSsrc = 1337u;

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kRecvOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);

  callee->AddTrack(callee->CreateAudioTrack("audio_track"), {});
  callee->AddTrack(callee->CreateVideoTrack("video_track"), {});

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
  ASSERT_EQ(caller->observer()->add_track_events_.size(), 2u);
  ASSERT_EQ(caller->pc()->GetReceivers().size(), 2u);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer();
  auto& contents = answer->description()->contents();
  ASSERT_TRUE(!contents.empty());
  for (size_t i = 0; i < contents.size(); ++i) {
    auto& mutable_streams = contents[i].media_description()->mutable_streams();
    ASSERT_EQ(mutable_streams.size(), 1u);
    ReplaceFirstSsrc(mutable_streams[0],
                     kFirstMungedSsrc + static_cast<uint32_t>(i));
  }
  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(
      caller->SetRemoteDescription(CloneSessionDescription(answer.get())));

  ASSERT_EQ(caller->observer()->add_track_events_.size(), 2u);

  auto receivers = caller->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 2u);

  EXPECT_EQ(receivers[0]->track()->state(),
            MediaStreamTrackInterface::TrackState::kLive);
  EXPECT_EQ(receivers[1]->track()->state(),
            MediaStreamTrackInterface::TrackState::kLive);
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       DISABLED_RemoteStreamIdChangesUpdatesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddAudioTrack("audio_track1", {kStreamId1});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);

  auto offer = caller->CreateOfferAndSetAsLocal();
  auto contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  ASSERT_EQ(contents[0].media_description()->mutable_streams().size(), 1u);
  contents[0].media_description()->mutable_streams()[0].set_stream_ids(
      {kStreamId2});

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 1u);
  ASSERT_EQ(receivers[0]->streams().size(), 1u);
  EXPECT_EQ(receivers[0]->streams()[0]->id(), kStreamId2);
}

TEST_F(PeerConnectionRtpTestPlanB,
       MultipleRemoteSendersWithoutStreamParamIdAddsMultipleReceivers) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddAudioTrack("audio_track1", {kStreamId1});
  caller->AddAudioTrack("audio_track2", {kStreamId2});

  auto offer = caller->CreateOfferAndSetAsLocal();
  auto mutable_streams =
      cricket::GetFirstAudioContentDescription(offer->description())
          ->mutable_streams();
  ASSERT_EQ(mutable_streams.size(), 2u);

  mutable_streams[0].id.clear();
  mutable_streams[1].id.clear();
  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));

  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 2u);
  ASSERT_EQ(receivers[0]->streams().size(), 1u);
  EXPECT_EQ(kStreamId1, receivers[0]->streams()[0]->id());
  ASSERT_EQ(receivers[1]->streams().size(), 1u);
  EXPECT_EQ(kStreamId2, receivers[1]->streams()[0]->id());
}

TEST_P(PeerConnectionRtpTest, LegacyObserverOnSuccess) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  std::string error;
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(), &error));
}

TEST_P(PeerConnectionRtpTest,
       LegacyObserverNotCalledIfPeerConnectionDereferenced) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer =
      rtc::make_ref_counted<MockSetSessionDescriptionObserver>();

  auto offer = caller->CreateOfferAndSetAsLocal();
  callee->pc()->SetRemoteDescription(observer.get(), offer.release());
  callee = nullptr;
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_FALSE(observer->called());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, PeerConnectionHasNoTransceivers) {
  auto caller = CreatePeerConnection();
  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTransceiverHasCorrectInitProperties) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(absl::nullopt, transceiver->mid());
  EXPECT_FALSE(transceiver->stopped());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
  EXPECT_EQ(absl::nullopt, transceiver->current_direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddAudioTransceiverCreatesAudioSenderAndReceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->media_type());

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddAudioTransceiverCreatesVideoSenderAndReceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->media_type());

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kVideoKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTransceiverShowsInLists) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>{transceiver},
      caller->pc()->GetTransceivers());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpSenderInterface>>{
          transceiver->sender()},
      caller->pc()->GetSenders());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          transceiver->receiver()},
      caller->pc()->GetReceivers());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTransceiverWithDirectionIsReflected) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTransceiverWithTrackCreatesSenderWithTrack) {
  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack("audio track");
  auto transceiver = caller->AddTransceiver(audio_track);

  auto sender = transceiver->sender();
  ASSERT_TRUE(sender->track());
  EXPECT_EQ(audio_track, sender->track());

  auto receiver = transceiver->receiver();
  ASSERT_TRUE(receiver->track());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, receiver->track()->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive,
            receiver->track()->state());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTransceiverTwiceWithSameTrackCreatesMultipleTransceivers) {
  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack("audio track");

  auto transceiver1 = caller->AddTransceiver(audio_track);
  auto transceiver2 = caller->AddTransceiver(audio_track);

  EXPECT_NE(transceiver1, transceiver2);

  auto sender1 = transceiver1->sender();
  auto sender2 = transceiver2->sender();
  EXPECT_NE(sender1, sender2);
  EXPECT_EQ(audio_track, sender1->track());
  EXPECT_EQ(audio_track, sender2->track());

  EXPECT_THAT(caller->pc()->GetTransceivers(),
              UnorderedElementsAre(transceiver1, transceiver2));
  EXPECT_THAT(caller->pc()->GetSenders(),
              UnorderedElementsAre(sender1, sender2));
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTransceiverWithInvalidKindReturnsError) {
  auto caller = CreatePeerConnection();

  auto result = caller->pc()->AddTransceiver(cricket::MEDIA_TYPE_DATA);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CanClosePeerConnectionWithoutCrashing) {
  auto caller = CreatePeerConnection();

  caller->pc()->Close();
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddAudioTrackCreatesAudioSender) {
  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, sender->media_type());
  EXPECT_EQ(audio_track, sender->track());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddVideoTrackCreatesVideoSender) {
  auto caller = CreatePeerConnection();

  auto video_track = caller->CreateVideoTrack("a");
  auto sender = caller->AddTrack(video_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, sender->media_type());
  EXPECT_EQ(video_track, sender->track());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddFirstTrackCreatesTransceiver) {
  auto caller = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceivers[0]->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTrackReusesTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(transceiver, transceivers[0]);
  EXPECT_EQ(sender, transceiver->sender());
  EXPECT_EQ(audio_track, sender->track());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTrackWithSendEncodingDoesNotReuseTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto audio_track = caller->CreateAudioTrack("a");
  RtpEncodingParameters encoding;
  auto sender = caller->AddTrack(audio_track, {}, {encoding});
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(transceiver, transceivers[0]);
  EXPECT_NE(sender, transceiver->sender());
  EXPECT_EQ(audio_track, sender->track());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, TwoAddTrackCreatesTwoTransceivers) {
  auto caller = CreatePeerConnection();

  auto sender1 = caller->AddAudioTrack("a");
  auto sender2 = caller->AddVideoTrack("v");
  ASSERT_TRUE(sender2);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender1, transceivers[0]->sender());
  EXPECT_EQ(sender2, transceivers[1]->sender());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTrackReusesTransceiverOfType) {
  auto caller = CreatePeerConnection();

  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto sender = caller->AddVideoTrack("v");

  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
  EXPECT_NE(sender, audio_transceiver->sender());
  EXPECT_EQ(sender, video_transceiver->sender());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTrackDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnection();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddVideoTrack("v");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_NE(sender, transceivers[0]->sender());
  EXPECT_EQ(sender, transceivers[1]->sender());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTrackReusesFirstMatchingTransceiver) {
  auto caller = CreatePeerConnection();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddAudioTrack("a");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_NE(sender, transceivers[1]->sender());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTrackChangesDirectionFromInactiveToSendOnly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kInactive;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddTrackChangesDirectionFromRecvOnlyToSendRecv) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kRecvOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTrackCreatesSenderWithTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack(kTrackId);
  auto sender = caller->AddTrack(audio_track);

  EXPECT_EQ(kTrackId, sender->id());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTrackErrorIfClosed) {
  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  auto result = caller->pc()->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.error().type());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddTrackErrorIfTrackAlreadyHasSender) {
  auto caller = CreatePeerConnection();

  auto audio_track = caller->CreateAudioTrack("a");
  ASSERT_TRUE(caller->AddTrack(audio_track));

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  auto result = caller->pc()->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, RemoveTrackClearsSenderTrack) {
  auto caller = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());

  EXPECT_FALSE(sender->track());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       RemoveTrackChangesDirectionFromSendRecvToRecvOnly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendRecv;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();

  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(transceiver->sender()).ok());
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       RemoveTrackChangesDirectionFromSendOnlyToInactive) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();

  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(transceiver->sender()).ok());
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  EXPECT_EQ(RtpTransceiverDirection::kInactive, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, RemoveTrackWithNullSenderTrackIsNoOp) {
  auto caller = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("a");
  auto transceiver = caller->pc()->GetTransceivers()[0];
  ASSERT_TRUE(sender->SetTrack(nullptr));

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, RemoveTrackErrorIfClosed) {
  auto caller = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  EXPECT_FALSE(caller->pc()->RemoveTrackOrError(sender).ok());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       RemoveTrackNoErrorIfTrackAlreadyRemoved) {
  auto caller = CreatePeerConnection();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  EXPECT_TRUE(caller->pc()->RemoveTrackOrError(sender).ok());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, AddRemoveAddTrackOffersWorksAudio) {
  auto caller = CreatePeerConnection();

  auto sender1 = caller->AddAudioTrack("audio1");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  caller->pc()->RemoveTrackOrError(sender1);
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto sender2 = caller->AddAudioTrack("audio2");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(sender1, sender2);
}
TEST_F(PeerConnectionRtpTestUnifiedPlan, AddRemoveAddTrackOffersWorksVideo) {
  auto caller = CreatePeerConnection();

  auto sender1 = caller->AddVideoTrack("video1");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  caller->pc()->RemoveTrackOrError(sender1);
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto sender2 = caller->AddVideoTrack("video2");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(sender1, sender2);
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, CreateOfferSameTrackLabel) {
  auto caller = CreatePeerConnection();

  auto audio_sender = caller->AddAudioTrack("track", {});
  auto video_sender = caller->AddVideoTrack("track", {});

  EXPECT_TRUE(caller->CreateOffer());

  EXPECT_EQ(audio_sender->track()->id(), video_sender->track()->id());
  EXPECT_NE(audio_sender->id(), video_sender->id());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, CreateAnswerSameTrackLabel) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit recvonly;
  recvonly.direction = RtpTransceiverDirection::kRecvOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, recvonly);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, recvonly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  auto audio_sender = callee->AddAudioTrack("track", {});
  auto video_sender = callee->AddVideoTrack("track", {});

  EXPECT_TRUE(callee->CreateAnswer());

  EXPECT_EQ(audio_sender->track()->id(), video_sender->track()->id());
  EXPECT_NE(audio_sender->id(), video_sender->id());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       AddRemoveAddTrackGeneratesNewSenderId) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto track = caller->CreateVideoTrack("video");
  auto sender1 = caller->AddTrack(track);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->pc()->RemoveTrackOrError(sender1);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto sender2 = caller->AddTrack(track);

  EXPECT_NE(sender1, sender2);
  EXPECT_NE(sender1->id(), sender2->id());
  std::string sender2_id = sender2->id();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  EXPECT_EQ(sender2_id, sender2->id());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       RenegotiationNeededAfterTransceiverSetDirection) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();

  transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       NoRenegotiationNeededAfterTransceiverSetSameDirection) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  transceiver->SetDirectionWithError(transceiver->direction());
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       NoRenegotiationNeededAfterSetDirectionOnStoppedTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->StopInternal();

  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive);
  EXPECT_FALSE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(caller->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CheckStoppedCurrentDirectionOnStoppedTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->StopInternal();

  EXPECT_TRUE(transceiver->stopping());
  EXPECT_TRUE(transceiver->stopped());
  EXPECT_EQ(RtpTransceiverDirection::kStopped,
            transceiver->current_direction());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CheckForInvalidStateOnStoppingTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->StopStandard();

  EXPECT_TRUE(transceiver->stopping());
  EXPECT_FALSE(transceiver->stopped());
  EXPECT_EQ(
      RTCErrorType::INVALID_STATE,
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive)
          .type());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CheckForInvalidStateOnStoppedTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->StopInternal();

  EXPECT_TRUE(transceiver->stopping());
  EXPECT_TRUE(transceiver->stopped());
  EXPECT_EQ(
      RTCErrorType::INVALID_STATE,
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive)
          .type());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CheckForTypeErrorForStoppedOnTransceiver) {
  auto caller = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(
      RTCErrorType::INVALID_PARAMETER,
      transceiver->SetDirectionWithError(RtpTransceiverDirection::kStopped)
          .type());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       SetLocalDescriptionWithStoppedMediaSection) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  callee->pc()->GetTransceivers()[0]->StopStandard();
  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(RtpTransceiverDirection::kStopped,
            transceiver->current_direction());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       StopAndNegotiateCausesTransceiverToDisappear) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  callee->pc()->GetTransceivers()[0]->StopStandard();
  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(RtpTransceiverDirection::kStopped,
            transceiver->current_direction());
  EXPECT_EQ(0U, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(0U, callee->pc()->GetTransceivers().size());
  EXPECT_EQ(0U, caller->pc()->GetSenders().size());
  EXPECT_EQ(0U, callee->pc()->GetSenders().size());
  EXPECT_EQ(0U, caller->pc()->GetReceivers().size());
  EXPECT_EQ(0U, callee->pc()->GetReceivers().size());
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       SetLocalDescriptionWorksAfterRepeatedAddRemove) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto video_track = caller->CreateVideoTrack("v");
  auto track = caller->CreateAudioTrack("a");
  caller->AddTransceiver(video_track);
  auto transceiver = caller->AddTransceiver(track);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->pc()->RemoveTrackOrError(transceiver->sender());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->AddTrack(track);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->pc()->RemoveTrackOrError(transceiver->sender());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       SetLocalDescriptionWorksAfterRepeatedAddRemoveWithRemoteReject) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  auto video_track = caller->CreateVideoTrack("v");
  auto track = caller->CreateAudioTrack("a");
  caller->AddTransceiver(video_track);
  auto transceiver = caller->AddTransceiver(track);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->pc()->RemoveTrackOrError(transceiver->sender());
  ExchangeOfferAnswerWhereRemoteStopsTransceiver(caller.get(), callee.get(), 1);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->AddTrack(track);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->pc()->RemoveTrackOrError(transceiver->sender());
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       CheckForUnsupportedEncodingParameters) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init;
  init.send_encodings.emplace_back();

  auto default_send_encodings = init.send_encodings;

  init.send_encodings[0].ssrc = 1;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, CheckForInvalidEncodingParameters) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init;
  init.send_encodings.emplace_back();

  auto default_send_encodings = init.send_encodings;

  init.send_encodings[0].scale_resolution_down_by = 0.5;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;

  init.send_encodings[0].bitrate_priority = 0;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;

  init.send_encodings[0].min_bitrate_bps = 200000;
  init.send_encodings[0].max_bitrate_bps = 100000;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;

  init.send_encodings[0].num_temporal_layers = 0;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;

  init.send_encodings[0].num_temporal_layers = 5;
  EXPECT_EQ(RTCErrorType::INVALID_RANGE,
            caller->pc()
                ->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init)
                .error()
                .type());
  init.send_encodings = default_send_encodings;
}

TEST_F(PeerConnectionRtpTestUnifiedPlan, SendEncodingsPassedToSender) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init;
  init.send_encodings.emplace_back();
  init.send_encodings[0].active = false;
  init.send_encodings[0].max_bitrate_bps = 180000;

  auto result = caller->pc()->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  ASSERT_TRUE(result.ok());

  auto init_send_encodings = result.value()->sender()->init_send_encodings();
  EXPECT_FALSE(init_send_encodings[0].active);
  EXPECT_EQ(init_send_encodings[0].max_bitrate_bps, 180000);

  auto parameters = result.value()->sender()->GetParameters();
  EXPECT_FALSE(parameters.encodings[0].active);
  EXPECT_EQ(parameters.encodings[0].max_bitrate_bps, 180000);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  parameters = result.value()->sender()->GetParameters();
  EXPECT_FALSE(parameters.encodings[0].active);
  EXPECT_EQ(parameters.encodings[0].max_bitrate_bps, 180000);
}

class PeerConnectionMsidSignalingTest
    : public PeerConnectionRtpTestUnifiedPlan {};

TEST_F(PeerConnectionMsidSignalingTest, UnifiedPlanTalkingToOurself) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ(
      (cricket::kMsidSignalingSemantic | cricket::kMsidSignalingMediaSection |
       cricket::kMsidSignalingSsrcAttribute),
      offer->description()->msid_signaling());

  auto* answer = caller->pc()->remote_description();
  EXPECT_EQ(
      cricket::kMsidSignalingSemantic | cricket::kMsidSignalingMediaSection,
      answer->description()->msid_signaling());
}

TEST_F(PeerConnectionMsidSignalingTest, PlanBOfferToUnifiedPlanAnswer) {
  auto caller = CreatePeerConnectionWithPlanB();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ(
      cricket::kMsidSignalingSemantic | cricket::kMsidSignalingSsrcAttribute,
      offer->description()->msid_signaling());

  auto* answer = caller->pc()->remote_description();
  EXPECT_EQ(
      cricket::kMsidSignalingSemantic | cricket::kMsidSignalingSsrcAttribute,
      answer->description()->msid_signaling());
}

TEST_F(PeerConnectionMsidSignalingTest, UnifiedPlanToPlanBAnswer) {
  const std::string kStreamId1 = "audio_stream_1";
  const std::string kStreamId2 = "audio_stream_2";

  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio", {kStreamId1, kStreamId2});
  caller->AddVideoTrack("caller_video", {});
  auto callee = CreatePeerConnectionWithPlanB();
  callee->AddAudioTrack("callee_audio");
  caller->AddVideoTrack("callee_video");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ(
      (cricket::kMsidSignalingSemantic | cricket::kMsidSignalingMediaSection |
       cricket::kMsidSignalingSsrcAttribute),
      offer->description()->msid_signaling());

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(2u, track_events.size());
  ASSERT_EQ(1u, track_events[0].streams.size());
  EXPECT_EQ(kStreamId1, track_events[0].streams[0]->id());
  ASSERT_EQ(1u, track_events[1].streams.size());

  EXPECT_FALSE(track_events[1].streams[0]->id().empty());
}

TEST_F(PeerConnectionMsidSignalingTest, PureUnifiedPlanToUs) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");

  auto offer = caller->CreateOffer();

  offer->description()->set_msid_signaling(cricket::kMsidSignalingSemantic |
                                           cricket::kMsidSignalingMediaSection);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  EXPECT_EQ(
      cricket::kMsidSignalingSemantic | cricket::kMsidSignalingMediaSection,
      answer->description()->msid_signaling());
}

TEST_P(PeerConnectionRtpTest, CreateTwoSendersWithSameTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto track = caller->CreateAudioTrack("audio_track");
  auto sender1 = caller->AddTrack(track);
  ASSERT_TRUE(sender1);

  EXPECT_TRUE(sender1->SetTrack(nullptr));
  auto sender2 = caller->AddTrack(track);
  EXPECT_TRUE(sender2);
  EXPECT_TRUE(sender1->SetTrack(track.get()));

  if (sdp_semantics_ == SdpSemantics::kPlanB_DEPRECATED) {

    EXPECT_FALSE(caller->CreateOfferAndSetAsLocal());
  } else {
    EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  }
}

TEST_F(PeerConnectionRtpTestUnifiedPlan,
       ChangeAssociatedStreamsTriggersRenegotiation) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendRecv;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->observer()->clear_legacy_renegotiation_needed();
  caller->observer()->clear_latest_negotiation_needed_event();

  transceiver->sender()->SetStreams({"stream3", "stream4", "stream5"});
  EXPECT_TRUE(caller->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(caller->observer()->has_negotiation_needed_event());

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto callee_streams = callee->pc()->GetReceivers()[0]->streams();
  ASSERT_EQ(3u, callee_streams.size());
  EXPECT_EQ("stream3", callee_streams[0]->id());
  EXPECT_EQ("stream4", callee_streams[1]->id());
  EXPECT_EQ("stream5", callee_streams[2]->id());
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionRtpTest,
                         PeerConnectionRtpTest,
                         Values(SdpSemantics::kPlanB_DEPRECATED,
                                SdpSemantics::kUnifiedPlan));

}
