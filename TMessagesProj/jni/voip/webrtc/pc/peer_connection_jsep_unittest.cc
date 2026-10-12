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

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/enable_media_with_defaults.h"
#include "api/field_trials_view.h"
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
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/transport/sctp_transport_factory_interface.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "media/engine/webrtc_media_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_info.h"
#include "pc/channel_interface.h"
#include "pc/media_session.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/sdp_utils.h"
#include "pc/session_description.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/android_test_initializer.h"
#endif
#include "pc/test/fake_audio_capture_module.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/pc/sctp/fake_sctp_transport.h"

namespace webrtc {

using cricket::MediaContentDescription;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::Values;

PeerConnectionFactoryDependencies CreatePeerConnectionFactoryDependencies() {
  PeerConnectionFactoryDependencies dependencies;
  dependencies.worker_thread = rtc::Thread::Current();
  dependencies.network_thread = rtc::Thread::Current();
  dependencies.signaling_thread = rtc::Thread::Current();
  dependencies.task_queue_factory = CreateDefaultTaskQueueFactory();
  dependencies.trials = std::make_unique<FieldTrialBasedConfig>();
  dependencies.adm = FakeAudioCaptureModule::Create();
  EnableMediaWithDefaults(dependencies);
  dependencies.sctp_factory = std::make_unique<FakeSctpTransportFactory>();
  return dependencies;
}

class PeerConnectionJsepTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  PeerConnectionJsepTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory =
        CreateModularPeerConnectionFactory(
            CreatePeerConnectionFactoryDependencies());
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    auto result = pc_factory->CreatePeerConnectionOrError(
        config, PeerConnectionDependencies(observer.get()));
    if (!result.ok()) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(result.value().get());
    return std::make_unique<PeerConnectionWrapper>(
        pc_factory, result.MoveValue(), std::move(observer));
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionJsepTest, EmptyInitialOffer) {
  auto caller = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  ASSERT_EQ(0u, offer->description()->contents().size());
}

TEST_F(PeerConnectionJsepTest, AudioOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[0].media_description()->type());
}

TEST_F(PeerConnectionJsepTest, VideoOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[0].media_description()->type());
}

TEST_F(PeerConnectionJsepTest, DataOnlyInitialOffer) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("dc");

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[0].media_description()->type());
}

TEST_F(PeerConnectionJsepTest, MultipleDataChannelsCreateOnlyOneDataSection) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("first");
  caller->CreateDataChannel("second");
  caller->CreateDataChannel("third");

  auto offer = caller->CreateOffer();
  ASSERT_EQ(1u, offer->description()->contents().size());
}

TEST_F(PeerConnectionJsepTest, MediaSectionsInInitialOfferOrderedCorrectly) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(3u, contents.size());

  const MediaContentDescription* media_description1 =
      contents[0].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description1->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description1->direction());

  const MediaContentDescription* media_description2 =
      contents[1].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, media_description2->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv,
            media_description2->direction());

  const MediaContentDescription* media_description3 =
      contents[2].media_description();
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, media_description3->type());
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            media_description3->direction());
}

TEST_F(PeerConnectionJsepTest, MediaSectionsInInitialOfferHaveDifferentMids) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_NE(contents[0].name, contents[1].name);
}

TEST_F(PeerConnectionJsepTest,
       StoppedTransceiverHasNoMediaSectionInInitialOffer) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->StopInternal();

  auto offer = caller->CreateOffer();
  EXPECT_EQ(0u, offer->description()->contents().size());
}

TEST_F(PeerConnectionJsepTest, SetLocalEmptyOfferCreatesNoTransceivers) {
  auto caller = CreatePeerConnection();

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetSenders(), ElementsAre());
  EXPECT_THAT(caller->pc()->GetReceivers(), ElementsAre());
}

TEST_F(PeerConnectionJsepTest, SetLocalOfferSetsTransceiverMid) {
  auto caller = CreatePeerConnection();
  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);

  auto offer = caller->CreateOffer();
  std::string audio_mid = offer->description()->contents()[0].name;
  std::string video_mid = offer->description()->contents()[1].name;

  ASSERT_TRUE(caller->SetLocalDescription(std::move(offer)));

  EXPECT_EQ(audio_mid, audio_transceiver->mid());
  EXPECT_EQ(video_mid, video_transceiver->mid());
}

TEST_F(PeerConnectionJsepTest, SetRemoteOfferCreatesTransceivers) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto caller_video = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());

  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceivers[0]->media_type());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[0]->direction());
  EXPECT_EQ(0u, transceivers[0]->sender()->stream_ids().size());

  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceivers[1]->media_type());
  EXPECT_EQ(caller_video->mid(), transceivers[1]->mid());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceivers[1]->direction());
  EXPECT_EQ(0u, transceivers[1]->sender()->stream_ids().size());
}

TEST_F(PeerConnectionJsepTest, SetRemoteOfferReusesTransceiverFromAddTrack) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[0]->receiver()->track()->kind());
  EXPECT_EQ(caller_audio->mid(), transceivers[0]->mid());
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverIfDirectionSendOnly) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto caller_audio = caller->pc()->GetTransceivers()[0];
  caller_audio->SetDirectionWithError(RtpTransceiverDirection::kSendOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller_audio->mid(), transceivers[1]->mid());
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverFromAddTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto transceiver = callee->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  auto video_sender = callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(absl::nullopt, transceivers[0]->mid());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[1]->mid());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind,
            transceivers[1]->receiver()->track()->kind());
}

TEST_F(PeerConnectionJsepTest, SetRemoteOfferDoesNotReuseStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->StopInternal();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());

  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
  transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->mid(), transceivers[0]->mid());
  EXPECT_FALSE(transceivers[0]->stopped());
}

TEST_F(PeerConnectionJsepTest, SetRemoteOfferReusesTransceiversOfBothTypes) {
  auto caller = CreatePeerConnection();
  caller->AddVideoTrack("v");
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto caller_transceivers = caller->pc()->GetTransceivers();
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(2u, callee_transceivers.size());
  EXPECT_EQ(caller_transceivers[0]->mid(), callee_transceivers[1]->mid());
  EXPECT_EQ(caller_transceivers[1]->mid(), callee_transceivers[0]->mid());
}

TEST_F(PeerConnectionJsepTest, CreateAnswerHasSameMidsAsOffer) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto third_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  caller->CreateDataChannel("dc");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  const auto* offer_data = cricket::GetFirstDataContent(offer->description());
  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(4u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[0].media_description()->type());
  EXPECT_EQ(first_transceiver->mid(), contents[0].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[1].media_description()->type());
  EXPECT_EQ(second_transceiver->mid(), contents[1].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, contents[2].media_description()->type());
  EXPECT_EQ(third_transceiver->mid(), contents[2].name);
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[3].media_description()->type());
  EXPECT_EQ(offer_data->name, contents[3].name);
}

TEST_F(PeerConnectionJsepTest, CreateAnswerRejectsStoppedTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  callee->pc()->GetTransceivers()[0]->StopInternal();

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

TEST_F(PeerConnectionJsepTest, CreateAnswerNegotiatesDirection) {
  auto caller = CreatePeerConnection();
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            contents[0].media_description()->direction());
}

TEST_F(PeerConnectionJsepTest, SetLocalAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  auto caller_audio = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller_audio->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));

  auto transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            transceivers[0]->current_direction());
}

TEST_F(PeerConnectionJsepTest, SetRemoteAnswerUpdatesCurrentDirection) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  auto callee_audio = callee->pc()->GetTransceivers()[0];
  callee_audio->SetDirectionWithError(RtpTransceiverDirection::kSendOnly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly,
            transceivers[0]->current_direction());
}

TEST_F(PeerConnectionJsepTest,
       ChangeDirectionFromRecvOnlyToSendRecvDoesNotBreakVideoNegotiation) {
  auto caller = CreatePeerConnection();
  auto caller_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection();
  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
}

TEST_F(PeerConnectionJsepTest,
       ChangeDirectionFromRecvOnlyToSendRecvDoesNotBreakAudioNegotiation) {
  auto caller = CreatePeerConnection();
  auto caller_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
}

TEST_F(PeerConnectionJsepTest, SettingTransceiverInactiveDoesNotStopIt) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->SetDirectionWithError(
      RtpTransceiverDirection::kInactive);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  EXPECT_FALSE(caller->pc()->GetTransceivers()[0]->stopped());
  EXPECT_FALSE(callee->pc()->GetTransceivers()[0]->stopped());
}

TEST_F(PeerConnectionJsepTest,
       ReOfferMediaSectionForAssociatedStoppedTransceiverIsRejected) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_TRUE(transceiver->mid());
  transceiver->StopInternal();

  auto reoffer = caller->CreateOffer();
  auto contents = reoffer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
}

TEST_F(PeerConnectionJsepTest,
       StoppingTransceiverInOfferStopsTransceiverOnRemoteSide) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  transceiver->StopInternal();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto transceivers = callee->pc()->GetTransceivers();
  EXPECT_EQ(1u, transceivers.size());
  ASSERT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
  transceivers = callee->pc()->GetTransceivers();
  EXPECT_EQ(0u, transceivers.size());
}

TEST_F(PeerConnectionJsepTest,
       CreateOfferDoesNotRecycleMediaSectionIfFirstStopped) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto second_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  first_transceiver->StopInternal();

  auto reoffer = caller->CreateOffer();
  auto contents = reoffer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(contents[0].rejected);
  EXPECT_FALSE(contents[1].rejected);
}

TEST_F(PeerConnectionJsepTest,
       RecycleMediaSectionWhenStoppingTransceiverOnAnswerer) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  std::string first_mid = *first_transceiver->mid();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  callee->pc()->GetTransceivers()[0]->StopInternal();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
  EXPECT_TRUE(first_transceiver->stopped());

  ASSERT_EQ(absl::nullopt, first_transceiver->mid());

  ASSERT_EQ(0u, callee->pc()->GetTransceivers().size());

  caller->AddAudioTrack("audio2");
  callee->AddAudioTrack("audio2");
  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  std::string second_mid = offer_contents[0].name;
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, first_transceiver->mid());
  ASSERT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(second_mid, caller->pc()->GetTransceivers()[0]->mid());
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  EXPECT_EQ(second_mid, callee->pc()->GetTransceivers()[0]->mid());

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, caller_transceivers.size());
  EXPECT_EQ(second_mid, caller_transceivers[0]->mid());
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, callee_transceivers.size());
  EXPECT_EQ(second_mid, callee_transceivers[0]->mid());
}

TEST_F(PeerConnectionJsepTest, CreateOfferRecyclesWhenOfferingTwice) {

  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  first_transceiver->StopInternal();
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  caller->AddAudioTrack("audio2");

  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  ASSERT_TRUE(caller->SetLocalDescription(std::move(offer)));
  ASSERT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_FALSE(caller->pc()->GetTransceivers()[0]->stopped());
  std::string second_mid = offer_contents[0].name;

  auto second_offer = caller->CreateOffer();
  auto second_offer_contents = second_offer->description()->contents();
  ASSERT_EQ(1u, second_offer_contents.size());
  EXPECT_FALSE(second_offer_contents[0].rejected);

  EXPECT_EQ(second_mid, second_offer_contents[0].name);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(second_offer)));

  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, caller_transceivers.size());
  EXPECT_EQ(second_mid, caller_transceivers[0]->mid());
  EXPECT_FALSE(caller_transceivers[0]->stopped());
}

class RecycleMediaSectionTest
    : public PeerConnectionJsepTest,
      public ::testing::WithParamInterface<
          std::tuple<cricket::MediaType, cricket::MediaType>> {
 protected:
  RecycleMediaSectionTest() {
    first_type_ = std::get<0>(GetParam());
    second_type_ = std::get<1>(GetParam());
  }

  cricket::MediaType first_type_;
  cricket::MediaType second_type_;
};

TEST_P(RecycleMediaSectionTest, CurrentLocalAndCurrentRemoteRejected) {
  auto caller = CreatePeerConnection();
  auto first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  std::string first_mid = *first_transceiver->mid();
  first_transceiver->StopInternal();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto second_transceiver = caller->AddTransceiver(second_type_);

  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, first_transceiver->mid());
  EXPECT_EQ(second_mid, second_transceiver->mid());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, callee_transceivers.size());
  EXPECT_EQ(second_mid, callee_transceivers[0]->mid());
  EXPECT_EQ(second_type_, callee_transceivers[0]->media_type());

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(1u, caller->pc()->GetTransceivers().size());
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
}

TEST_P(RecycleMediaSectionTest, CurrentRemoteOnlyRejected) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  std::string first_mid = *caller_first_transceiver->mid();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  callee_first_transceiver->StopInternal();

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto caller_second_transceiver = caller->AddTransceiver(second_type_);
  auto offer = caller->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  ASSERT_EQ(1u, callee_transceivers.size());
  EXPECT_EQ(second_mid, callee_transceivers[0]->mid());
  EXPECT_EQ(second_type_, callee_transceivers[0]->media_type());

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(1u, caller->pc()->GetTransceivers().size());
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
}

TEST_P(RecycleMediaSectionTest, CurrentLocalOnlyRejected) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  std::string first_mid = *caller_first_transceiver->mid();
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  callee_first_transceiver->StopInternal();

  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  auto callee_second_transceiver = callee->AddTransceiver(second_type_);
  auto offer = callee->CreateOffer();
  const auto& offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());
  EXPECT_FALSE(offer_contents[0].rejected);
  EXPECT_EQ(second_type_, offer_contents[0].media_description()->type());
  std::string second_mid = offer_contents[0].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(offer.get())));
  EXPECT_EQ(absl::nullopt, callee_first_transceiver->mid());
  EXPECT_EQ(second_mid, callee_second_transceiver->mid());

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(offer)));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, caller_transceivers.size());
  EXPECT_EQ(second_mid, caller_transceivers[0]->mid());
  EXPECT_EQ(second_type_, caller_transceivers[0]->media_type());

  auto answer = caller->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_FALSE(answer_contents[0].rejected);
  EXPECT_EQ(second_mid, answer_contents[0].name);
  EXPECT_EQ(second_type_, answer_contents[0].media_description()->type());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(answer.get())));

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(answer)));
  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  ASSERT_EQ(1u, caller->pc()->GetTransceivers().size());
}

TEST_P(RecycleMediaSectionTest, PendingLocalRejectedAndNoRemote) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  std::string first_mid = *caller_first_transceiver->mid();
  caller_first_transceiver->StopInternal();

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto caller_second_transceiver = caller->AddTransceiver(second_type_);

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(reoffer)));

  EXPECT_EQ(first_mid, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());
}

TEST_P(RecycleMediaSectionTest, PendingLocalRejectedAndNotRejectedRemote) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  std::string first_mid = *caller_first_transceiver->mid();
  caller_first_transceiver->StopInternal();

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  auto caller_second_transceiver = caller->AddTransceiver(second_type_);

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

  ASSERT_TRUE(caller->SetLocalDescription(std::move(reoffer)));

  EXPECT_EQ(first_mid, caller_first_transceiver->mid());
  EXPECT_EQ(second_mid, caller_second_transceiver->mid());
}

TEST_P(RecycleMediaSectionTest, PendingRemoteRejectedAndNoLocal) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  std::string first_mid = *callee_first_transceiver->mid();
  caller_first_transceiver->StopInternal();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto callee_second_transceiver = callee->AddTransceiver(second_type_);

  auto reoffer = callee->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

}

TEST_P(RecycleMediaSectionTest, PendingRemoteRejectedAndNotRejectedLocal) {
  auto caller = CreatePeerConnection();
  auto caller_first_transceiver = caller->AddTransceiver(first_type_);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(1u, callee->pc()->GetTransceivers().size());
  auto callee_first_transceiver = callee->pc()->GetTransceivers()[0];
  std::string first_mid = *callee_first_transceiver->mid();
  caller_first_transceiver->StopInternal();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto callee_second_transceiver = callee->AddTransceiver(second_type_);

  auto reoffer = callee->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  ASSERT_EQ(2u, reoffer_contents.size());
  EXPECT_TRUE(reoffer_contents[0].rejected);
  EXPECT_EQ(first_type_, reoffer_contents[0].media_description()->type());
  EXPECT_EQ(first_mid, reoffer_contents[0].name);
  EXPECT_FALSE(reoffer_contents[1].rejected);
  EXPECT_EQ(second_type_, reoffer_contents[1].media_description()->type());
  std::string second_mid = reoffer_contents[1].name;
  EXPECT_NE(first_mid, second_mid);

}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionJsepTest,
    RecycleMediaSectionTest,
    Combine(Values(cricket::MEDIA_TYPE_AUDIO, cricket::MEDIA_TYPE_VIDEO),
            Values(cricket::MEDIA_TYPE_AUDIO, cricket::MEDIA_TYPE_VIDEO)));

TEST_F(PeerConnectionJsepTest, DataChannelDoesNotRecycleMediaSection) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  transceiver->StopInternal();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->CreateDataChannel("dc");

  auto offer = caller->CreateOffer();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(2u, offer_contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO,
            offer_contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA,
            offer_contents[1].media_description()->type());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(2u, answer_contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO,
            answer_contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA,
            answer_contents[1].media_description()->type());
}

TEST_F(PeerConnectionJsepTest, AudioTrackAddedAfterDataSectionInReoffer) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("dc");
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->AddAudioTrack("a");

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());
  EXPECT_EQ(cricket::MEDIA_TYPE_DATA, contents[0].media_description()->type());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, contents[1].media_description()->type());
}

static void RenameSection(size_t mline_index,
                          const std::string& new_mid,
                          SessionDescriptionInterface* sdesc) {
  cricket::SessionDescription* desc = sdesc->description();
  std::string old_mid = desc->contents()[mline_index].name;
  desc->contents()[mline_index].name = new_mid;
  desc->transport_infos()[mline_index].content_name = new_mid;
  const cricket::ContentGroup* bundle =
      desc->GetGroupByName(cricket::GROUP_TYPE_BUNDLE);
  if (bundle) {
    cricket::ContentGroup new_bundle = *bundle;
    if (new_bundle.RemoveContentName(old_mid)) {
      new_bundle.AddContentName(new_mid);
    }
    desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
    desc->AddGroup(new_bundle);
  }
}

TEST_F(PeerConnectionJsepTest, OfferAnswerWithChangedMids) {
  constexpr char kFirstMid[] = "nondefaultmid";
  constexpr char kSecondMid[] = "randommid";

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  caller->AddAudioTrack("b");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, kFirstMid, offer.get());
  RenameSection(1, kSecondMid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  auto caller_transceivers = caller->pc()->GetTransceivers();
  EXPECT_EQ(kFirstMid, caller_transceivers[0]->mid());
  EXPECT_EQ(kSecondMid, caller_transceivers[1]->mid());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  auto callee_transceivers = callee->pc()->GetTransceivers();
  EXPECT_EQ(kFirstMid, callee_transceivers[0]->mid());
  EXPECT_EQ(kSecondMid, callee_transceivers[1]->mid());

  auto answer = callee->CreateAnswer();
  auto answer_contents = answer->description()->contents();
  EXPECT_EQ(kFirstMid, answer_contents[0].name);
  EXPECT_EQ(kSecondMid, answer_contents[1].name);

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

TEST_F(PeerConnectionJsepTest, CreateOfferGeneratesUniqueMidIfAlreadyTaken) {

  auto pc = CreatePeerConnection();
  pc->AddAudioTrack("a");
  pc->AddAudioTrack("b");
  auto default_offer = pc->CreateOffer();
  std::string default_second_mid =
      default_offer->description()->contents()[1].name;

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, default_second_mid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  caller->AddAudioTrack("b");

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  EXPECT_EQ(default_second_mid, reoffer_contents[0].name);
  EXPECT_NE(reoffer_contents[0].name, reoffer_contents[1].name);
}

TEST_F(PeerConnectionJsepTest,
       CreateOfferGeneratesUniqueMidForDataSectionIfAlreadyTaken) {

  auto pc = CreatePeerConnection();
  pc->CreateDataChannel("dc");
  auto default_offer = pc->CreateOffer();
  std::string default_data_mid =
      default_offer->description()->contents()[0].name;

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();

  auto offer = caller->CreateOffer();
  RenameSection(0, default_data_mid, offer.get());

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  caller->CreateDataChannel("dc");

  auto reoffer = caller->CreateOffer();
  auto reoffer_contents = reoffer->description()->contents();
  EXPECT_EQ(default_data_mid, reoffer_contents[0].name);
  EXPECT_NE(reoffer_contents[0].name, reoffer_contents[1].name);
}

TEST_F(PeerConnectionJsepTest, CalleeDoesReoffer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  callee->AddVideoTrack("v");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  EXPECT_EQ(1u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(2u, callee->pc()->GetTransceivers().size());

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));

  EXPECT_EQ(2u, caller->pc()->GetTransceivers().size());
  EXPECT_EQ(2u, callee->pc()->GetTransceivers().size());
}

TEST_F(PeerConnectionJsepTest, AddingTrackWithAddTrackSpecifiesTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto streams = contents[0].media_description()->streams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_EQ(kTrackId, streams[0].id);
}

TEST_F(PeerConnectionJsepTest,
       AddingTrackWithAddTransceiverSpecifiesRandomTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->sender()->SetTrack(caller->CreateAudioTrack(kTrackId).get());

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto streams = contents[0].media_description()->streams();
  ASSERT_EQ(1u, streams.size());
  EXPECT_NE(kTrackId, streams[0].id);
}

TEST_F(PeerConnectionJsepTest, NoMsidInOfferIfTransceiverDirectionHasNoSend) {
  auto caller = CreatePeerConnection();

  RtpTransceiverInit init_recvonly;
  init_recvonly.direction = RtpTransceiverDirection::kRecvOnly;
  ASSERT_TRUE(caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init_recvonly));

  RtpTransceiverInit init_inactive;
  init_inactive.direction = RtpTransceiverDirection::kInactive;
  ASSERT_TRUE(caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init_inactive));

  auto offer = caller->CreateOffer();
  auto contents = offer->description()->contents();
  ASSERT_EQ(2u, contents.size());

  EXPECT_EQ(0u, contents[0].media_description()->streams().size());
  EXPECT_EQ(0u, contents[1].media_description()->streams().size());
}

TEST_F(PeerConnectionJsepTest, NoMsidInAnswerIfNoRespondingTracks) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  RtpTransceiverInit init_recvonly;
  init_recvonly.direction = RtpTransceiverDirection::kRecvOnly;
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init_recvonly);

  RtpTransceiverInit init_sendrecv;
  init_sendrecv.direction = RtpTransceiverDirection::kSendRecv;
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init_sendrecv);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  auto contents = answer->description()->contents();
  ASSERT_EQ(2u, contents.size());

  EXPECT_EQ(0u, contents[0].media_description()->streams().size());
  EXPECT_EQ(0u, contents[1].media_description()->streams().size());
}

TEST_F(PeerConnectionJsepTest, IncludeMsidEvenIfDirectionHasChanged) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  caller->pc()->GetTransceivers()[0]->SetDirectionWithError(
      RtpTransceiverDirection::kInactive);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());

  EXPECT_EQ(1u, offer_contents[0].media_description()->streams().size());

  auto* answer = caller->pc()->remote_description();
  auto answer_contents = answer->description()->contents();
  ASSERT_EQ(1u, answer_contents.size());
  EXPECT_EQ(1u, answer_contents[0].media_description()->streams().size());
}

TEST_F(PeerConnectionJsepTest, RemoveMsidIfTransceiverStopped) {
  auto caller = CreatePeerConnection();
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  transceiver->StopInternal();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  auto* offer = callee->pc()->remote_description();
  auto offer_contents = offer->description()->contents();
  ASSERT_EQ(1u, offer_contents.size());

  EXPECT_EQ(0u, offer_contents[0].media_description()->streams().size());
}

TEST_F(PeerConnectionJsepTest,
       RtpReceiverCreatedBySetRemoteDescriptionHasSignaledTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
  auto receiver = callee->pc()->GetReceivers()[0];
  EXPECT_EQ(kTrackId, receiver->id());
}

TEST_F(PeerConnectionJsepTest,
       RtpReceiverCreatedBeforeSetRemoteDescriptionKeepsId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack(kTrackId);
  callee->AddAudioTrack("dummy_track");

  ASSERT_EQ(1u, callee->pc()->GetReceivers().size());
  auto receiver = callee->pc()->GetReceivers()[0];
  std::string receiver_id = receiver->id();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(receiver_id, receiver->id());
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackNoStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  EXPECT_EQ(kTrackLabel, event.receiver->track()->id());
  EXPECT_EQ(0u, event.streams.size());
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackOneStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";
  const std::string kStreamId = "audio_stream";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel, {kStreamId}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  ASSERT_EQ(1u, event.streams.size());
  auto stream = event.streams[0];
  EXPECT_EQ(kStreamId, stream->id());
  EXPECT_THAT(track_events[0].snapshotted_stream_tracks.at(stream),
              ElementsAre(event.receiver->track()));
  EXPECT_EQ(event.receiver->streams(), track_events[0].streams);
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithTwoTracksSameStreamFiresOnAddTrack) {
  const std::string kTrack1Label = "audio_track1";
  const std::string kTrack2Label = "audio_track2";
  const std::string kSharedStreamId = "stream";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrack1Label, {kSharedStreamId}));
  ASSERT_TRUE(caller->AddAudioTrack(kTrack2Label, {kSharedStreamId}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(2u, track_events.size());
  const auto& event1 = track_events[0];
  const auto& event2 = track_events[1];
  ASSERT_EQ(1u, event1.streams.size());
  auto stream = event1.streams[0];
  ASSERT_THAT(event2.streams, ElementsAre(stream));
  auto track1 = event1.receiver->track();
  auto track2 = event2.receiver->track();
  EXPECT_THAT(event1.snapshotted_stream_tracks.at(stream),
              UnorderedElementsAre(track1, track2));
  EXPECT_THAT(event2.snapshotted_stream_tracks.at(stream),
              UnorderedElementsAre(track1, track2));
}

TEST_F(PeerConnectionJsepTest,
       SetRemoteOfferWithOneTrackTwoStreamFiresOnAddTrack) {
  const std::string kTrackLabel = "audio_track";
  const std::string kStreamId1 = "audio_stream1";
  const std::string kStreamId2 = "audio_stream2";

  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  ASSERT_TRUE(caller->AddAudioTrack(kTrackLabel, {kStreamId1, kStreamId2}));

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(1u, track_events.size());
  const auto& event = track_events[0];
  ASSERT_EQ(2u, event.streams.size());
  EXPECT_EQ(kStreamId1, event.streams[0]->id());
  EXPECT_EQ(kStreamId2, event.streams[1]->id());
}

TEST_F(PeerConnectionJsepTest, CurrentDirectionResetWhenRtpTransceiverStopped) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_TRUE(transceiver->current_direction());
  transceiver->StopInternal();
  EXPECT_EQ(transceiver->current_direction(),
            RtpTransceiverDirection::kStopped);
}

TEST_F(PeerConnectionJsepTest, AnswerBeforeOfferFails) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  RTCError error;
  ASSERT_FALSE(caller->SetRemoteDescription(callee->CreateAnswer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_STATE, error.type());
}

TEST_F(PeerConnectionJsepTest, TwoVideoPlanBToUnifiedPlanFails) {
  RTCConfiguration config_planb;
  config_planb.sdp_semantics = SdpSemantics::kPlanB_DEPRECATED;
  auto caller = CreatePeerConnection(config_planb);
  auto callee = CreatePeerConnection();
  caller->AddVideoTrack("video1");
  caller->AddVideoTrack("video2");

  RTCError error;
  ASSERT_FALSE(callee->SetRemoteDescription(caller->CreateOffer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.type());
}

TEST_F(PeerConnectionJsepTest, OneVideoUnifiedPlanToTwoVideoPlanBFails) {
  auto caller = CreatePeerConnection();
  RTCConfiguration config_planb;
  config_planb.sdp_semantics = SdpSemantics::kPlanB_DEPRECATED;
  auto callee = CreatePeerConnection(config_planb);
  caller->AddVideoTrack("video");
  callee->AddVideoTrack("video1");
  callee->AddVideoTrack("video2");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  RTCError error;
  ASSERT_FALSE(caller->SetRemoteDescription(caller->CreateAnswer(), &error));
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.type());
}

static void RemoveRtpHeaderExtensionByUri(
    MediaContentDescription* media_description,
    absl::string_view uri) {
  std::vector<RtpExtension> header_extensions =
      media_description->rtp_header_extensions();
  header_extensions.erase(std::remove_if(
      header_extensions.begin(), header_extensions.end(),
      [uri](const RtpExtension& extension) { return extension.uri == uri; }));
  media_description->set_rtp_header_extensions(header_extensions);
}

static void ClearMids(SessionDescriptionInterface* sdesc) {
  cricket::SessionDescription* desc = sdesc->description();
  desc->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  cricket::ContentInfo* audio_content = cricket::GetFirstAudioContent(desc);
  if (audio_content) {
    desc->GetTransportInfoByName(audio_content->name)->content_name = "";
    audio_content->name = "";
    RemoveRtpHeaderExtensionByUri(audio_content->media_description(),
                                  RtpExtension::kMidUri);
  }
  cricket::ContentInfo* video_content = cricket::GetFirstVideoContent(desc);
  if (video_content) {
    desc->GetTransportInfoByName(video_content->name)->content_name = "";
    video_content->name = "";
    RemoveRtpHeaderExtensionByUri(video_content->media_description(),
                                  RtpExtension::kMidUri);
  }
}

TEST_F(PeerConnectionJsepTest, LegacyNoMidAudioOnlyOffer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");

  auto offer = caller->CreateOffer();
  ClearMids(offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  EXPECT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
}
TEST_F(PeerConnectionJsepTest, LegacyNoMidAudioVideoOffer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");
  callee->AddVideoTrack("video");

  auto offer = caller->CreateOffer();
  ClearMids(offer.get());

  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  EXPECT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
}
TEST_F(PeerConnectionJsepTest, LegacyNoMidAudioOnlyAnswer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  ClearMids(answer.get());

  EXPECT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_F(PeerConnectionJsepTest, LegacyNoMidAudioVideoAnswer) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");
  callee->AddVideoTrack("video");

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  ClearMids(answer.get());

  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

TEST_F(PeerConnectionJsepTest, LegacyNoMidTwoRemoteOffers) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("audio");

  auto offer = caller->CreateOffer();
  ClearMids(offer.get());

  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));
  EXPECT_TRUE(callee->SetLocalDescription(callee->CreateAnswer()));
}

TEST_F(PeerConnectionJsepTest, SetLocalDescriptionFailsMissingMid) {
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("audio");

  auto offer = caller->CreateOffer();
  ClearMids(offer.get());

  std::string error;
  ASSERT_FALSE(caller->SetLocalDescription(std::move(offer), &error));
  EXPECT_EQ(
      "Failed to set local offer sdp: A media section is missing a MID "
      "attribute.",
      error);
}

TEST_F(PeerConnectionJsepTest, RollbackSupportedInUnifiedPlan) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->SetRemoteDescription(callee->CreateOffer()));
}

TEST_F(PeerConnectionJsepTest, RollbackNotSupportedInPlanB) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kPlanB_DEPRECATED;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_FALSE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_FALSE(caller->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_FALSE(caller->SetRemoteDescription(callee->CreateOffer()));
}

TEST_F(PeerConnectionJsepTest, RollbackFailsInStableState) {
  auto caller = CreatePeerConnection();
  EXPECT_FALSE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_FALSE(caller->SetRemoteDescription(caller->CreateRollback()));
}

TEST_F(PeerConnectionJsepTest, RollbackToStableStateAndClearLocalOffer) {
  auto caller = CreatePeerConnection();
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_EQ(caller->signaling_state(), PeerConnectionInterface::kStable);
  EXPECT_EQ(caller->pc()->pending_local_description(), nullptr);

  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(caller->signaling_state(), PeerConnectionInterface::kStable);
  EXPECT_EQ(caller->pc()->pending_local_description(), nullptr);
}

TEST_F(PeerConnectionJsepTest, RollbackToStableStateAndClearRemoteOffer) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->signaling_state(), PeerConnectionInterface::kStable);
  EXPECT_EQ(callee->pc()->pending_remote_description(), nullptr);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetLocalDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->signaling_state(), PeerConnectionInterface::kStable);
  EXPECT_EQ(callee->pc()->pending_remote_description(), nullptr);
}

TEST_F(PeerConnectionJsepTest, RollbackImplicitly) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->signaling_state(),
            PeerConnectionInterface::kHaveRemoteOffer);
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());
  EXPECT_FALSE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(callee->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionJsepTest, RollbackImplicitlyNegotatiationNotNeeded) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  caller->AddAudioTrack("a");
  callee->AddAudioTrack("b");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  callee->observer()->clear_legacy_renegotiation_needed();
  callee->observer()->clear_latest_negotiation_needed_event();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->signaling_state(),
            PeerConnectionInterface::kHaveRemoteOffer);
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());

  EXPECT_FALSE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(callee->observer()->has_negotiation_needed_event());
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 0u);
}

TEST_F(PeerConnectionJsepTest, RollbackImplicitlyAndNegotiationNeeded) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  callee->AddAudioTrack("a");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  callee->observer()->clear_legacy_renegotiation_needed();
  callee->observer()->clear_latest_negotiation_needed_event();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->signaling_state(),
            PeerConnectionInterface::kHaveRemoteOffer);
  EXPECT_FALSE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(callee->observer()->has_negotiation_needed_event());
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());
  EXPECT_TRUE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(callee->observer()->has_negotiation_needed_event());
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 0u);
}

TEST_F(PeerConnectionJsepTest, AttemptToRollbackImplicitly) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_FALSE(callee->SetRemoteDescription(
      CreateSessionDescription(SdpType::kOffer, "invalid sdp")));
  EXPECT_EQ(callee->signaling_state(),
            PeerConnectionInterface::kHaveLocalOffer);
}

TEST_F(PeerConnectionJsepTest, RollbackRemovesTransceiver) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  ASSERT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  auto transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 0u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);

  EXPECT_TRUE(transceiver->stopping());
  EXPECT_TRUE(transceiver->stopping());
  EXPECT_EQ(transceiver->receiver()->track()->state(),
            MediaStreamTrackInterface::kEnded);
}

TEST_F(PeerConnectionJsepTest, RollbackKeepsTransceiverAndClearsMid) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  callee->AddAudioTrack("a");
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));

  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);

  auto transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_FALSE(transceiver->stopping());
  EXPECT_FALSE(transceiver->stopping());
  EXPECT_EQ(transceiver->receiver()->track()->state(),
            MediaStreamTrackInterface::kLive);
}

TEST_F(PeerConnectionJsepTest,
       RollbackKeepsTransceiverAfterAddTrackEvenWhenTrackIsNulled) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->sender()->SetTrack(nullptr);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->sender()->track(), nullptr);
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));

  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);
}

TEST_F(PeerConnectionJsepTest, RollbackRestoresMid) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  auto offer = callee->CreateOffer();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_NE(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);
  EXPECT_TRUE(callee->SetLocalDescription(std::move(offer)));
}

TEST_F(PeerConnectionJsepTest, RollbackRestoresInitSendEncodings) {
  auto caller = CreatePeerConnection();
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendRecv;
  RtpEncodingParameters encoding;
  encoding.rid = "hi";
  init.send_encodings.push_back(encoding);
  encoding.rid = "mid";
  init.send_encodings.push_back(encoding);
  encoding.rid = "lo";
  init.send_encodings.push_back(encoding);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto encodings =
      caller->pc()->GetTransceivers()[0]->sender()->init_send_encodings();
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_NE(caller->pc()->GetTransceivers()[0]->sender()->init_send_encodings(),
            encodings);
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_EQ(caller->pc()->GetTransceivers()[0]->sender()->init_send_encodings(),
            encodings);
}

TEST_F(PeerConnectionJsepTest, RollbackDoesNotAffectSendEncodings) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  RtpEncodingParameters encoding;
  encoding.rid = "hi";
  init.send_encodings.push_back(encoding);
  encoding.rid = "mid";
  init.send_encodings.push_back(encoding);
  encoding.rid = "lo";
  init.send_encodings.push_back(encoding);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal());
  auto params = caller->pc()->GetTransceivers()[0]->sender()->GetParameters();
  EXPECT_TRUE(params.encodings[0].active);
  params.encodings[0].active = false;
  caller->pc()->GetTransceivers()[0]->sender()->SetParameters(params);
  auto offer = caller->CreateOffer();
  std::string offer_string;
  EXPECT_TRUE(offer.get()->ToString(&offer_string));
  std::string simulcast_line =
      offer_string.substr(offer_string.find("a=simulcast"));
  EXPECT_FALSE(simulcast_line.empty());
  EXPECT_TRUE(caller->SetLocalDescription(std::move(offer)));
  EXPECT_TRUE(caller->SetLocalDescription(caller->CreateRollback()));
  EXPECT_FALSE(caller->pc()
                   ->GetTransceivers()[0]
                   ->sender()
                   ->GetParameters()
                   .encodings[0]
                   .active);
  offer = caller->CreateOffer();
  EXPECT_TRUE(offer.get()->ToString(&offer_string));
  EXPECT_EQ(offer_string.substr(offer_string.find("a=simulcast")),
            simulcast_line);
}

TEST_F(PeerConnectionJsepTest, RollbackRestoresMidAndRemovesTransceiver) {
  auto callee = CreatePeerConnection();
  callee->AddVideoTrack("a");
  auto offer = callee->CreateOffer();
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("b");
  caller->AddVideoTrack("c");
  auto mid = callee->pc()->GetTransceivers()[0]->mid();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 2u);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), mid);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->media_type(),
            cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->SetLocalDescription(std::move(offer)));
  EXPECT_EQ(callee->observer()->remove_track_events_.size(),
            callee->observer()->add_track_events_.size());
}

TEST_F(PeerConnectionJsepTest, RollbackHasNoEffectOnStableTransceivers) {
  auto callee = CreatePeerConnection();
  callee->AddVideoTrack("a");
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("b");
  caller->AddVideoTrack("c");
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  callee->observer()->clear_legacy_renegotiation_needed();
  callee->observer()->clear_latest_negotiation_needed_event();
  size_t transceiver_count = callee->pc()->GetTransceivers().size();
  auto mid_0 = callee->pc()->GetTransceivers()[0]->mid();
  auto mid_1 = callee->pc()->GetTransceivers()[1]->mid();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), transceiver_count);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), mid_0);
  EXPECT_EQ(callee->pc()->GetTransceivers()[1]->mid(), mid_1);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 0u);
  EXPECT_FALSE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_FALSE(callee->observer()->has_negotiation_needed_event());
}

TEST_F(PeerConnectionJsepTest, ImplicitlyRollbackTransceiversWithSameMids) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection(config);
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  auto initial_mid = callee->pc()->GetTransceivers()[0]->mid();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 2u);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);
  EXPECT_EQ(callee->pc()->GetTransceivers()[1]->mid(),
            caller->pc()->GetTransceivers()[0]->mid());
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_NE(callee->pc()->GetTransceivers()[0]->mid(), initial_mid);
}

TEST_F(PeerConnectionJsepTest, RollbackToNegotiatedStableState) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  auto caller = CreatePeerConnection(config);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection(config);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());
  caller->AddVideoTrack("a");
  callee->AddVideoTrack("b");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 2u);
  auto audio_transport =
      callee->pc()->GetTransceivers()[0]->sender()->dtls_transport();
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            callee->pc()->GetTransceivers()[1]->sender()->dtls_transport());
  EXPECT_NE(callee->pc()->GetTransceivers()[1]->sender()->dtls_transport(),
            nullptr);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            audio_transport);
  EXPECT_EQ(callee->pc()->GetTransceivers()[1]->sender()->dtls_transport(),
            nullptr);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            audio_transport);
}

TEST_F(PeerConnectionJsepTest, RollbackHasToDestroyTransport) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  auto pc = CreatePeerConnection(config);
  pc->AddAudioTrack("a");
  pc->AddVideoTrack("b");
  EXPECT_TRUE(pc->CreateOfferAndSetAsLocal());
  auto offer = pc->CreateOffer();
  EXPECT_EQ(pc->pc()->GetTransceivers().size(), 2u);
  auto audio_transport =
      pc->pc()->GetTransceivers()[0]->sender()->dtls_transport();
  EXPECT_EQ(pc->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            pc->pc()->GetTransceivers()[1]->sender()->dtls_transport());
  EXPECT_NE(pc->pc()->GetTransceivers()[1]->sender()->dtls_transport(),
            nullptr);
  EXPECT_TRUE(pc->SetRemoteDescription(pc->CreateRollback()));
  EXPECT_TRUE(pc->SetLocalDescription(std::move(offer)));
  EXPECT_NE(pc->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            nullptr);
  EXPECT_NE(pc->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            audio_transport);
}

TEST_F(PeerConnectionJsepTest, RollbackLocalDirectionChange) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
  callee->AddAudioTrack("a");
  callee->pc()->GetTransceivers()[0]->SetDirectionWithError(
      RtpTransceiverDirection::kSendOnly);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  auto audio_transport =
      callee->pc()->GetTransceivers()[0]->receiver()->dtls_transport();
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->direction(),
            RtpTransceiverDirection::kSendOnly);

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->receiver()->dtls_transport(),
            audio_transport);
}

TEST_F(PeerConnectionJsepTest, RollbackRemoteDirectionChange) {
  auto caller = CreatePeerConnection();
  auto caller_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->direction(),
            RtpTransceiverDirection::kSendRecv);
  auto audio_transport =
      callee->pc()->GetTransceivers()[0]->sender()->dtls_transport();
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->direction(),
            RtpTransceiverDirection::kSendRecv);

  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->sender()->dtls_transport(),
            audio_transport);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);
}

TEST_F(PeerConnectionJsepTest,
       RollbackRestoresFiredDirectionAndOnTrackCanFireAgain) {
  auto caller = CreatePeerConnection();
  auto caller_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  callee->AddAudioTrack("a");
  ASSERT_EQ(callee->pc()->GetTransceivers().size(), 1u);
  auto callee_transceiver = callee->pc()->GetTransceivers()[0];
  EXPECT_FALSE(callee_transceiver->fired_direction().has_value());
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(callee_transceiver->fired_direction().has_value());
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);

  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_FALSE(callee_transceiver->fired_direction().has_value());
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(callee_transceiver->fired_direction().has_value());
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 1u);
}

TEST_F(PeerConnectionJsepTest,
       RollbackFromInactiveToReceivingMakesOnTrackFire) {
  auto caller = CreatePeerConnection();
  auto caller_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 0u);
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  caller_transceiver->SetDirectionWithError(RtpTransceiverDirection::kInactive);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);

  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateRollback()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->remove_track_events_.size(), 1u);
}

TEST_F(PeerConnectionJsepTest, RollbackAfterMultipleSLD) {
  auto callee = CreatePeerConnection();
  callee->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  callee->observer()->clear_legacy_renegotiation_needed();
  callee->observer()->clear_latest_negotiation_needed_event();
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  EXPECT_TRUE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(callee->observer()->has_negotiation_needed_event());
  EXPECT_EQ(callee->pc()->GetTransceivers().size(), 2u);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->mid(), absl::nullopt);
  EXPECT_EQ(callee->pc()->GetTransceivers()[1]->mid(), absl::nullopt);
}

TEST_F(PeerConnectionJsepTest, NoRollbackNeeded) {
  auto caller = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto callee = CreatePeerConnection();
  callee->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(caller->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
}

TEST_F(PeerConnectionJsepTest, RollbackMultipleStreamChanges) {
  auto callee = CreatePeerConnection();
  auto caller = CreatePeerConnection();
  caller->AddAudioTrack("a_1", {"id_1"});
  caller->AddVideoTrack("v_0", {"id_0"});
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));
  caller->pc()->GetTransceivers()[0]->sender()->SetStreams({"id_2"});
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  caller->pc()->GetTransceivers()[0]->sender()->SetStreams({"id_3"});
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->receiver()->stream_ids()[0],
            "id_3");
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->receiver()->stream_ids().size(),
            1u);
  EXPECT_EQ(callee->pc()->GetTransceivers()[0]->receiver()->stream_ids()[0],
            "id_1");
}

TEST_F(PeerConnectionJsepTest, DataChannelImplicitRollback) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  config.enable_implicit_rollback = true;
  auto caller = CreatePeerConnection(config);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto callee = CreatePeerConnection(config);
  callee->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->CreateAnswerAndSetAsLocal());
  EXPECT_TRUE(callee->observer()->legacy_renegotiation_needed());
  EXPECT_TRUE(callee->observer()->has_negotiation_needed_event());
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest, RollbackRemoteDataChannelThenAddTransceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest,
       RollbackRemoteDataChannelThenAddTransceiverAndDataChannel) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  callee->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest, RollbackRemoteDataChannelThenAddDataChannel) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  callee->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest, RollbackRemoteTransceiverThenAddDataChannel) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  callee->CreateDataChannel("dummy");
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest,
       RollbackRemoteTransceiverThenAddDataChannelAndTransceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));
  EXPECT_TRUE(callee->SetRemoteDescription(callee->CreateRollback()));
  callee->CreateDataChannel("dummy");
  callee->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_TRUE(callee->CreateOfferAndSetAsLocal());
}

TEST_F(PeerConnectionJsepTest, BundleOnlySectionDoesNotNeedRtcpMux) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto offer = caller->CreateOffer();

  offer->description()->contents()[1].media_description()->set_rtcp_mux(false);
  offer->description()->contents()[1].bundle_only = true;

  EXPECT_TRUE(callee->SetRemoteDescription(std::move(offer)));
}

}
