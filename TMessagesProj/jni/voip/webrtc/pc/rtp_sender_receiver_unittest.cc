/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/media_stream_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/fake_frame_decryptor.h"
#include "api/test/fake_frame_encryptor.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video/video_codec_constants.h"
#include "media/base/codec.h"
#include "media/base/fake_media_engine.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "media/base/rid_description.h"
#include "media/base/stream_params.h"
#include "media/base/test_utils.h"
#include "media/engine/fake_webrtc_call.h"
#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/fake_dtls_transport.h"
#include "p2p/base/p2p_constants.h"
#include "pc/audio_rtp_receiver.h"
#include "pc/audio_track.h"
#include "pc/channel.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/local_audio_source.h"
#include "pc/media_stream.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_transport_internal.h"
#include "pc/test/fake_video_track_source.h"
#include "pc/video_rtp_receiver.h"
#include "pc/video_track.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/scoped_key_value_config.h"

using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using RidList = std::vector<std::string>;

namespace {

static const char kStreamId1[] = "local_stream_1";
static const char kVideoTrackId[] = "video_1";
static const char kAudioTrackId[] = "audio_1";
static const uint32_t kVideoSsrc = 98;
static const uint32_t kVideoSsrc2 = 100;
static const uint32_t kAudioSsrc = 99;
static const uint32_t kAudioSsrc2 = 101;
static const uint32_t kVideoSsrcSimulcast = 102;
static const uint32_t kVideoSimulcastLayerCount = 2;
static const int kDefaultTimeout = 10000;

class MockSetStreamsObserver
    : public webrtc::RtpSenderBase::SetStreamsObserver {
 public:
  MOCK_METHOD(void, OnSetStreams, (), (override));
};

}

namespace webrtc {

class RtpSenderReceiverTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::pair<RidList, RidList>> {
 public:
  RtpSenderReceiverTest()
      : network_thread_(rtc::Thread::Current()),
        worker_thread_(rtc::Thread::Current()),
        video_bitrate_allocator_factory_(
            CreateBuiltinVideoBitrateAllocatorFactory()),

        media_engine_(std::make_unique<cricket::FakeMediaEngine>()),
        fake_call_(worker_thread_, network_thread_),
        local_stream_(MediaStream::Create(kStreamId1)) {
    rtp_dtls_transport_ = std::make_unique<cricket::FakeDtlsTransport>(
        "fake_dtls_transport", cricket::ICE_CANDIDATE_COMPONENT_RTP);
    rtp_transport_ = CreateDtlsSrtpTransport();

    voice_media_send_channel_ = media_engine_->voice().CreateSendChannel(
        &fake_call_, cricket::MediaConfig(), cricket::AudioOptions(),
        CryptoOptions(), AudioCodecPairId::Create());
    video_media_send_channel_ = media_engine_->video().CreateSendChannel(
        &fake_call_, cricket::MediaConfig(), cricket::VideoOptions(),
        CryptoOptions(), video_bitrate_allocator_factory_.get());
    voice_media_receive_channel_ = media_engine_->voice().CreateReceiveChannel(
        &fake_call_, cricket::MediaConfig(), cricket::AudioOptions(),
        CryptoOptions(), AudioCodecPairId::Create());
    video_media_receive_channel_ = media_engine_->video().CreateReceiveChannel(
        &fake_call_, cricket::MediaConfig(), cricket::VideoOptions(),
        CryptoOptions());

    voice_media_send_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc));
    voice_media_receive_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc));
    voice_media_send_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc2));
    voice_media_receive_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kAudioSsrc2));
    video_media_send_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc));
    video_media_receive_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc));
    video_media_send_channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc2));
    video_media_receive_channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kVideoSsrc2));
  }

  ~RtpSenderReceiverTest() {
    audio_rtp_sender_ = nullptr;
    video_rtp_sender_ = nullptr;
    audio_rtp_receiver_ = nullptr;
    video_rtp_receiver_ = nullptr;
    local_stream_ = nullptr;
    video_track_ = nullptr;
    audio_track_ = nullptr;
  }

  std::unique_ptr<RtpTransportInternal> CreateDtlsSrtpTransport() {
    auto dtls_srtp_transport = std::make_unique<DtlsSrtpTransport>(
                              true, field_trials_);
    dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport_.get(),
                                                                   nullptr);
    return dtls_srtp_transport;
  }

  void AddDtmfCodec() {
    cricket::AudioSenderParameter params;
    const cricket::AudioCodec kTelephoneEventCodec =
        cricket::CreateAudioCodec(106, "telephone-event", 8000, 1);
    params.codecs.push_back(kTelephoneEventCodec);
    voice_media_send_channel()->SetSenderParameters(params);
  }

  void AddVideoTrack() { AddVideoTrack(false); }

  void AddVideoTrack(bool is_screencast) {
    rtc::scoped_refptr<VideoTrackSourceInterface> source(
        FakeVideoTrackSource::Create(is_screencast));
    video_track_ =
        VideoTrack::Create(kVideoTrackId, source, rtc::Thread::Current());
    EXPECT_TRUE(local_stream_->AddTrack(video_track_));
  }

  void CreateAudioRtpSender() { CreateAudioRtpSender(nullptr); }

  void CreateAudioRtpSender(
      const rtc::scoped_refptr<LocalAudioSource>& source) {
    audio_track_ = AudioTrack::Create(kAudioTrackId, source);
    EXPECT_TRUE(local_stream_->AddTrack(audio_track_));
    std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
        std::make_unique<MockSetStreamsObserver>();
    audio_rtp_sender_ =
        AudioRtpSender::Create(worker_thread_, audio_track_->id(), nullptr,
                               set_streams_observer.get());
    ASSERT_TRUE(audio_rtp_sender_->SetTrack(audio_track_.get()));
    EXPECT_CALL(*set_streams_observer, OnSetStreams());
    audio_rtp_sender_->SetStreams({local_stream_->id()});
    audio_rtp_sender_->SetMediaChannel(voice_media_send_channel_.get());
    audio_rtp_sender_->SetSsrc(kAudioSsrc);
    VerifyVoiceChannelInput();
  }

  void CreateAudioRtpSenderWithNoTrack() {
    audio_rtp_sender_ =
        AudioRtpSender::Create(worker_thread_,        "", nullptr, nullptr);
    audio_rtp_sender_->SetMediaChannel(voice_media_send_channel_.get());
  }

  void CreateVideoRtpSender(uint32_t ssrc) {
    CreateVideoRtpSender(false, ssrc);
  }

  void CreateVideoRtpSender() { CreateVideoRtpSender(false); }

  cricket::StreamParams CreateSimulcastStreamParams(int num_layers) {
    std::vector<uint32_t> ssrcs;
    ssrcs.reserve(num_layers);
    for (int i = 0; i < num_layers; ++i) {
      ssrcs.push_back(kVideoSsrcSimulcast + i);
    }
    return cricket::CreateSimStreamParams("cname", ssrcs);
  }

  uint32_t CreateVideoRtpSender(const cricket::StreamParams& stream_params) {
    video_media_send_channel_->AddSendStream(stream_params);
    uint32_t primary_ssrc = stream_params.first_ssrc();
    CreateVideoRtpSender(primary_ssrc);
    return primary_ssrc;
  }

  uint32_t CreateVideoRtpSenderWithSimulcast(
      int num_layers = kVideoSimulcastLayerCount) {
    return CreateVideoRtpSender(CreateSimulcastStreamParams(num_layers));
  }

  uint32_t CreateVideoRtpSenderWithSimulcast(
      const std::vector<std::string>& rids) {
    cricket::StreamParams stream_params =
        CreateSimulcastStreamParams(rids.size());
    std::vector<cricket::RidDescription> rid_descriptions;
    absl::c_transform(
        rids, std::back_inserter(rid_descriptions), [](const std::string& rid) {
          return cricket::RidDescription(rid, cricket::RidDirection::kSend);
        });
    stream_params.set_rids(rid_descriptions);
    return CreateVideoRtpSender(stream_params);
  }

  void CreateVideoRtpSender(bool is_screencast, uint32_t ssrc = kVideoSsrc) {
    AddVideoTrack(is_screencast);
    std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
        std::make_unique<MockSetStreamsObserver>();
    video_rtp_sender_ = VideoRtpSender::Create(
        worker_thread_, video_track_->id(), set_streams_observer.get());
    ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
    EXPECT_CALL(*set_streams_observer, OnSetStreams());
    video_rtp_sender_->SetStreams({local_stream_->id()});
    video_rtp_sender_->SetMediaChannel(video_media_send_channel());
    video_rtp_sender_->SetSsrc(ssrc);
    VerifyVideoChannelInput(ssrc);
  }
  void CreateVideoRtpSenderWithNoTrack() {
    video_rtp_sender_ =
        VideoRtpSender::Create(worker_thread_,        "", nullptr);
    video_rtp_sender_->SetMediaChannel(video_media_send_channel());
  }

  void DestroyAudioRtpSender() {
    audio_rtp_sender_ = nullptr;
    VerifyVoiceChannelNoInput();
  }

  void DestroyVideoRtpSender() {
    video_rtp_sender_ = nullptr;
    VerifyVideoChannelNoInput();
  }

  void CreateAudioRtpReceiver(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {}) {
    audio_rtp_receiver_ = rtc::make_ref_counted<AudioRtpReceiver>(
        rtc::Thread::Current(), kAudioTrackId, streams,
                            true);
    audio_rtp_receiver_->SetMediaChannel(voice_media_receive_channel());
    audio_rtp_receiver_->SetupMediaChannel(kAudioSsrc);
    audio_track_ = audio_rtp_receiver_->audio_track();
    VerifyVoiceChannelOutput();
  }

  void CreateVideoRtpReceiver(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {}) {
    video_rtp_receiver_ = rtc::make_ref_counted<VideoRtpReceiver>(
        rtc::Thread::Current(), kVideoTrackId, streams);
    video_rtp_receiver_->SetMediaChannel(video_media_receive_channel());
    video_rtp_receiver_->SetupMediaChannel(kVideoSsrc);
    video_track_ = video_rtp_receiver_->video_track();
    VerifyVideoChannelOutput();
  }

  void CreateVideoRtpReceiverWithSimulcast(
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams = {},
      int num_layers = kVideoSimulcastLayerCount) {
    std::vector<uint32_t> ssrcs;
    ssrcs.reserve(num_layers);
    for (int i = 0; i < num_layers; ++i)
      ssrcs.push_back(kVideoSsrcSimulcast + i);
    cricket::StreamParams stream_params =
        cricket::CreateSimStreamParams("cname", ssrcs);
    video_media_receive_channel_->AddRecvStream(stream_params);
    uint32_t primary_ssrc = stream_params.first_ssrc();

    video_rtp_receiver_ = rtc::make_ref_counted<VideoRtpReceiver>(
        rtc::Thread::Current(), kVideoTrackId, streams);
    video_rtp_receiver_->SetMediaChannel(video_media_receive_channel());
    video_rtp_receiver_->SetupMediaChannel(primary_ssrc);
    video_track_ = video_rtp_receiver_->video_track();
  }

  void DestroyAudioRtpReceiver() {
    if (!audio_rtp_receiver_)
      return;
    audio_rtp_receiver_->SetMediaChannel(nullptr);
    audio_rtp_receiver_ = nullptr;
    VerifyVoiceChannelNoOutput();
  }

  void DestroyVideoRtpReceiver() {
    if (!video_rtp_receiver_)
      return;
    video_rtp_receiver_->Stop();
    video_rtp_receiver_->SetMediaChannel(nullptr);
    video_rtp_receiver_ = nullptr;
    VerifyVideoChannelNoOutput();
  }

  void VerifyVoiceChannelInput() { VerifyVoiceChannelInput(kAudioSsrc); }

  void VerifyVoiceChannelInput(uint32_t ssrc) {

    EXPECT_TRUE(voice_media_send_channel()->HasSource(ssrc));
    EXPECT_FALSE(voice_media_send_channel()->IsStreamMuted(ssrc));
  }

  void VerifyVideoChannelInput() { VerifyVideoChannelInput(kVideoSsrc); }

  void VerifyVideoChannelInput(uint32_t ssrc) {

    EXPECT_TRUE(video_media_send_channel()->HasSource(ssrc));
  }

  void VerifyVoiceChannelNoInput() { VerifyVoiceChannelNoInput(kAudioSsrc); }

  void VerifyVoiceChannelNoInput(uint32_t ssrc) {

    EXPECT_FALSE(voice_media_receive_channel()->HasSource(ssrc));
  }

  void VerifyVideoChannelNoInput() { VerifyVideoChannelNoInput(kVideoSsrc); }

  void VerifyVideoChannelNoInput(uint32_t ssrc) {

    EXPECT_FALSE(video_media_receive_channel()->HasSource(ssrc));
  }

  void VerifyVoiceChannelOutput() {

    double volume;
    EXPECT_TRUE(
        voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
    EXPECT_EQ(1, volume);
  }

  void VerifyVideoChannelOutput() {

    EXPECT_TRUE(video_media_receive_channel()->HasSink(kVideoSsrc));
  }

  void VerifyVoiceChannelNoOutput() {

    double volume;
    EXPECT_TRUE(
        voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
    EXPECT_EQ(0, volume);
  }

  void VerifyVideoChannelNoOutput() {

    EXPECT_FALSE(video_media_receive_channel()->HasSink(kVideoSsrc));
  }

  bool VerifyEncodingLayers(const VideoRtpSender& sender,
                            const std::vector<std::string>& rids) {
    bool has_failure = HasFailure();
    RtpParameters parameters = sender.GetParameters();
    std::vector<std::string> encoding_rids;
    absl::c_transform(
        parameters.encodings, std::back_inserter(encoding_rids),
        [](const RtpEncodingParameters& encoding) { return encoding.rid; });
    EXPECT_THAT(rids, ContainerEq(encoding_rids));
    return has_failure || !HasFailure();
  }

  void RunDisableEncodingLayersTest(
      const std::vector<std::string>& all_layers,
      const std::vector<std::string>& disabled_layers,
      VideoRtpSender* sender) {
    std::vector<std::string> expected;
    absl::c_copy_if(all_layers, std::back_inserter(expected),
                    [&disabled_layers](const std::string& rid) {
                      return !absl::c_linear_search(disabled_layers, rid);
                    });

    EXPECT_TRUE(VerifyEncodingLayers(*sender, all_layers));
    sender->DisableEncodingLayers(disabled_layers);
    EXPECT_TRUE(VerifyEncodingLayers(*sender, expected));
  }

  void RunSetLastLayerAsInactiveTest(VideoRtpSender* sender) {
    auto parameters = sender->GetParameters();
    if (parameters.encodings.size() == 0) {
      return;
    }

    RtpEncodingParameters& encoding = parameters.encodings.back();
    auto rid = encoding.rid;
    EXPECT_TRUE(encoding.active);
    encoding.active = false;
    auto error = sender->SetParameters(parameters);
    ASSERT_TRUE(error.ok());
    parameters = sender->GetParameters();
    RtpEncodingParameters& result_encoding = parameters.encodings.back();
    EXPECT_EQ(rid, result_encoding.rid);
    EXPECT_FALSE(result_encoding.active);
  }

  void RunDisableSimulcastLayersWithoutMediaEngineTest(
      const std::vector<std::string>& all_layers,
      const std::vector<std::string>& disabled_layers) {
    auto sender = VideoRtpSender::Create(rtc::Thread::Current(), "1", nullptr);
    RtpParameters parameters;
    parameters.encodings.resize(all_layers.size());
    for (size_t i = 0; i < all_layers.size(); ++i) {
      parameters.encodings[i].rid = all_layers[i];
    }
    sender->set_init_send_encodings(parameters.encodings);
    RunDisableEncodingLayersTest(all_layers, disabled_layers, sender.get());
    RunSetLastLayerAsInactiveTest(sender.get());
  }

  void RunDisableSimulcastLayersWithMediaEngineTest(
      const std::vector<std::string>& all_layers,
      const std::vector<std::string>& disabled_layers) {
    uint32_t ssrc = CreateVideoRtpSenderWithSimulcast(all_layers);
    RunDisableEncodingLayersTest(all_layers, disabled_layers,
                                 video_rtp_sender_.get());

    auto channel_parameters =
        video_media_send_channel_->GetRtpSendParameters(ssrc);
    ASSERT_EQ(channel_parameters.encodings.size(), all_layers.size());
    for (size_t i = 0; i < all_layers.size(); ++i) {
      EXPECT_EQ(all_layers[i], channel_parameters.encodings[i].rid);
      bool is_active = !absl::c_linear_search(disabled_layers, all_layers[i]);
      EXPECT_EQ(is_active, channel_parameters.encodings[i].active);
    }

    RunSetLastLayerAsInactiveTest(video_rtp_sender_.get());
  }

  void VerifyRtpReceiverDelayBehaviour(
      cricket::MediaReceiveChannelInterface* media_channel,
      RtpReceiverInterface* receiver,
      uint32_t ssrc) {
    receiver->SetJitterBufferMinimumDelay(                  0.5);
    absl::optional<int> delay_ms =
        media_channel->GetBaseMinimumPlayoutDelayMs(ssrc);
    EXPECT_DOUBLE_EQ(0.5, delay_ms.value_or(0) / 1000.0);
  }

 protected:
  cricket::FakeVideoMediaSendChannel* video_media_send_channel() {
    return static_cast<cricket::FakeVideoMediaSendChannel*>(
        video_media_send_channel_.get());
  }
  cricket::FakeVoiceMediaSendChannel* voice_media_send_channel() {
    return static_cast<cricket::FakeVoiceMediaSendChannel*>(
        voice_media_send_channel_.get());
  }
  cricket::FakeVideoMediaReceiveChannel* video_media_receive_channel() {
    return static_cast<cricket::FakeVideoMediaReceiveChannel*>(
        video_media_receive_channel_.get());
  }
  cricket::FakeVoiceMediaReceiveChannel* voice_media_receive_channel() {
    return static_cast<cricket::FakeVoiceMediaReceiveChannel*>(
        voice_media_receive_channel_.get());
  }

  test::RunLoop run_loop_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  RtcEventLogNull event_log_;

  std::unique_ptr<cricket::DtlsTransportInternal> rtp_dtls_transport_;
  std::unique_ptr<RtpTransportInternal> rtp_transport_;
  std::unique_ptr<VideoBitrateAllocatorFactory>
      video_bitrate_allocator_factory_;
  std::unique_ptr<cricket::FakeMediaEngine> media_engine_;
  rtc::UniqueRandomIdGenerator ssrc_generator_;
  cricket::FakeCall fake_call_;
  std::unique_ptr<cricket::VoiceMediaSendChannelInterface>
      voice_media_send_channel_;
  std::unique_ptr<cricket::VideoMediaSendChannelInterface>
      video_media_send_channel_;
  std::unique_ptr<cricket::VoiceMediaReceiveChannelInterface>
      voice_media_receive_channel_;
  std::unique_ptr<cricket::VideoMediaReceiveChannelInterface>
      video_media_receive_channel_;
  rtc::scoped_refptr<AudioRtpSender> audio_rtp_sender_;
  rtc::scoped_refptr<VideoRtpSender> video_rtp_sender_;
  rtc::scoped_refptr<AudioRtpReceiver> audio_rtp_receiver_;
  rtc::scoped_refptr<VideoRtpReceiver> video_rtp_receiver_;
  rtc::scoped_refptr<MediaStreamInterface> local_stream_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
  rtc::scoped_refptr<AudioTrackInterface> audio_track_;
  test::ScopedKeyValueConfig field_trials_;
};

TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpSender) {
  CreateAudioRtpSender();
  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpSender) {
  CreateVideoRtpSender();
  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpReceiver) {
  CreateAudioRtpReceiver();
  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpReceiver) {
  CreateVideoRtpReceiver();
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyAudioRtpReceiverWithStreams) {
  CreateAudioRtpReceiver({local_stream_});
  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AddAndDestroyVideoRtpReceiverWithStreams) {
  CreateVideoRtpReceiver({local_stream_});
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, LocalAudioSourceOptionsApplied) {
  cricket::AudioOptions options;
  options.echo_cancellation = true;
  auto source = LocalAudioSource::Create(&options);
  CreateAudioRtpSender(source);

  EXPECT_EQ(true, voice_media_send_channel()->options().echo_cancellation);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, LocalAudioTrackDisable) {
  CreateAudioRtpSender();

  audio_track_->set_enabled(false);
  EXPECT_TRUE(voice_media_send_channel()->IsStreamMuted(kAudioSsrc));

  audio_track_->set_enabled(true);
  EXPECT_FALSE(voice_media_send_channel()->IsStreamMuted(kAudioSsrc));

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, RemoteAudioTrackDisable) {
  CreateAudioRtpReceiver();

  double volume;
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(1, volume);

  audio_track_->set_enabled(false);
  run_loop_.Flush();

  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0, volume);

  audio_track_->set_enabled(true);
  run_loop_.Flush();
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(1, volume);

  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, LocalVideoTrackDisable) {
  CreateVideoRtpSender();

  video_track_->set_enabled(false);
  video_track_->set_enabled(true);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, RemoteVideoTrackState) {
  CreateVideoRtpReceiver();

  EXPECT_EQ(MediaStreamTrackInterface::kLive, video_track_->state());
  EXPECT_EQ(MediaSourceInterface::kLive, video_track_->GetSource()->state());

  DestroyVideoRtpReceiver();

  EXPECT_EQ(MediaStreamTrackInterface::kEnded, video_track_->state());
  EXPECT_EQ(MediaSourceInterface::kEnded, video_track_->GetSource()->state());
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, RemoteVideoTrackDisable) {
  CreateVideoRtpReceiver();

  video_track_->set_enabled(false);
  video_track_->set_enabled(true);

  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, RemoteAudioTrackSetVolume) {
  CreateAudioRtpReceiver();

  double volume;
  audio_track_->GetSource()->SetVolume(0.5);
  run_loop_.Flush();
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.5, volume);

  audio_track_->set_enabled(false);
  RTC_DCHECK_EQ(worker_thread_, run_loop_.task_queue());
  run_loop_.Flush();
  audio_track_->GetSource()->SetVolume(0.8);
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0, volume);

  audio_track_->set_enabled(true);
  run_loop_.Flush();
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.8, volume);

  audio_track_->GetSource()->SetVolume(0.9);
  run_loop_.Flush();
  EXPECT_TRUE(
      voice_media_receive_channel()->GetOutputVolume(kAudioSsrc, &volume));
  EXPECT_EQ(0.9, volume);

  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AudioRtpReceiverDelay) {
  CreateAudioRtpReceiver();
  VerifyRtpReceiverDelayBehaviour(
      voice_media_receive_channel()->AsVoiceReceiveChannel(),
      audio_rtp_receiver_.get(), kAudioSsrc);
  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, VideoRtpReceiverDelay) {
  CreateVideoRtpReceiver();
  VerifyRtpReceiverDelayBehaviour(
      video_media_receive_channel()->AsVideoReceiveChannel(),
      video_rtp_receiver_.get(), kVideoSsrc);
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AudioSenderWithoutTrackAndSsrc) {
  CreateAudioRtpSenderWithNoTrack();
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);

  EXPECT_TRUE(audio_rtp_sender_->SetTrack(track.get()));
  VerifyVoiceChannelNoInput();

  EXPECT_TRUE(audio_rtp_sender_->SetTrack(nullptr));
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  VerifyVoiceChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, VideoSenderWithoutTrackAndSsrc) {
  CreateVideoRtpSenderWithNoTrack();

  EXPECT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
  VerifyVideoChannelNoInput();

  EXPECT_TRUE(video_rtp_sender_->SetTrack(nullptr));
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  VerifyVideoChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupSsrcThenTrack) {
  CreateAudioRtpSenderWithNoTrack();
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  audio_rtp_sender_->SetTrack(track.get());
  VerifyVoiceChannelInput();

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderEarlyWarmupTrackThenSsrc) {
  CreateAudioRtpSenderWithNoTrack();
  rtc::scoped_refptr<AudioTrackInterface> track =
      AudioTrack::Create(kAudioTrackId, nullptr);
  audio_rtp_sender_->SetTrack(track.get());
  audio_rtp_sender_->SetSsrc(kAudioSsrc);
  VerifyVoiceChannelInput();

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupSsrcThenTrack) {
  AddVideoTrack();
  CreateVideoRtpSenderWithNoTrack();
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  video_rtp_sender_->SetTrack(video_track_.get());
  VerifyVideoChannelInput();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderEarlyWarmupTrackThenSsrc) {
  AddVideoTrack();
  CreateVideoRtpSenderWithNoTrack();
  video_rtp_sender_->SetTrack(video_track_.get());
  video_rtp_sender_->SetSsrc(kVideoSsrc);
  VerifyVideoChannelInput();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderSsrcSetToZero) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(0);
  VerifyVoiceChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, VideoSenderSsrcSetToZero) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(0);
  VerifyVideoChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, AudioSenderTrackSetToNull) {
  CreateAudioRtpSender();

  EXPECT_TRUE(audio_rtp_sender_->SetTrack(nullptr));
  VerifyVoiceChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, VideoSenderTrackSetToNull) {
  CreateVideoRtpSender();

  video_rtp_sender_->SetSsrc(0);
  VerifyVideoChannelNoInput();
}

TEST_F(RtpSenderReceiverTest, AudioSenderSsrcChanged) {
  CreateAudioRtpSender();

  audio_rtp_sender_->SetSsrc(kAudioSsrc2);
  VerifyVoiceChannelNoInput(kAudioSsrc);
  VerifyVoiceChannelInput(kAudioSsrc2);

  audio_rtp_sender_ = nullptr;
  VerifyVoiceChannelNoInput(kAudioSsrc2);
}

TEST_F(RtpSenderReceiverTest, VideoSenderSsrcChanged) {
  CreateVideoRtpSender();

  video_rtp_sender_->SetSsrc(kVideoSsrc2);
  VerifyVideoChannelNoInput(kVideoSsrc);
  VerifyVideoChannelInput(kVideoSsrc2);

  video_rtp_sender_ = nullptr;
  VerifyVideoChannelNoInput(kVideoSsrc2);
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetParameters) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetParametersAsync) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  absl::optional<RTCError> result;
  audio_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetParametersBeforeNegotiation) {
  audio_rtp_sender_ =
      AudioRtpSender::Create(worker_thread_,        "", nullptr, nullptr);

  RtpParameters params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderCanSetParametersAsyncBeforeNegotiation) {
  audio_rtp_sender_ =
      AudioRtpSender::Create(worker_thread_,        "", nullptr, nullptr);

  absl::optional<RTCError> result;
  RtpParameters params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;

  audio_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);

  audio_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderInitParametersMovedAfterNegotiation) {
  audio_track_ = AudioTrack::Create(kAudioTrackId, nullptr);
  EXPECT_TRUE(local_stream_->AddTrack(audio_track_));

  std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
      std::make_unique<MockSetStreamsObserver>();
  audio_rtp_sender_ = AudioRtpSender::Create(
      worker_thread_, audio_track_->id(), nullptr, set_streams_observer.get());
  ASSERT_TRUE(audio_rtp_sender_->SetTrack(audio_track_.get()));
  EXPECT_CALL(*set_streams_observer, OnSetStreams());
  audio_rtp_sender_->SetStreams({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(1);
  init_encodings[0].max_bitrate_bps = 60000;
  audio_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  std::vector<uint32_t> ssrcs(1, 1);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  voice_media_send_channel()->AddSendStream(stream_params);
  audio_rtp_sender_->SetMediaChannel(
      voice_media_send_channel()->AsVoiceSendChannel());
  audio_rtp_sender_->SetSsrc(1);

  params = audio_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderMustCallGetParametersBeforeSetParametersBeforeNegotiation) {
  audio_rtp_sender_ =
      AudioRtpSender::Create(worker_thread_,        "", nullptr, nullptr);

  RtpParameters params;
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderMustCallGetParametersBeforeSetParameters) {
  CreateAudioRtpSender();

  RtpParameters params;
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderSetParametersInvalidatesTransactionId) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       AudioSenderSetParametersAsyncInvalidatesTransactionId) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  absl::optional<RTCError> result;
  audio_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());
  audio_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result->type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderDetectTransactionIdModification) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  params.transaction_id = "";
  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCheckTransactionIdRefresh) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_NE(params.transaction_id.size(), 0U);
  auto saved_transaction_id = params.transaction_id;
  params = audio_rtp_sender_->GetParameters();
  EXPECT_NE(saved_transaction_id, params.transaction_id);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderSetParametersOldValueFail) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  RtpParameters second_params = audio_rtp_sender_->GetParameters();

  RTCError result = audio_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());
  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderCantSetUnimplementedRtpParameters) {
  CreateAudioRtpSender();
  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  params.mid = "dummy_mid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            audio_rtp_sender_->SetParameters(params).type());
  params = audio_rtp_sender_->GetParameters();

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetAudioMaxSendBitrate) {
  CreateAudioRtpSender();

  EXPECT_EQ(-1, voice_media_send_channel()->max_bps());
  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].max_bitrate_bps = 1000;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  params = voice_media_send_channel()->GetRtpSendParameters(kAudioSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  EXPECT_EQ(-1, voice_media_send_channel()->max_bps());

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetAudioBitratePriority) {
  CreateAudioRtpSender();

  RtpParameters params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(kDefaultBitratePriority, params.encodings[0].bitrate_priority);
  double new_bitrate_priority = 2.0;
  params.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(audio_rtp_sender_->SetParameters(params).ok());

  params = audio_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  params = voice_media_send_channel()->GetRtpSendParameters(kAudioSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParameters) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParametersAsync) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  absl::optional<RTCError> result;
  video_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetParametersBeforeNegotiation) {
  video_rtp_sender_ =
      VideoRtpSender::Create(worker_thread_,        "", nullptr);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderCanSetParametersAsyncBeforeNegotiation) {
  video_rtp_sender_ =
      VideoRtpSender::Create(worker_thread_,        "", nullptr);

  absl::optional<RTCError> result;
  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  params.encodings[0].max_bitrate_bps = 90000;
  video_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 90000);
  video_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderInitParametersMovedAfterNegotiation) {
  AddVideoTrack(false);

  std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
      std::make_unique<MockSetStreamsObserver>();
  video_rtp_sender_ = VideoRtpSender::Create(worker_thread_, video_track_->id(),
                                             set_streams_observer.get());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
  EXPECT_CALL(*set_streams_observer, OnSetStreams());
  video_rtp_sender_->SetStreams({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(2);
  init_encodings[0].max_bitrate_bps = 60000;
  init_encodings[1].max_bitrate_bps = 900000;
  video_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);
  EXPECT_EQ(params.encodings[1].max_bitrate_bps, 900000);

  std::vector<uint32_t> ssrcs;
  ssrcs.reserve(2);
  for (int i = 0; i < 2; ++i)
    ssrcs.push_back(kVideoSsrcSimulcast + i);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  video_media_send_channel()->AddSendStream(stream_params);
  video_rtp_sender_->SetMediaChannel(
      video_media_send_channel()->AsVideoSendChannel());
  video_rtp_sender_->SetSsrc(kVideoSsrcSimulcast);

  params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);
  EXPECT_EQ(params.encodings[1].max_bitrate_bps, 900000);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderInitParametersMovedAfterManualSimulcastAndNegotiation) {
  AddVideoTrack(false);

  std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
      std::make_unique<MockSetStreamsObserver>();
  video_rtp_sender_ = VideoRtpSender::Create(worker_thread_, video_track_->id(),
                                             set_streams_observer.get());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
  EXPECT_CALL(*set_streams_observer, OnSetStreams());
  video_rtp_sender_->SetStreams({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(1);
  init_encodings[0].max_bitrate_bps = 60000;
  video_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(1u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  std::vector<uint32_t> ssrcs;
  ssrcs.reserve(2);
  for (int i = 0; i < 2; ++i)
    ssrcs.push_back(kVideoSsrcSimulcast + i);
  cricket::StreamParams stream_params =
      cricket::CreateSimStreamParams("cname", ssrcs);
  video_media_send_channel()->AddSendStream(stream_params);
  video_rtp_sender_->SetMediaChannel(
      video_media_send_channel()->AsVideoSendChannel());
  video_rtp_sender_->SetSsrc(kVideoSsrcSimulcast);

  params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  DestroyVideoRtpSender();
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
using RtpSenderReceiverDeathTest = RtpSenderReceiverTest;

TEST_F(RtpSenderReceiverDeathTest,
       VideoSenderManualRemoveSimulcastFailsDeathTest) {
  AddVideoTrack(false);

  std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
      std::make_unique<MockSetStreamsObserver>();
  video_rtp_sender_ = VideoRtpSender::Create(worker_thread_, video_track_->id(),
                                             set_streams_observer.get());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
  EXPECT_CALL(*set_streams_observer, OnSetStreams());
  video_rtp_sender_->SetStreams({local_stream_->id()});

  std::vector<RtpEncodingParameters> init_encodings(2);
  init_encodings[0].max_bitrate_bps = 60000;
  init_encodings[1].max_bitrate_bps = 120000;
  video_rtp_sender_->set_init_send_encodings(init_encodings);

  RtpParameters params = video_rtp_sender_->GetParameters();
  ASSERT_EQ(2u, params.encodings.size());
  EXPECT_EQ(params.encodings[0].max_bitrate_bps, 60000);

  std::vector<uint32_t> ssrcs;
  ssrcs.reserve(2);
  for (int i = 0; i < 2; ++i)
    ssrcs.push_back(kVideoSsrcSimulcast + i);
  cricket::StreamParams stream_params =
      cricket::StreamParams::CreateLegacy(kVideoSsrc);
  video_media_send_channel()->AddSendStream(stream_params);
  video_rtp_sender_->SetMediaChannel(
      video_media_send_channel()->AsVideoSendChannel());
  EXPECT_DEATH(video_rtp_sender_->SetSsrc(kVideoSsrcSimulcast), "");
}
#endif

TEST_F(RtpSenderReceiverTest,
       VideoSenderMustCallGetParametersBeforeSetParametersBeforeNegotiation) {
  video_rtp_sender_ =
      VideoRtpSender::Create(worker_thread_,        "", nullptr);

  RtpParameters params;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderMustCallGetParametersBeforeSetParameters) {
  CreateVideoRtpSender();

  RtpParameters params;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderSetParametersInvalidatesTransactionId) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       VideoSenderSetParametersAsyncInvalidatesTransactionId) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());
  absl::optional<RTCError> result;
  video_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_TRUE(result->ok());
  video_rtp_sender_->SetParametersAsync(
      params, [&result](RTCError error) { result = error; });
  run_loop_.Flush();
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result->type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderDetectTransactionIdModification) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.transaction_id = "";
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCheckTransactionIdRefresh) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_NE(params.transaction_id.size(), 0U);
  auto saved_transaction_id = params.transaction_id;
  params = video_rtp_sender_->GetParameters();
  EXPECT_NE(saved_transaction_id, params.transaction_id);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderSetParametersOldValueFail) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  RtpParameters second_params = video_rtp_sender_->GetParameters();

  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCantSetUnimplementedRtpParameters) {
  CreateVideoRtpSender();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1u, params.encodings.size());

  params.mid = "dummy_mid";
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetScaleResolutionDownBy) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].scale_resolution_down_by = 2;

  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(2, params.encodings[0].scale_resolution_down_by);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderDetectInvalidScaleResolutionDownBy) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].scale_resolution_down_by = 0.5;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetNumTemporalLayers) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].num_temporal_layers = 2;

  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(2, params.encodings[0].num_temporal_layers);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderDetectInvalidNumTemporalLayers) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].num_temporal_layers = kMaxTemporalStreams + 1;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetMaxFramerate) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].max_framerate = 20;

  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(20., params.encodings[0].max_framerate);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetMaxFramerateZero) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].max_framerate = 0.;

  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());
  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(0., params.encodings[0].max_framerate);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderDetectInvalidMaxFramerate) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  params.encodings[0].max_framerate = -5.;
  RTCError result = video_rtp_sender_->SetParameters(params);
  EXPECT_EQ(RTCErrorType::INVALID_RANGE, result.type());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCantSetPerSenderEncodingParameters) {

  CreateVideoRtpSenderWithSimulcast();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());

  params.encodings[1].bitrate_priority = 2.0;
  EXPECT_EQ(RTCErrorType::UNSUPPORTED_PARAMETER,
            video_rtp_sender_->SetParameters(params).type());
  params = video_rtp_sender_->GetParameters();

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCantSetReadOnlyEncodingParameters) {

  CreateVideoRtpSenderWithSimulcast();
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());

  for (size_t i = 0; i < params.encodings.size(); i++) {
    params.encodings[i].ssrc = 1337;
    EXPECT_EQ(RTCErrorType::INVALID_MODIFICATION,
              video_rtp_sender_->SetParameters(params).type());
    params = video_rtp_sender_->GetParameters();
  }

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoMinMaxSendBitrate) {
  CreateVideoRtpSender();

  EXPECT_EQ(-1, video_media_send_channel()->max_bps());
  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_FALSE(params.encodings[0].min_bitrate_bps);
  EXPECT_FALSE(params.encodings[0].max_bitrate_bps);
  params.encodings[0].min_bitrate_bps = 100;
  params.encodings[0].max_bitrate_bps = 1000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  params = video_media_send_channel()->GetRtpSendParameters(kVideoSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);

  EXPECT_EQ(-1, video_media_send_channel()->max_bps());

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoMinMaxSendBitrateSimulcast) {

  CreateVideoRtpSenderWithSimulcast();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());
  params.encodings[0].min_bitrate_bps = 100;
  params.encodings[0].max_bitrate_bps = 1000;
  params.encodings[1].min_bitrate_bps = 200;
  params.encodings[1].max_bitrate_bps = 2000;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params =
      video_media_send_channel()->GetRtpSendParameters(kVideoSsrcSimulcast);
  EXPECT_EQ(kVideoSimulcastLayerCount, params.encodings.size());
  EXPECT_EQ(100, params.encodings[0].min_bitrate_bps);
  EXPECT_EQ(1000, params.encodings[0].max_bitrate_bps);
  EXPECT_EQ(200, params.encodings[1].min_bitrate_bps);
  EXPECT_EQ(2000, params.encodings[1].max_bitrate_bps);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, SetVideoBitratePriority) {
  CreateVideoRtpSender();

  RtpParameters params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(kDefaultBitratePriority, params.encodings[0].bitrate_priority);
  double new_bitrate_priority = 2.0;
  params.encodings[0].bitrate_priority = new_bitrate_priority;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(params).ok());

  params = video_rtp_sender_->GetParameters();
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  params = video_media_send_channel()->GetRtpSendParameters(kVideoSsrc);
  EXPECT_EQ(1U, params.encodings.size());
  EXPECT_EQ(new_bitrate_priority, params.encodings[0].bitrate_priority);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, VideoReceiverCanGetParametersWithSimulcast) {
  CreateVideoRtpReceiverWithSimulcast({}, 2);

  RtpParameters params = video_rtp_receiver_->GetParameters();
  EXPECT_EQ(2u, params.encodings.size());

  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, GenerateKeyFrameWithAudio) {
  CreateAudioRtpSender();

  auto error = audio_rtp_sender_->GenerateKeyFrame({});
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::UNSUPPORTED_OPERATION);

  DestroyAudioRtpSender();
}

TEST_F(RtpSenderReceiverTest, GenerateKeyFrameWithVideo) {
  CreateVideoRtpSenderWithSimulcast({"1", "2", "3"});

  auto error = video_rtp_sender_->GenerateKeyFrame({});
  EXPECT_TRUE(error.ok());

  error = video_rtp_sender_->GenerateKeyFrame({"1"});
  EXPECT_TRUE(error.ok());

  error = video_rtp_sender_->GenerateKeyFrame({""});
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);

  error = video_rtp_sender_->GenerateKeyFrame({"no-such-rid"});
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.type(), RTCErrorType::INVALID_PARAMETER);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, PropagatesVideoTrackContentHint) {
  CreateVideoRtpSender();

  video_track_->set_enabled(true);

  EXPECT_EQ(false, video_media_send_channel()->options().is_screencast);

  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(false, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(false, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kText);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       PropagatesVideoTrackContentHintForScreencastSource) {
  CreateVideoRtpSender(true);

  video_track_->set_enabled(true);

  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  EXPECT_EQ(VideoTrackInterface::ContentHint::kNone,
            video_track_->content_hint());

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kFluid);
  EXPECT_EQ(false, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kText);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest,
       PropagatesVideoTrackContentHintSetBeforeEnabling) {
  AddVideoTrack();
  std::unique_ptr<MockSetStreamsObserver> set_streams_observer =
      std::make_unique<MockSetStreamsObserver>();

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kDetailed);
  video_rtp_sender_ = VideoRtpSender::Create(worker_thread_, video_track_->id(),
                                             set_streams_observer.get());
  ASSERT_TRUE(video_rtp_sender_->SetTrack(video_track_.get()));
  EXPECT_CALL(*set_streams_observer, OnSetStreams());
  video_rtp_sender_->SetStreams({local_stream_->id()});
  video_rtp_sender_->SetMediaChannel(
      video_media_send_channel()->AsVideoSendChannel());
  video_track_->set_enabled(true);

  EXPECT_EQ(absl::nullopt, video_media_send_channel()->options().is_screencast);

  video_rtp_sender_->SetSsrc(kVideoSsrc);
  EXPECT_EQ(true, video_media_send_channel()->options().is_screencast);

  video_track_->set_content_hint(VideoTrackInterface::ContentHint::kNone);
  EXPECT_EQ(false, video_media_send_channel()->options().is_screencast);

  DestroyVideoRtpSender();
}

TEST_F(RtpSenderReceiverTest, AudioSenderHasDtmfSender) {
  CreateAudioRtpSender();
  EXPECT_NE(nullptr, audio_rtp_sender_->GetDtmfSender());
}

TEST_F(RtpSenderReceiverTest, VideoSenderDoesNotHaveDtmfSender) {
  CreateVideoRtpSender();
  EXPECT_EQ(nullptr, video_rtp_sender_->GetDtmfSender());
}

TEST_F(RtpSenderReceiverTest, CanInsertDtmf) {
  AddDtmfCodec();
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);
  EXPECT_TRUE(dtmf_sender->CanInsertDtmf());
}

TEST_F(RtpSenderReceiverTest, CanNotInsertDtmf) {
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);

  EXPECT_FALSE(dtmf_sender->CanInsertDtmf());
}

TEST_F(RtpSenderReceiverTest, InsertDtmf) {
  AddDtmfCodec();
  CreateAudioRtpSender();
  auto dtmf_sender = audio_rtp_sender_->GetDtmfSender();
  ASSERT_NE(nullptr, dtmf_sender);

  EXPECT_EQ(0U, voice_media_send_channel()->dtmf_info_queue().size());

  const int expected_duration = 90;
  dtmf_sender->InsertDtmf("012", expected_duration, 100);

  ASSERT_EQ_WAIT(3U, voice_media_send_channel()->dtmf_info_queue().size(),
                 kDefaultTimeout);
  const uint32_t send_ssrc =
      voice_media_send_channel()->send_streams()[0].first_ssrc();
  EXPECT_TRUE(CompareDtmfInfo(voice_media_send_channel()->dtmf_info_queue()[0],
                              send_ssrc, 0, expected_duration));
  EXPECT_TRUE(CompareDtmfInfo(voice_media_send_channel()->dtmf_info_queue()[1],
                              send_ssrc, 1, expected_duration));
  EXPECT_TRUE(CompareDtmfInfo(voice_media_send_channel()->dtmf_info_queue()[2],
                              send_ssrc, 2, expected_duration));
}

TEST_F(RtpSenderReceiverTest, AudioSenderCanSetFrameEncryptor) {
  CreateAudioRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, audio_rtp_sender_->GetFrameEncryptor());
  audio_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  EXPECT_EQ(fake_frame_encryptor.get(),
            audio_rtp_sender_->GetFrameEncryptor().get());
}

TEST_F(RtpSenderReceiverTest, AudioSenderCannotSetFrameEncryptorAfterStop) {
  CreateAudioRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, audio_rtp_sender_->GetFrameEncryptor());
  audio_rtp_sender_->Stop();
  audio_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);

}

TEST_F(RtpSenderReceiverTest, AudioReceiverCanSetFrameDecryptor) {
  CreateAudioRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      rtc::make_ref_counted<FakeFrameDecryptor>());
  EXPECT_EQ(nullptr, audio_rtp_receiver_->GetFrameDecryptor());
  audio_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  EXPECT_EQ(fake_frame_decryptor.get(),
            audio_rtp_receiver_->GetFrameDecryptor().get());
  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, AudioReceiverCannotSetFrameDecryptorAfterStop) {
  CreateAudioRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      rtc::make_ref_counted<FakeFrameDecryptor>());
  EXPECT_EQ(nullptr, audio_rtp_receiver_->GetFrameDecryptor());
  audio_rtp_receiver_->SetMediaChannel(nullptr);
  audio_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);

  DestroyAudioRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, VideoSenderCanSetFrameEncryptor) {
  CreateVideoRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, video_rtp_sender_->GetFrameEncryptor());
  video_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);
  EXPECT_EQ(fake_frame_encryptor.get(),
            video_rtp_sender_->GetFrameEncryptor().get());
}

TEST_F(RtpSenderReceiverTest, VideoSenderCannotSetFrameEncryptorAfterStop) {
  CreateVideoRtpSender();
  rtc::scoped_refptr<FrameEncryptorInterface> fake_frame_encryptor(
      new FakeFrameEncryptor());
  EXPECT_EQ(nullptr, video_rtp_sender_->GetFrameEncryptor());
  video_rtp_sender_->Stop();
  video_rtp_sender_->SetFrameEncryptor(fake_frame_encryptor);

}

TEST_F(RtpSenderReceiverTest, VideoReceiverCanSetFrameDecryptor) {
  CreateVideoRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      rtc::make_ref_counted<FakeFrameDecryptor>());
  EXPECT_EQ(nullptr, video_rtp_receiver_->GetFrameDecryptor());
  video_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);
  EXPECT_EQ(fake_frame_decryptor.get(),
            video_rtp_receiver_->GetFrameDecryptor().get());
  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest, VideoReceiverCannotSetFrameDecryptorAfterStop) {
  CreateVideoRtpReceiver();
  rtc::scoped_refptr<FrameDecryptorInterface> fake_frame_decryptor(
      rtc::make_ref_counted<FakeFrameDecryptor>());
  EXPECT_EQ(nullptr, video_rtp_receiver_->GetFrameDecryptor());
  video_rtp_receiver_->SetMediaChannel(nullptr);
  video_rtp_receiver_->SetFrameDecryptor(fake_frame_decryptor);

  DestroyVideoRtpReceiver();
}

TEST_F(RtpSenderReceiverTest,
       InternalParameterMethodsDoNotInvalidateTransaction) {
  CreateVideoRtpSender();
  RtpParameters parameters = video_rtp_sender_->GetParameters();
  RtpParameters new_parameters = video_rtp_sender_->GetParametersInternal();
  new_parameters.encodings[0].active = false;
  video_rtp_sender_->SetParametersInternal(new_parameters, nullptr, true);
  new_parameters.encodings[0].active = true;
  video_rtp_sender_->SetParametersInternal(new_parameters, nullptr, true);
  parameters.encodings[0].active = false;
  EXPECT_TRUE(video_rtp_sender_->SetParameters(parameters).ok());
}

TEST_F(RtpSenderReceiverTest, SenderSetStreamsEliminatesDuplicateIds) {
  AddVideoTrack();
  video_rtp_sender_ =
      VideoRtpSender::Create(worker_thread_, video_track_->id(), nullptr);
  video_rtp_sender_->SetStreams({"1", "2", "1"});
  EXPECT_EQ(video_rtp_sender_->stream_ids().size(), 2u);
}

std::pair<RidList, RidList> CreatePairOfRidVectors(
    const std::vector<std::string>& first,
    const std::vector<std::string>& second) {
  return std::make_pair(first, second);
}

const std::pair<RidList, RidList> kDisableSimulcastLayersParameters[] = {

    CreatePairOfRidVectors({"1", "2", "3", "4"}, {"1"}),

    CreatePairOfRidVectors({"1", "2", "3", "4"}, {"2", "4"}),

    CreatePairOfRidVectors({"1", "2", "3", "4"}, {"2", "3", "4"}),

    CreatePairOfRidVectors({"1", "2", "3", "4"}, {"1", "2", "3", "4"}),
};

TEST_P(RtpSenderReceiverTest, DisableSimulcastLayersWithoutMediaEngine) {
  auto parameter = GetParam();
  RunDisableSimulcastLayersWithoutMediaEngineTest(parameter.first,
                                                  parameter.second);
}

TEST_P(RtpSenderReceiverTest, DisableSimulcastLayersWithMediaEngine) {
  auto parameter = GetParam();
  RunDisableSimulcastLayersWithMediaEngineTest(parameter.first,
                                               parameter.second);
}

INSTANTIATE_TEST_SUITE_P(
    DisableSimulcastLayersInSender,
    RtpSenderReceiverTest,
    ::testing::ValuesIn(kDisableSimulcastLayersParameters));

}
