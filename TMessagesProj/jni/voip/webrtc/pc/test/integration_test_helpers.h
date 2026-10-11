/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_INTEGRATION_TEST_HELPERS_H_
#define PC_TEST_INTEGRATION_TEST_HELPERS_H_

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/audio_options.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/data_channel_interface.h"
#include "api/enable_media_with_defaults.h"
#include "api/field_trials_view.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtc_event_log_output.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/mock_async_dns_resolver.h"
#include "api/transport/field_trial_based_config.h"
#include "api/uma_metrics.h"
#include "api/units/time_delta.h"
#include "api/video/video_rotation.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/call.h"
#include "logging/rtc_event_log/fake_rtc_event_log_factory.h"
#include "media/base/media_engine.h"
#include "media/base/stream_params.h"
#include "media/engine/fake_webrtc_video_engine.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "p2p/base/fake_ice_transport.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/test_stun_server.h"
#include "p2p/base/test_turn_customizer.h"
#include "p2p/base/test_turn_server.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/dtmf_sender.h"
#include "pc/local_audio_source.h"
#include "pc/media_session.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_factory.h"
#include "pc/peer_connection_proxy.h"
#include "pc/rtp_media_utils.h"
#include "pc/session_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_periodic_video_source.h"
#include "pc/test/fake_periodic_video_track_source.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "pc/test/fake_video_track_renderer.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_mdns_responder.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/firewall_socket_server.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/test_certificate_verifier.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {

using ::cricket::ContentInfo;
using ::cricket::StreamParams;
using ::rtc::SocketAddress;
using ::testing::_;
using ::testing::Combine;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::InvokeArgument;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;

static const int kDefaultTimeout = 10000;
static const int kMaxWaitForStatsMs = 3000;
static const int kMaxWaitForActivationMs = 5000;
static const int kMaxWaitForFramesMs = 10000;

static const int kDefaultExpectedAudioFrameCount = 3;
static const int kDefaultExpectedVideoFrameCount = 3;

static const char kDataChannelLabel[] = "data_channel";

static const int kDefaultSrtpCryptoSuite = rtc::kSrtpAes128CmSha1_80;
static const int kDefaultSrtpCryptoSuiteGcm = rtc::kSrtpAeadAes256Gcm;

static const SocketAddress kDefaultLocalAddress("192.168.1.1", 0);

PeerConnectionInterface::RTCOfferAnswerOptions IceRestartOfferAnswerOptions();

void RemoveSsrcsAndMsids(cricket::SessionDescription* desc);

void RemoveSsrcsAndKeepMsids(cricket::SessionDescription* desc);

void ReplaceFirstSsrc(StreamParams& stream, uint32_t ssrc);

int FindFirstMediaStatsIndexByKind(
    const std::string& kind,
    const std::vector<const RTCInboundRtpStreamStats*>& inbound_rtps);

class TaskQueueMetronome : public Metronome {
 public:
  explicit TaskQueueMetronome(TimeDelta tick_period);
  ~TaskQueueMetronome() override;

  void RequestCallOnNextTick(absl::AnyInvocable<void() &&> callback) override;
  TimeDelta TickPeriod() const override;

 private:
  const TimeDelta tick_period_;
  SequenceChecker sequence_checker_{SequenceChecker::kDetached};
  std::vector<absl::AnyInvocable<void() &&>> callbacks_;
  ScopedTaskSafetyDetached safety_;
};

class SignalingMessageReceiver {
 public:
  virtual void ReceiveSdpMessage(SdpType type, const std::string& msg) = 0;
  virtual void ReceiveIceMessage(const std::string& sdp_mid,
                                 int sdp_mline_index,
                                 const std::string& msg) = 0;

 protected:
  SignalingMessageReceiver() {}
  virtual ~SignalingMessageReceiver() {}
};

class MockRtpReceiverObserver : public RtpReceiverObserverInterface {
 public:
  explicit MockRtpReceiverObserver(cricket::MediaType media_type)
      : expected_media_type_(media_type) {}

  void OnFirstPacketReceived(cricket::MediaType media_type) override {
    ASSERT_EQ(expected_media_type_, media_type);
    first_packet_received_ = true;
  }

  bool first_packet_received() const { return first_packet_received_; }

  virtual ~MockRtpReceiverObserver() {}

 private:
  bool first_packet_received_ = false;
  cricket::MediaType expected_media_type_;
};

class PeerConnectionIntegrationWrapper : public PeerConnectionObserver,
                                         public SignalingMessageReceiver {
 public:
  PeerConnectionFactoryInterface* pc_factory() const {
    return peer_connection_factory_.get();
  }

  PeerConnectionInterface* pc() const { return peer_connection_.get(); }

  void CreateAndSetAndSignalOffer() {
    auto offer = CreateOfferAndWait();
    ASSERT_NE(nullptr, offer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(offer)));
  }

  void SetOfferAnswerOptions(
      const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
    offer_answer_options_ = options;
  }

  void SetReceivedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    received_sdp_munger_ = std::move(munger);
  }

  void SetGeneratedSdpMunger(
      std::function<void(cricket::SessionDescription*)> munger) {
    generated_sdp_munger_ = std::move(munger);
  }

  void SetRemoteOfferHandler(std::function<void()> handler) {
    remote_offer_handler_ = std::move(handler);
  }

  void SetRemoteAsyncResolver(MockAsyncDnsResolver* resolver) {
    remote_async_dns_resolver_ = resolver;
  }

  std::vector<PeerConnectionInterface::IceConnectionState>
  ice_connection_state_history() const {
    return ice_connection_state_history_;
  }
  void clear_ice_connection_state_history() {
    ice_connection_state_history_.clear();
  }

  std::vector<PeerConnectionInterface::IceConnectionState>
  standardized_ice_connection_state_history() const {
    return standardized_ice_connection_state_history_;
  }

  std::vector<PeerConnectionInterface::PeerConnectionState>
  peer_connection_state_history() const {
    return peer_connection_state_history_;
  }

  std::vector<PeerConnectionInterface::IceGatheringState>
  ice_gathering_state_history() const {
    return ice_gathering_state_history_;
  }
  std::vector<cricket::CandidatePairChangeEvent>
  ice_candidate_pair_change_history() const {
    return ice_candidate_pair_change_history_;
  }

  std::vector<PeerConnectionInterface::SignalingState>
  peer_connection_signaling_state_history() const {
    return peer_connection_signaling_state_history_;
  }

  void AddAudioVideoTracks() {
    AddAudioTrack();
    AddVideoTrack();
  }

  rtc::scoped_refptr<RtpSenderInterface> AddAudioTrack() {
    return AddTrack(CreateLocalAudioTrack());
  }

  rtc::scoped_refptr<RtpSenderInterface> AddVideoTrack() {
    return AddTrack(CreateLocalVideoTrack());
  }

  rtc::scoped_refptr<AudioTrackInterface> CreateLocalAudioTrack() {
    cricket::AudioOptions options;

    options.highpass_filter = false;
    rtc::scoped_refptr<AudioSourceInterface> source =
        peer_connection_factory_->CreateAudioSource(options);

    return peer_connection_factory_->CreateAudioTrack(rtc::CreateRandomUuid(),
                                                      source.get());
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrack() {
    FakePeriodicVideoSource::Config config;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrackWithConfig(
      FakePeriodicVideoSource::Config config) {
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrackWithRotation(
      VideoRotation rotation) {
    FakePeriodicVideoSource::Config config;
    config.rotation = rotation;
    config.timestamp_offset_ms = rtc::TimeMillis();
    return CreateLocalVideoTrackInternal(config);
  }

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {}) {
    EXPECT_TRUE(track);
    if (!track) {
      return nullptr;
    }
    auto result = pc()->AddTrack(track, stream_ids);
    EXPECT_EQ(RTCErrorType::NONE, result.error().type());
    if (result.ok()) {
      return result.MoveValue();
    } else {
      return nullptr;
    }
  }

  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> GetReceiversOfType(
      cricket::MediaType media_type) {
    std::vector<rtc::scoped_refptr<RtpReceiverInterface>> receivers;
    for (const auto& receiver : pc()->GetReceivers()) {
      if (receiver->media_type() == media_type) {
        receivers.push_back(receiver);
      }
    }
    return receivers;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> GetFirstTransceiverOfType(
      cricket::MediaType media_type) {
    for (auto transceiver : pc()->GetTransceivers()) {
      if (transceiver->receiver()->media_type() == media_type) {
        return transceiver;
      }
    }
    return nullptr;
  }

  bool SignalingStateStable() {
    return pc()->signaling_state() == PeerConnectionInterface::kStable;
  }

  bool IceGatheringStateComplete() {
    return pc()->ice_gathering_state() ==
           PeerConnectionInterface::kIceGatheringComplete;
  }

  void CreateDataChannel() { CreateDataChannel(nullptr); }

  void CreateDataChannel(const DataChannelInit* init) {
    CreateDataChannel(kDataChannelLabel, init);
  }

  void CreateDataChannel(const std::string& label,
                         const DataChannelInit* init) {
    auto data_channel_or_error = pc()->CreateDataChannelOrError(label, init);
    ASSERT_TRUE(data_channel_or_error.ok());
    data_channels_.push_back(data_channel_or_error.MoveValue());
    ASSERT_TRUE(data_channels_.back().get() != nullptr);
    data_observers_.push_back(
        std::make_unique<MockDataChannelObserver>(data_channels_.back().get()));
  }

  DataChannelInterface* data_channel() {
    if (data_channels_.size() == 0) {
      return nullptr;
    }
    return data_channels_.back().get();
  }

  std::vector<rtc::scoped_refptr<DataChannelInterface>>& data_channels() {
    return data_channels_;
  }

  const MockDataChannelObserver* data_observer() const {
    if (data_observers_.size() == 0) {
      return nullptr;
    }
    return data_observers_.back().get();
  }

  std::vector<std::unique_ptr<MockDataChannelObserver>>& data_observers() {
    return data_observers_;
  }

  int audio_frames_received() const {
    return fake_audio_capture_module_->frames_received();
  }

  int min_video_frames_received_per_track() const {
    int min_frames = INT_MAX;
    if (fake_video_renderers_.empty()) {
      return 0;
    }

    for (const auto& pair : fake_video_renderers_) {
      min_frames = std::min(min_frames, pair.second->num_rendered_frames());
    }
    return min_frames;
  }

  rtc::scoped_refptr<MockStatsObserver> OldGetStatsForTrack(
      MediaStreamTrackInterface* track) {
    auto observer = rtc::make_ref_counted<MockStatsObserver>();
    EXPECT_TRUE(peer_connection_->GetStats(
        observer.get(), nullptr,
        PeerConnectionInterface::kStatsOutputLevelStandard));
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer;
  }

  rtc::scoped_refptr<MockStatsObserver> OldGetStats() {
    return OldGetStatsForTrack(nullptr);
  }

  rtc::scoped_refptr<const RTCStatsReport> NewGetStats() {
    auto callback = rtc::make_ref_counted<MockRTCStatsCollectorCallback>();
    peer_connection_->GetStats(callback.get());
    EXPECT_TRUE_WAIT(callback->called(), kDefaultTimeout);
    return callback->report();
  }

  int rendered_width() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->width();
  }

  int rendered_height() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? 0
               : fake_video_renderers_.begin()->second->height();
  }

  double rendered_aspect_ratio() {
    if (rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(rendered_width()) / rendered_height();
  }

  VideoRotation rendered_rotation() {
    EXPECT_FALSE(fake_video_renderers_.empty());
    return fake_video_renderers_.empty()
               ? kVideoRotation_0
               : fake_video_renderers_.begin()->second->rotation();
  }

  int local_rendered_width() {
    return local_video_renderer_ ? local_video_renderer_->width() : 0;
  }

  int local_rendered_height() {
    return local_video_renderer_ ? local_video_renderer_->height() : 0;
  }

  double local_rendered_aspect_ratio() {
    if (local_rendered_height() == 0) {
      return 0.0;
    }
    return static_cast<double>(local_rendered_width()) /
           local_rendered_height();
  }

  size_t number_of_remote_streams() {
    if (!pc()) {
      return 0;
    }
    return pc()->remote_streams()->count();
  }

  StreamCollectionInterface* remote_streams() const {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->remote_streams().get();
  }

  StreamCollectionInterface* local_streams() {
    if (!pc()) {
      ADD_FAILURE();
      return nullptr;
    }
    return pc()->local_streams().get();
  }

  PeerConnectionInterface::SignalingState signaling_state() {
    return pc()->signaling_state();
  }

  PeerConnectionInterface::IceConnectionState ice_connection_state() {
    return pc()->ice_connection_state();
  }

  PeerConnectionInterface::IceConnectionState
  standardized_ice_connection_state() {
    return pc()->standardized_ice_connection_state();
  }

  PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

  const std::vector<std::unique_ptr<MockRtpReceiverObserver>>&
  rtp_receiver_observers() {
    return rtp_receiver_observers_;
  }

  void ResetRtpReceiverObservers() {
    rtp_receiver_observers_.clear();
    for (const rtc::scoped_refptr<RtpReceiverInterface>& receiver :
         pc()->GetReceivers()) {
      std::unique_ptr<MockRtpReceiverObserver> observer(
          new MockRtpReceiverObserver(receiver->media_type()));
      receiver->SetObserver(observer.get());
      rtp_receiver_observers_.push_back(std::move(observer));
    }
  }

  rtc::FakeNetworkManager* network_manager() const {
    return fake_network_manager_.get();
  }
  cricket::PortAllocator* port_allocator() const { return port_allocator_; }

  FakeRtcEventLogFactory* event_log_factory() const {
    return event_log_factory_;
  }

  const cricket::Candidate& last_candidate_gathered() const {
    return last_candidate_gathered_;
  }
  const cricket::IceCandidateErrorEvent& error_event() const {
    return error_event_;
  }

  void SetMdnsResponder(std::unique_ptr<FakeMdnsResponder> mdns_responder) {
    RTC_DCHECK(mdns_responder != nullptr);
    mdns_responder_ = mdns_responder.get();
    network_manager()->set_mdns_responder(std::move(mdns_responder));
  }

  std::unique_ptr<SessionDescriptionInterface> CreateOfferAndWait() {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc()->CreateOffer(observer.get(), offer_answer_options_);
    return WaitForDescriptionFromObserver(observer.get());
  }
  bool Rollback() {
    return SetRemoteDescription(
        CreateSessionDescription(SdpType::kRollback, ""));
  }

  void StartWatchingDelayStats() {

    auto received_stats = NewGetStats();
    auto rtp_stats =
        received_stats->GetStatsOfType<RTCInboundRtpStreamStats>()[0];
    ASSERT_TRUE(rtp_stats->relative_packet_arrival_delay.has_value());
    ASSERT_TRUE(rtp_stats->packets_received.has_value());
    rtp_stats_id_ = rtp_stats->id();
    audio_packets_stat_ = *rtp_stats->packets_received;
    audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
    audio_samples_stat_ = *rtp_stats->total_samples_received;
    audio_concealed_stat_ = *rtp_stats->concealed_samples;
  }

  void UpdateDelayStats(std::string tag, int desc_size) {
    auto report = NewGetStats();
    auto rtp_stats = report->GetAs<RTCInboundRtpStreamStats>(rtp_stats_id_);
    ASSERT_TRUE(rtp_stats);
    auto delta_packets = *rtp_stats->packets_received - audio_packets_stat_;
    auto delta_rpad =
        *rtp_stats->relative_packet_arrival_delay - audio_delay_stat_;
    auto recent_delay = delta_packets > 0 ? delta_rpad / delta_packets : -1;

#if !defined(NDEBUG)
    EXPECT_GT(0.25, recent_delay) << tag << " size " << desc_size;
#else
    EXPECT_GT(0.1, recent_delay) << tag << " size " << desc_size;
#endif
    auto delta_samples =
        *rtp_stats->total_samples_received - audio_samples_stat_;
    auto delta_concealed =
        *rtp_stats->concealed_samples - audio_concealed_stat_;

#if !defined(NDEBUG)
    EXPECT_GT(18000U, delta_concealed) << "Concealed " << delta_concealed
                                       << " of " << delta_samples << " samples";
#else
    EXPECT_GT(15000U, delta_concealed) << "Concealed " << delta_concealed
                                       << " of " << delta_samples << " samples";
#endif

    if (delta_samples > 0) {
#if !defined(NDEBUG)
      EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.99)
          << "Concealed " << delta_concealed << " of " << delta_samples
          << " samples";
#else
      EXPECT_LT(1.0 * delta_concealed / delta_samples, 0.7)
          << "Concealed " << delta_concealed << " of " << delta_samples
          << " samples";
#endif
    }

    audio_packets_stat_ = *rtp_stats->packets_received;
    audio_delay_stat_ = *rtp_stats->relative_packet_arrival_delay;
    audio_samples_stat_ = *rtp_stats->total_samples_received;
    audio_concealed_stat_ = *rtp_stats->concealed_samples;
  }

  void ExpectCandidates(int candidate_count) {
    candidates_expected_ = candidate_count;
  }

 private:

  explicit PeerConnectionIntegrationWrapper(const std::string& debug_name)
      : debug_name_(debug_name) {}

  bool Init(const PeerConnectionFactory::Options* options,
            const PeerConnectionInterface::RTCConfiguration* config,
            PeerConnectionDependencies dependencies,
            rtc::SocketServer* socket_server,
            rtc::Thread* network_thread,
            rtc::Thread* worker_thread,
            std::unique_ptr<FakeRtcEventLogFactory> event_log_factory,
            bool reset_encoder_factory,
            bool reset_decoder_factory,
            bool create_media_engine) {

    RTC_DCHECK(!peer_connection_);
    RTC_DCHECK(!peer_connection_factory_);

    fake_network_manager_.reset(new rtc::FakeNetworkManager());
    fake_network_manager_->AddInterface(kDefaultLocalAddress);

    socket_factory_.reset(new rtc::BasicPacketSocketFactory(socket_server));

    std::unique_ptr<cricket::PortAllocator> port_allocator(
        new cricket::BasicPortAllocator(fake_network_manager_.get(),
                                        socket_factory_.get()));
    port_allocator_ = port_allocator.get();
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
    if (!fake_audio_capture_module_) {
      return false;
    }
    rtc::Thread* const signaling_thread = rtc::Thread::Current();

    PeerConnectionFactoryDependencies pc_factory_dependencies;
    pc_factory_dependencies.network_thread = network_thread;
    pc_factory_dependencies.worker_thread = worker_thread;
    pc_factory_dependencies.signaling_thread = signaling_thread;
    pc_factory_dependencies.task_queue_factory =
        CreateDefaultTaskQueueFactory();
    pc_factory_dependencies.trials = std::make_unique<FieldTrialBasedConfig>();
    pc_factory_dependencies.decode_metronome =
        std::make_unique<TaskQueueMetronome>(TimeDelta::Millis(8));

    pc_factory_dependencies.adm = fake_audio_capture_module_;
    if (create_media_engine) {
      EnableMediaWithDefaults(pc_factory_dependencies);
    }

    if (reset_encoder_factory) {
      pc_factory_dependencies.video_encoder_factory.reset();
    }
    if (reset_decoder_factory) {
      pc_factory_dependencies.video_decoder_factory.reset();
    }

    if (!pc_factory_dependencies.audio_processing) {

      pc_factory_dependencies.audio_processing =
          AudioProcessingBuilderForTesting().Create();
    }

    if (event_log_factory) {
      event_log_factory_ = event_log_factory.get();
      pc_factory_dependencies.event_log_factory = std::move(event_log_factory);
    } else {
      pc_factory_dependencies.event_log_factory =
          std::make_unique<RtcEventLogFactory>();
    }
    peer_connection_factory_ =
        CreateModularPeerConnectionFactory(std::move(pc_factory_dependencies));

    if (!peer_connection_factory_) {
      return false;
    }
    if (options) {
      peer_connection_factory_->SetOptions(*options);
    }
    if (config) {
      sdp_semantics_ = config->sdp_semantics;
    }

    dependencies.allocator = std::move(port_allocator);
    peer_connection_ = CreatePeerConnection(config, std::move(dependencies));
    return peer_connection_.get() != nullptr;
  }

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration* config,
      PeerConnectionDependencies dependencies) {
    PeerConnectionInterface::RTCConfiguration modified_config;
    modified_config.sdp_semantics = sdp_semantics_;

    if (config) {
      modified_config = *config;
    }

    modified_config.set_cpu_adaptation(false);

    dependencies.observer = this;
    auto peer_connection_or_error =
        peer_connection_factory_->CreatePeerConnectionOrError(
            modified_config, std::move(dependencies));
    return peer_connection_or_error.ok() ? peer_connection_or_error.MoveValue()
                                         : nullptr;
  }

  void set_signaling_message_receiver(
      SignalingMessageReceiver* signaling_message_receiver) {
    signaling_message_receiver_ = signaling_message_receiver;
  }

  void set_signaling_delay_ms(int delay_ms) { signaling_delay_ms_ = delay_ms; }

  void set_signal_ice_candidates(bool signal) {
    signal_ice_candidates_ = signal;
  }

  rtc::scoped_refptr<VideoTrackInterface> CreateLocalVideoTrackInternal(
      FakePeriodicVideoSource::Config config) {

    config.frame_interval_ms = 100;

    video_track_sources_.emplace_back(
        rtc::make_ref_counted<FakePeriodicVideoTrackSource>(
            config, false             ));
    rtc::scoped_refptr<VideoTrackInterface> track =
        peer_connection_factory_->CreateVideoTrack(video_track_sources_.back(),
                                                   rtc::CreateRandomUuid());
    if (!local_video_renderer_) {
      local_video_renderer_.reset(new FakeVideoTrackRenderer(track.get()));
    }
    return track;
  }

  void HandleIncomingOffer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingOffer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        CreateSessionDescription(SdpType::kOffer, msg);
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));

    ResetRtpReceiverObservers();
    if (remote_offer_handler_) {
      remote_offer_handler_();
    }
    auto answer = CreateAnswer();
    ASSERT_NE(nullptr, answer);
    EXPECT_TRUE(SetLocalDescriptionAndSendSdpMessage(std::move(answer)));
  }

  void HandleIncomingAnswer(const std::string& msg) {
    RTC_LOG(LS_INFO) << debug_name_ << ": HandleIncomingAnswer";
    std::unique_ptr<SessionDescriptionInterface> desc =
        CreateSessionDescription(SdpType::kAnswer, msg);
    if (received_sdp_munger_) {
      received_sdp_munger_(desc->description());
    }

    EXPECT_TRUE(SetRemoteDescription(std::move(desc)));

    ResetRtpReceiverObservers();
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    auto observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    pc()->CreateAnswer(observer.get(), offer_answer_options_);
    return WaitForDescriptionFromObserver(observer.get());
  }

  std::unique_ptr<SessionDescriptionInterface> WaitForDescriptionFromObserver(
      MockCreateSessionDescriptionObserver* observer) {
    EXPECT_EQ_WAIT(true, observer->called(), kDefaultTimeout);
    if (!observer->result()) {
      return nullptr;
    }
    auto description = observer->MoveDescription();
    if (generated_sdp_munger_) {
      generated_sdp_munger_(description->description());
    }
    return description;
  }

  bool SetLocalDescriptionAndSendSdpMessage(
      std::unique_ptr<SessionDescriptionInterface> desc) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    RTC_LOG(LS_INFO) << debug_name_ << ": SetLocalDescriptionAndSendSdpMessage";
    SdpType type = desc->GetType();
    std::string sdp;
    EXPECT_TRUE(desc->ToString(&sdp));
    RTC_LOG(LS_INFO) << debug_name_ << ": local SDP contents=\n" << sdp;
    pc()->SetLocalDescription(observer.get(), desc.release());
    RemoveUnusedVideoRenderers();

    SendSdpMessage(type, sdp);
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return true;
  }

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc) {
    auto observer = rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
    RTC_LOG(LS_INFO) << debug_name_ << ": SetRemoteDescription";
    pc()->SetRemoteDescription(observer.get(), desc.release());
    RemoveUnusedVideoRenderers();
    EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
    return observer->result();
  }

  void RemoveUnusedVideoRenderers() {
    if (sdp_semantics_ != SdpSemantics::kUnifiedPlan) {
      return;
    }
    auto transceivers = pc()->GetTransceivers();
    std::set<std::string> active_renderers;
    for (auto& transceiver : transceivers) {

      if (transceiver->receiver()->media_type() == cricket::MEDIA_TYPE_VIDEO) {
        active_renderers.insert(transceiver->receiver()->track()->id());
      }
    }
    for (auto it = fake_video_renderers_.begin();
         it != fake_video_renderers_.end();) {

      if (!active_renderers.count(it->first)) {
        it = fake_video_renderers_.erase(it);
      } else {
        it++;
      }
    }
  }

  void SendSdpMessage(SdpType type, const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelaySdpMessageIfReceiverExists(type, msg);
    } else {
      rtc::Thread::Current()->PostDelayedTask(
          SafeTask(task_safety_.flag(),
                   [this, type, msg] {
                     RelaySdpMessageIfReceiverExists(type, msg);
                   }),
          TimeDelta::Millis(signaling_delay_ms_));
    }
  }

  void RelaySdpMessageIfReceiverExists(SdpType type, const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveSdpMessage(type, msg);
    }
  }

  void SendIceMessage(const std::string& sdp_mid,
                      int sdp_mline_index,
                      const std::string& msg) {
    if (signaling_delay_ms_ == 0) {
      RelayIceMessageIfReceiverExists(sdp_mid, sdp_mline_index, msg);
    } else {
      rtc::Thread::Current()->PostDelayedTask(
          SafeTask(task_safety_.flag(),
                   [this, sdp_mid, sdp_mline_index, msg] {
                     RelayIceMessageIfReceiverExists(sdp_mid, sdp_mline_index,
                                                     msg);
                   }),
          TimeDelta::Millis(signaling_delay_ms_));
    }
  }

  void RelayIceMessageIfReceiverExists(const std::string& sdp_mid,
                                       int sdp_mline_index,
                                       const std::string& msg) {
    if (signaling_message_receiver_) {
      signaling_message_receiver_->ReceiveIceMessage(sdp_mid, sdp_mline_index,
                                                     msg);
    }
  }

  void ReceiveSdpMessage(SdpType type, const std::string& msg) override {
    if (type == SdpType::kOffer) {
      HandleIncomingOffer(msg);
    } else {
      HandleIncomingAnswer(msg);
    }
  }

  void ReceiveIceMessage(const std::string& sdp_mid,
                         int sdp_mline_index,
                         const std::string& msg) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": ReceiveIceMessage";
    absl::optional<RTCError> result;
    pc()->AddIceCandidate(absl::WrapUnique(CreateIceCandidate(
                              sdp_mid, sdp_mline_index, msg, nullptr)),
                          [&result](RTCError r) { result = r; });
    EXPECT_TRUE_WAIT(result.has_value(), kDefaultTimeout);
    EXPECT_TRUE(result.value().ok());
  }

  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {
    EXPECT_EQ(pc()->signaling_state(), new_state);
    peer_connection_signaling_state_history_.push_back(new_state);
  }
  void OnAddTrack(rtc::scoped_refptr<RtpReceiverInterface> receiver,
                  const std::vector<rtc::scoped_refptr<MediaStreamInterface>>&
                      streams) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      rtc::scoped_refptr<VideoTrackInterface> video_track(
          static_cast<VideoTrackInterface*>(receiver->track().get()));
      ASSERT_TRUE(fake_video_renderers_.find(video_track->id()) ==
                  fake_video_renderers_.end());
      fake_video_renderers_[video_track->id()] =
          std::make_unique<FakeVideoTrackRenderer>(video_track.get());
    }
  }
  void OnRemoveTrack(
      rtc::scoped_refptr<RtpReceiverInterface> receiver) override {
    if (receiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      auto it = fake_video_renderers_.find(receiver->track()->id());
      if (it != fake_video_renderers_.end()) {
        fake_video_renderers_.erase(it);
      } else {
        RTC_LOG(LS_ERROR) << "OnRemoveTrack called for non-active renderer";
      }
    }
  }
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    EXPECT_EQ(pc()->ice_connection_state(), new_state);
    ice_connection_state_history_.push_back(new_state);
  }
  void OnStandardizedIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    standardized_ice_connection_state_history_.push_back(new_state);
  }
  void OnConnectionChange(
      PeerConnectionInterface::PeerConnectionState new_state) override {
    peer_connection_state_history_.push_back(new_state);
  }

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    EXPECT_EQ(pc()->ice_gathering_state(), new_state);
    ice_gathering_state_history_.push_back(new_state);
  }

  void OnIceSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event) {
    ice_candidate_pair_change_history_.push_back(event);
  }

  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnIceCandidate";

    if (remote_async_dns_resolver_) {
      const auto& local_candidate = candidate->candidate();
      if (local_candidate.address().IsUnresolvedIP()) {
        RTC_DCHECK(local_candidate.is_local());
        const auto resolved_ip = mdns_responder_->GetMappedAddressForName(
            local_candidate.address().hostname());
        RTC_DCHECK(!resolved_ip.IsNil());
        remote_async_dns_resolved_addr_ = local_candidate.address();
        remote_async_dns_resolved_addr_.SetResolvedIP(resolved_ip);
        EXPECT_CALL(*remote_async_dns_resolver_, Start(_, _))
            .WillOnce([](const rtc::SocketAddress& addr,
                         absl::AnyInvocable<void()> callback) { callback(); });
        EXPECT_CALL(*remote_async_dns_resolver_, result())
            .WillOnce(ReturnRef(remote_async_dns_resolver_result_));
        EXPECT_CALL(remote_async_dns_resolver_result_, GetResolvedAddress(_, _))
            .WillOnce(DoAll(SetArgPointee<1>(remote_async_dns_resolved_addr_),
                            Return(true)));
      }
    }

    EXPECT_GT(candidates_expected_, 1);
    candidates_expected_--;
    std::string ice_sdp;
    EXPECT_TRUE(candidate->ToString(&ice_sdp));
    if (signaling_message_receiver_ == nullptr || !signal_ice_candidates_) {

      return;
    }
    SendIceMessage(candidate->sdp_mid(), candidate->sdp_mline_index(), ice_sdp);
    last_candidate_gathered_ = candidate->candidate();
  }
  void OnIceCandidateError(const std::string& address,
                           int port,
                           const std::string& url,
                           int error_code,
                           const std::string& error_text) override {
    error_event_ = cricket::IceCandidateErrorEvent(address, port, url,
                                                   error_code, error_text);
  }
  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    RTC_LOG(LS_INFO) << debug_name_ << ": OnDataChannel";
    data_channels_.push_back(data_channel);
    data_observers_.push_back(
        std::make_unique<MockDataChannelObserver>(data_channel.get()));
  }

  std::string debug_name_;

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;
  std::unique_ptr<rtc::BasicPacketSocketFactory> socket_factory_;

  FakeMdnsResponder* mdns_responder_ = nullptr;

  rtc::scoped_refptr<PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> peer_connection_factory_;

  cricket::PortAllocator* port_allocator_;

  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;

  std::map<std::string, std::unique_ptr<FakeVideoTrackRenderer>>
      fake_video_renderers_;

  std::vector<std::unique_ptr<FakeVideoTrackRenderer>>
      removed_fake_video_renderers_;

  SignalingMessageReceiver* signaling_message_receiver_ = nullptr;
  int signaling_delay_ms_ = 0;
  bool signal_ice_candidates_ = true;
  cricket::Candidate last_candidate_gathered_;
  cricket::IceCandidateErrorEvent error_event_;

  std::vector<rtc::scoped_refptr<VideoTrackSource>> video_track_sources_;

  std::unique_ptr<FakeVideoTrackRenderer> local_video_renderer_;

  SdpSemantics sdp_semantics_;
  PeerConnectionInterface::RTCOfferAnswerOptions offer_answer_options_;
  std::function<void(cricket::SessionDescription*)> received_sdp_munger_;
  std::function<void(cricket::SessionDescription*)> generated_sdp_munger_;
  std::function<void()> remote_offer_handler_;
  MockAsyncDnsResolver* remote_async_dns_resolver_ = nullptr;

  NiceMock<MockAsyncDnsResolverResult> remote_async_dns_resolver_result_;
  rtc::SocketAddress remote_async_dns_resolved_addr_;

  std::vector<rtc::scoped_refptr<DataChannelInterface>> data_channels_;
  std::vector<std::unique_ptr<MockDataChannelObserver>> data_observers_;

  std::vector<std::unique_ptr<MockRtpReceiverObserver>> rtp_receiver_observers_;

  std::vector<PeerConnectionInterface::IceConnectionState>
      ice_connection_state_history_;
  std::vector<PeerConnectionInterface::IceConnectionState>
      standardized_ice_connection_state_history_;
  std::vector<PeerConnectionInterface::PeerConnectionState>
      peer_connection_state_history_;
  std::vector<PeerConnectionInterface::IceGatheringState>
      ice_gathering_state_history_;
  std::vector<cricket::CandidatePairChangeEvent>
      ice_candidate_pair_change_history_;
  std::vector<PeerConnectionInterface::SignalingState>
      peer_connection_signaling_state_history_;
  FakeRtcEventLogFactory* event_log_factory_;

  int candidates_expected_ = std::numeric_limits<int>::max();

  int audio_packets_stat_ = 0;
  double audio_delay_stat_ = 0.0;
  uint64_t audio_samples_stat_ = 0;
  uint64_t audio_concealed_stat_ = 0;
  std::string rtp_stats_id_;

  ScopedTaskSafety task_safety_;

  friend class PeerConnectionIntegrationBaseTest;
};

class MockRtcEventLogOutput : public RtcEventLogOutput {
 public:
  virtual ~MockRtcEventLogOutput() = default;
  MOCK_METHOD(bool, IsActive, (), (const, override));
  MOCK_METHOD(bool, Write, (absl::string_view), (override));
};

class MediaExpectations {
 public:
  enum ExpectFrames {
    kExpectSomeFrames,
    kExpectNoFrames,
    kNoExpectation,
  };

  void ExpectBidirectionalAudioAndVideo() {
    ExpectBidirectionalAudio();
    ExpectBidirectionalVideo();
  }

  void ExpectBidirectionalAudio() {
    CallerExpectsSomeAudio();
    CalleeExpectsSomeAudio();
  }

  void ExpectNoAudio() {
    CallerExpectsNoAudio();
    CalleeExpectsNoAudio();
  }

  void ExpectBidirectionalVideo() {
    CallerExpectsSomeVideo();
    CalleeExpectsSomeVideo();
  }

  void ExpectNoVideo() {
    CallerExpectsNoVideo();
    CalleeExpectsNoVideo();
  }

  void CallerExpectsSomeAudioAndVideo() {
    CallerExpectsSomeAudio();
    CallerExpectsSomeVideo();
  }

  void CalleeExpectsSomeAudioAndVideo() {
    CalleeExpectsSomeAudio();
    CalleeExpectsSomeVideo();
  }

  void CallerExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    caller_audio_expectation_ = kExpectSomeFrames;
    caller_audio_frames_expected_ = expected_audio_frames;
  }

  void CallerExpectsNoAudio() {
    caller_audio_expectation_ = kExpectNoFrames;
    caller_audio_frames_expected_ = 0;
  }

  void CallerExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    caller_video_expectation_ = kExpectSomeFrames;
    caller_video_frames_expected_ = expected_video_frames;
  }

  void CallerExpectsNoVideo() {
    caller_video_expectation_ = kExpectNoFrames;
    caller_video_frames_expected_ = 0;
  }

  void CalleeExpectsSomeAudio(
      int expected_audio_frames = kDefaultExpectedAudioFrameCount) {
    callee_audio_expectation_ = kExpectSomeFrames;
    callee_audio_frames_expected_ = expected_audio_frames;
  }

  void CalleeExpectsNoAudio() {
    callee_audio_expectation_ = kExpectNoFrames;
    callee_audio_frames_expected_ = 0;
  }

  void CalleeExpectsSomeVideo(
      int expected_video_frames = kDefaultExpectedVideoFrameCount) {
    callee_video_expectation_ = kExpectSomeFrames;
    callee_video_frames_expected_ = expected_video_frames;
  }

  void CalleeExpectsNoVideo() {
    callee_video_expectation_ = kExpectNoFrames;
    callee_video_frames_expected_ = 0;
  }

  ExpectFrames caller_audio_expectation_ = kNoExpectation;
  ExpectFrames caller_video_expectation_ = kNoExpectation;
  ExpectFrames callee_audio_expectation_ = kNoExpectation;
  ExpectFrames callee_video_expectation_ = kNoExpectation;
  int caller_audio_frames_expected_ = 0;
  int caller_video_frames_expected_ = 0;
  int callee_audio_frames_expected_ = 0;
  int callee_video_frames_expected_ = 0;
};

class MockIceTransport : public IceTransportInterface {
 public:
  MockIceTransport(const std::string& name, int component)
      : internal_(std::make_unique<cricket::FakeIceTransport>(
            name,
            component,
            nullptr                     )) {}
  ~MockIceTransport() = default;
  cricket::IceTransportInternal* internal() { return internal_.get(); }

 private:
  std::unique_ptr<cricket::FakeIceTransport> internal_;
};

class MockIceTransportFactory : public IceTransportFactory {
 public:
  ~MockIceTransportFactory() override = default;
  rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      IceTransportInit init) {
    RecordIceTransportCreated();
    return rtc::make_ref_counted<MockIceTransport>(transport_name, component);
  }
  MOCK_METHOD(void, RecordIceTransportCreated, ());
};

class PeerConnectionIntegrationBaseTest : public ::testing::Test {
 public:
  PeerConnectionIntegrationBaseTest(
      SdpSemantics sdp_semantics,
      absl::optional<std::string> field_trials = absl::nullopt)
      : sdp_semantics_(sdp_semantics),
        ss_(new rtc::VirtualSocketServer()),
        fss_(new rtc::FirewallSocketServer(ss_.get())),
        network_thread_(new rtc::Thread(fss_.get())),
        worker_thread_(rtc::Thread::Create()),

        field_trials_(new test::ScopedKeyValueConfig(
            field_trials.has_value() ? *field_trials : "")) {
    network_thread_->SetName("PCNetworkThread", this);
    worker_thread_->SetName("PCWorkerThread", this);
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());
    metrics::Reset();
  }

  ~PeerConnectionIntegrationBaseTest() {

    if (caller_) {
      caller_->set_signaling_message_receiver(nullptr);
      caller_->pc()->Close();
      delete SetCallerPcWrapperAndReturnCurrent(nullptr);
    }
    if (callee_) {
      callee_->set_signaling_message_receiver(nullptr);
      callee_->pc()->Close();
      delete SetCalleePcWrapperAndReturnCurrent(nullptr);
    }

    SendTask(network_thread(), [this] {
      turn_servers_.clear();
      turn_customizers_.clear();
    });
  }

  bool SignalingStateStable() {
    return caller_->SignalingStateStable() && callee_->SignalingStateStable();
  }

  bool DtlsConnected() {

    return (callee()->ice_connection_state() ==
                PeerConnectionInterface::kIceConnectionConnected ||
            callee()->ice_connection_state() ==
                PeerConnectionInterface::kIceConnectionCompleted) &&
           (caller()->ice_connection_state() ==
                PeerConnectionInterface::kIceConnectionConnected ||
            caller()->ice_connection_state() ==
                PeerConnectionInterface::kIceConnectionCompleted);
  }

  std::unique_ptr<PeerConnectionIntegrationWrapper> CreatePeerConnectionWrapper(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      PeerConnectionDependencies dependencies,
      std::unique_ptr<FakeRtcEventLogFactory> event_log_factory,
      bool reset_encoder_factory,
      bool reset_decoder_factory,
      bool create_media_engine = true) {
    RTCConfiguration modified_config;
    if (config) {
      modified_config = *config;
    }
    modified_config.sdp_semantics = sdp_semantics_;
    if (!dependencies.cert_generator) {
      dependencies.cert_generator =
          std::make_unique<FakeRTCCertificateGenerator>();
    }
    std::unique_ptr<PeerConnectionIntegrationWrapper> client(
        new PeerConnectionIntegrationWrapper(debug_name));

    if (!client->Init(options, &modified_config, std::move(dependencies),
                      fss_.get(), network_thread_.get(), worker_thread_.get(),
                      std::move(event_log_factory), reset_encoder_factory,
                      reset_decoder_factory, create_media_engine)) {
      return nullptr;
    }
    return client;
  }

  std::unique_ptr<PeerConnectionIntegrationWrapper>
  CreatePeerConnectionWrapperWithFakeRtcEventLog(
      const std::string& debug_name,
      const PeerConnectionFactory::Options* options,
      const RTCConfiguration* config,
      PeerConnectionDependencies dependencies) {
    return CreatePeerConnectionWrapper(
        debug_name, options, config, std::move(dependencies),
        std::make_unique<FakeRtcEventLogFactory>(),
                                  false,
                                  false);
  }

  bool CreatePeerConnectionWrappers() {
    return CreatePeerConnectionWrappersWithConfig(
        PeerConnectionInterface::RTCConfiguration(),
        PeerConnectionInterface::RTCConfiguration());
  }

  bool CreatePeerConnectionWrappersWithSdpSemantics(
      SdpSemantics caller_semantics,
      SdpSemantics callee_semantics) {

    SdpSemantics original_semantics = sdp_semantics_;
    sdp_semantics_ = caller_semantics;
    caller_ = CreatePeerConnectionWrapper("Caller", nullptr, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    sdp_semantics_ = callee_semantics;
    callee_ = CreatePeerConnectionWrapper("Callee", nullptr, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    sdp_semantics_ = original_semantics;
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfig(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      const PeerConnectionInterface::RTCConfiguration& callee_config) {
    caller_ = CreatePeerConnectionWrapper("Caller", nullptr, &caller_config,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    callee_ = CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithConfigAndDeps(
      const PeerConnectionInterface::RTCConfiguration& caller_config,
      PeerConnectionDependencies caller_dependencies,
      const PeerConnectionInterface::RTCConfiguration& callee_config,
      PeerConnectionDependencies callee_dependencies) {
    caller_ =
        CreatePeerConnectionWrapper("Caller", nullptr, &caller_config,
                                    std::move(caller_dependencies), nullptr,
                                                              false,
                                                              false);
    callee_ =
        CreatePeerConnectionWrapper("Callee", nullptr, &callee_config,
                                    std::move(callee_dependencies), nullptr,
                                                              false,
                                                              false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithOptions(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options) {
    caller_ = CreatePeerConnectionWrapper("Caller", &caller_options, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    callee_ = CreatePeerConnectionWrapper("Callee", &callee_options, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithFakeRtcEventLog() {
    PeerConnectionInterface::RTCConfiguration default_config;
    caller_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Caller", nullptr, &default_config,
        PeerConnectionDependencies(nullptr));
    callee_ = CreatePeerConnectionWrapperWithFakeRtcEventLog(
        "Callee", nullptr, &default_config,
        PeerConnectionDependencies(nullptr));
    return caller_ && callee_;
  }

  std::unique_ptr<PeerConnectionIntegrationWrapper>
  CreatePeerConnectionWrapperWithAlternateKey() {
    std::unique_ptr<FakeRTCCertificateGenerator> cert_generator(
        new FakeRTCCertificateGenerator());
    cert_generator->use_alternate_key();

    PeerConnectionDependencies dependencies(nullptr);
    dependencies.cert_generator = std::move(cert_generator);
    return CreatePeerConnectionWrapper("New Peer", nullptr, nullptr,
                                       std::move(dependencies), nullptr,
                                                                 false,
                                                                 false);
  }

  bool CreateOneDirectionalPeerConnectionWrappers(bool caller_to_callee) {
    caller_ = CreatePeerConnectionWrapper(
        "Caller", nullptr, nullptr, PeerConnectionDependencies(nullptr),
        nullptr,
                                  !caller_to_callee,
                                  caller_to_callee);
    callee_ = CreatePeerConnectionWrapper(
        "Callee", nullptr, nullptr, PeerConnectionDependencies(nullptr),
        nullptr,
                                  caller_to_callee,
                                  !caller_to_callee);
    return caller_ && callee_;
  }

  bool CreatePeerConnectionWrappersWithoutMediaEngine() {
    caller_ = CreatePeerConnectionWrapper("Caller", nullptr, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false,
                                                                  false);
    callee_ = CreatePeerConnectionWrapper("Callee", nullptr, nullptr,
                                          PeerConnectionDependencies(nullptr),
                                          nullptr,
                                                                    false,
                                                                    false,
                                                                  false);
    return caller_ && callee_;
  }

  cricket::TestTurnServer* CreateTurnServer(
      rtc::SocketAddress internal_address,
      rtc::SocketAddress external_address,
      cricket::ProtocolType type = cricket::ProtocolType::PROTO_UDP,
      const std::string& common_name = "test turn server") {
    rtc::Thread* thread = network_thread();
    rtc::SocketFactory* socket_factory = fss_.get();
    std::unique_ptr<cricket::TestTurnServer> turn_server;
    SendTask(network_thread(), [&] {
      turn_server = std::make_unique<cricket::TestTurnServer>(
          thread, socket_factory, internal_address, external_address, type,
                               true, common_name);
    });
    turn_servers_.push_back(std::move(turn_server));

    return turn_servers_.back().get();
  }

  cricket::TestTurnCustomizer* CreateTurnCustomizer() {
    std::unique_ptr<cricket::TestTurnCustomizer> turn_customizer;
    SendTask(network_thread(), [&] {
      turn_customizer = std::make_unique<cricket::TestTurnCustomizer>();
    });
    turn_customizers_.push_back(std::move(turn_customizer));

    return turn_customizers_.back().get();
  }

  void ExpectTurnCustomizerCountersIncremented(
      cricket::TestTurnCustomizer* turn_customizer) {
    SendTask(network_thread(), [turn_customizer] {
      EXPECT_GT(turn_customizer->allow_channel_data_cnt_, 0u);
      EXPECT_GT(turn_customizer->modify_cnt_, 0u);
    });
  }

  void ConnectFakeSignaling() {
    caller_->set_signaling_message_receiver(callee_.get());
    callee_->set_signaling_message_receiver(caller_.get());
  }

  void ConnectFakeSignalingForSdpOnly() {
    ConnectFakeSignaling();
    SetSignalIceCandidates(false);
  }

  void SetSignalingDelayMs(int delay_ms) {
    caller_->set_signaling_delay_ms(delay_ms);
    callee_->set_signaling_delay_ms(delay_ms);
  }

  void SetSignalIceCandidates(bool signal) {
    caller_->set_signal_ice_candidates(signal);
    callee_->set_signal_ice_candidates(signal);
  }

  void SendRtpDataWithRetries(DataChannelInterface* dc,
                              const std::string& data,
                              int retries) {
    for (int i = 0; i < retries; ++i) {
      dc->Send(DataBuffer(data));
    }
  }

  rtc::Thread* network_thread() { return network_thread_.get(); }

  rtc::VirtualSocketServer* virtual_socket_server() { return ss_.get(); }

  PeerConnectionIntegrationWrapper* caller() { return caller_.get(); }

  void DestroyPeerConnections() {
    if (caller_) {
      caller_->pc()->Close();
    }
    if (callee_) {
      callee_->pc()->Close();
    }
    caller_.reset();
    callee_.reset();
  }

  PeerConnectionIntegrationWrapper* SetCallerPcWrapperAndReturnCurrent(
      PeerConnectionIntegrationWrapper* wrapper) {
    PeerConnectionIntegrationWrapper* old = caller_.release();
    caller_.reset(wrapper);
    return old;
  }

  PeerConnectionIntegrationWrapper* callee() { return callee_.get(); }

  PeerConnectionIntegrationWrapper* SetCalleePcWrapperAndReturnCurrent(
      PeerConnectionIntegrationWrapper* wrapper) {
    PeerConnectionIntegrationWrapper* old = callee_.release();
    callee_.reset(wrapper);
    return old;
  }

  void SetPortAllocatorFlags(uint32_t caller_flags, uint32_t callee_flags) {
    SendTask(network_thread(), [this, caller_flags] {
      caller()->port_allocator()->set_flags(caller_flags);
    });
    SendTask(network_thread(), [this, callee_flags] {
      callee()->port_allocator()->set_flags(callee_flags);
    });
  }

  rtc::FirewallSocketServer* firewall() const { return fss_.get(); }

  bool ExpectNewFrames(const MediaExpectations& media_expectations) {

    caller()->RemoveUnusedVideoRenderers();
    callee()->RemoveUnusedVideoRenderers();

    int total_caller_audio_frames_expected = caller()->audio_frames_received();
    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_audio_frames_expected +=
          media_expectations.caller_audio_frames_expected_;
    }
    int total_caller_video_frames_expected =
        caller()->min_video_frames_received_per_track();
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_caller_video_frames_expected +=
          media_expectations.caller_video_frames_expected_;
    }
    int total_callee_audio_frames_expected = callee()->audio_frames_received();
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_audio_frames_expected +=
          media_expectations.callee_audio_frames_expected_;
    }
    int total_callee_video_frames_expected =
        callee()->min_video_frames_received_per_track();
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectSomeFrames) {
      total_callee_video_frames_expected +=
          media_expectations.callee_video_frames_expected_;
    }

    EXPECT_TRUE_WAIT(caller()->audio_frames_received() >=
                             total_caller_audio_frames_expected &&
                         caller()->min_video_frames_received_per_track() >=
                             total_caller_video_frames_expected &&
                         callee()->audio_frames_received() >=
                             total_callee_audio_frames_expected &&
                         callee()->min_video_frames_received_per_track() >=
                             total_callee_video_frames_expected,
                     kMaxWaitForFramesMs);
    bool expectations_correct =
        caller()->audio_frames_received() >=
            total_caller_audio_frames_expected &&
        caller()->min_video_frames_received_per_track() >=
            total_caller_video_frames_expected &&
        callee()->audio_frames_received() >=
            total_callee_audio_frames_expected &&
        callee()->min_video_frames_received_per_track() >=
            total_callee_video_frames_expected;

    EXPECT_GE(caller()->audio_frames_received(),
              total_caller_audio_frames_expected);
    EXPECT_GE(caller()->min_video_frames_received_per_track(),
              total_caller_video_frames_expected);
    EXPECT_GE(callee()->audio_frames_received(),
              total_callee_audio_frames_expected);
    EXPECT_GE(callee()->min_video_frames_received_per_track(),
              total_callee_video_frames_expected);

    if (media_expectations.caller_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->audio_frames_received(),
                total_caller_audio_frames_expected);
      if (caller()->audio_frames_received() !=
          total_caller_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.caller_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(caller()->min_video_frames_received_per_track(),
                total_caller_video_frames_expected);
      if (caller()->min_video_frames_received_per_track() !=
          total_caller_video_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_audio_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->audio_frames_received(),
                total_callee_audio_frames_expected);
      if (callee()->audio_frames_received() !=
          total_callee_audio_frames_expected) {
        expectations_correct = false;
      }
    }
    if (media_expectations.callee_video_expectation_ ==
        MediaExpectations::kExpectNoFrames) {
      EXPECT_EQ(callee()->min_video_frames_received_per_track(),
                total_callee_video_frames_expected);
      if (callee()->min_video_frames_received_per_track() !=
          total_callee_video_frames_expected) {
        expectations_correct = false;
      }
    }
    return expectations_correct;
  }

  void ClosePeerConnections() {
    if (caller())
      caller()->pc()->Close();
    if (callee())
      callee()->pc()->Close();
  }

  void TestNegotiatedCipherSuite(
      const PeerConnectionFactory::Options& caller_options,
      const PeerConnectionFactory::Options& callee_options,
      int expected_cipher_suite) {
    ASSERT_TRUE(CreatePeerConnectionWrappersWithOptions(caller_options,
                                                        callee_options));
    ConnectFakeSignaling();
    caller()->AddAudioVideoTracks();
    callee()->AddAudioVideoTracks();
    caller()->CreateAndSetAndSignalOffer();
    ASSERT_TRUE_WAIT(DtlsConnected(), kDefaultTimeout);
    EXPECT_EQ_WAIT(rtc::SrtpCryptoSuiteToName(expected_cipher_suite),
                   caller()->OldGetStats()->SrtpCipher(), kDefaultTimeout);
  }

  void TestGcmNegotiationUsesCipherSuite(bool local_gcm_enabled,
                                         bool remote_gcm_enabled,
                                         bool aes_ctr_enabled,
                                         int expected_cipher_suite) {
    PeerConnectionFactory::Options caller_options;
    caller_options.crypto_options.srtp.enable_gcm_crypto_suites =
        local_gcm_enabled;
    caller_options.crypto_options.srtp.enable_aes128_sha1_80_crypto_cipher =
        aes_ctr_enabled;
    PeerConnectionFactory::Options callee_options;
    callee_options.crypto_options.srtp.enable_gcm_crypto_suites =
        remote_gcm_enabled;
    callee_options.crypto_options.srtp.enable_aes128_sha1_80_crypto_cipher =
        aes_ctr_enabled;
    TestNegotiatedCipherSuite(caller_options, callee_options,
                              expected_cipher_suite);
  }

  const FieldTrialsView& trials() const { return *field_trials_.get(); }

 protected:
  SdpSemantics sdp_semantics_;

 private:
  rtc::AutoThread main_thread_;

  std::unique_ptr<rtc::VirtualSocketServer> ss_;
  std::unique_ptr<rtc::FirewallSocketServer> fss_;

  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;

  std::vector<std::unique_ptr<cricket::TestTurnServer>> turn_servers_;
  std::vector<std::unique_ptr<cricket::TestTurnCustomizer>> turn_customizers_;
  std::unique_ptr<PeerConnectionIntegrationWrapper> caller_;
  std::unique_ptr<PeerConnectionIntegrationWrapper> callee_;
  std::unique_ptr<FieldTrialsView> field_trials_;
};

}

#endif
