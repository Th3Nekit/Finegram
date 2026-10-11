/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jsep_transport_controller.h"

#include <map>
#include <string>
#include <utility>

#include "api/dtls_transport_interface.h"
#include "api/transport/enums.h"
#include "p2p/base/candidate_pair_interface.h"
#include "p2p/base/dtls_transport_factory.h"
#include "p2p/base/fake_dtls_transport.h"
#include "p2p/base/fake_ice_transport.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_info.h"
#include "rtc_base/fake_ssl_identity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

using cricket::Candidate;
using cricket::Candidates;
using cricket::FakeDtlsTransport;
using webrtc::SdpType;

static const int kTimeout = 100;
static const char kIceUfrag1[] = "u0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIceUfrag2[] = "u0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";
static const char kIceUfrag3[] = "u0003";
static const char kIcePwd3[] = "TESTICEPWD00000000000003";
static const char kIceUfrag4[] = "u0004";
static const char kIcePwd4[] = "TESTICEPWD00000000000004";
static const char kAudioMid1[] = "audio1";
static const char kAudioMid2[] = "audio2";
static const char kVideoMid1[] = "video1";
static const char kVideoMid2[] = "video2";
static const char kDataMid1[] = "data1";

namespace webrtc {

class FakeIceTransportFactory : public IceTransportFactory {
 public:
  ~FakeIceTransportFactory() override = default;
  rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      IceTransportInit init) override {
    return rtc::make_ref_counted<cricket::FakeIceTransportWrapper>(
        std::make_unique<cricket::FakeIceTransport>(transport_name, component));
  }
};

class FakeDtlsTransportFactory : public cricket::DtlsTransportFactory {
 public:
  std::unique_ptr<cricket::DtlsTransportInternal> CreateDtlsTransport(
      cricket::IceTransportInternal* ice,
      const CryptoOptions& crypto_options,
      rtc::SSLProtocolVersion max_version) override {
    return std::make_unique<FakeDtlsTransport>(
        static_cast<cricket::FakeIceTransport*>(ice));
  }
};

class JsepTransportControllerTest : public JsepTransportController::Observer,
                                    public ::testing::Test,
                                    public sigslot::has_slots<> {
 public:
  JsepTransportControllerTest() : signaling_thread_(rtc::Thread::Current()) {
    fake_ice_transport_factory_ = std::make_unique<FakeIceTransportFactory>();
    fake_dtls_transport_factory_ = std::make_unique<FakeDtlsTransportFactory>();
  }

  void CreateJsepTransportController(
      JsepTransportController::Config config,
      rtc::Thread* network_thread = rtc::Thread::Current(),
      cricket::PortAllocator* port_allocator = nullptr) {
    config.transport_observer = this;
    config.rtcp_handler = [](const rtc::CopyOnWriteBuffer& packet,
                             int64_t packet_time_us) {
      RTC_DCHECK_NOTREACHED();
    };
    config.ice_transport_factory = fake_ice_transport_factory_.get();
    config.dtls_transport_factory = fake_dtls_transport_factory_.get();
    config.on_dtls_handshake_error_ = [](rtc::SSLHandshakeError s) {};
    config.field_trials = &field_trials_;
    transport_controller_ = std::make_unique<JsepTransportController>(
        network_thread, port_allocator, nullptr                             ,
        std::move(config));
    SendTask(network_thread, [&] { ConnectTransportControllerSignals(); });
  }

  void ConnectTransportControllerSignals() {
    transport_controller_->SubscribeIceConnectionState(
        [this](cricket::IceConnectionState s) {
          JsepTransportControllerTest::OnConnectionState(s);
        });
    transport_controller_->SubscribeConnectionState(
        [this](PeerConnectionInterface::PeerConnectionState s) {
          JsepTransportControllerTest::OnCombinedConnectionState(s);
        });
    transport_controller_->SubscribeStandardizedIceConnectionState(
        [this](PeerConnectionInterface::IceConnectionState s) {
          JsepTransportControllerTest::OnStandardizedIceConnectionState(s);
        });
    transport_controller_->SubscribeIceGatheringState(
        [this](cricket::IceGatheringState s) {
          JsepTransportControllerTest::OnGatheringState(s);
        });
    transport_controller_->SubscribeIceCandidateGathered(
        [this](const std::string& transport,
               const std::vector<cricket::Candidate>& candidates) {
          JsepTransportControllerTest::OnCandidatesGathered(transport,
                                                            candidates);
        });
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithoutBundle() {
    auto description = std::make_unique<cricket::SessionDescription>();
    AddAudioSection(description.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                    cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                    nullptr);
    AddVideoSection(description.get(), kVideoMid1, kIceUfrag1, kIcePwd1,
                    cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                    nullptr);
    return description;
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithBundleGroup() {
    auto description = CreateSessionDescriptionWithoutBundle();
    cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
    bundle_group.AddContentName(kAudioMid1);
    bundle_group.AddContentName(kVideoMid1);
    description->AddGroup(bundle_group);

    return description;
  }

  std::unique_ptr<cricket::SessionDescription>
  CreateSessionDescriptionWithBundledData() {
    auto description = CreateSessionDescriptionWithoutBundle();
    AddDataSection(description.get(), kDataMid1,
                   cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                   cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                   nullptr);
    cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
    bundle_group.AddContentName(kAudioMid1);
    bundle_group.AddContentName(kVideoMid1);
    bundle_group.AddContentName(kDataMid1);
    description->AddGroup(bundle_group);
    return description;
  }

  void AddAudioSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const std::string& ufrag,
                       const std::string& pwd,
                       cricket::IceMode ice_mode,
                       cricket::ConnectionRole conn_role,
                       rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<cricket::AudioContentDescription> audio(
        new cricket::AudioContentDescription());

    audio->set_rtcp_mux(true);
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                         false, std::move(audio));
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddVideoSection(cricket::SessionDescription* description,
                       const std::string& mid,
                       const std::string& ufrag,
                       const std::string& pwd,
                       cricket::IceMode ice_mode,
                       cricket::ConnectionRole conn_role,
                       rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<cricket::VideoContentDescription> video(
        new cricket::VideoContentDescription());

    video->set_rtcp_mux(true);
    description->AddContent(mid, cricket::MediaProtocolType::kRtp,
                                         false, std::move(video));
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddDataSection(cricket::SessionDescription* description,
                      const std::string& mid,
                      cricket::MediaProtocolType protocol_type,
                      const std::string& ufrag,
                      const std::string& pwd,
                      cricket::IceMode ice_mode,
                      cricket::ConnectionRole conn_role,
                      rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    RTC_CHECK(protocol_type == cricket::MediaProtocolType::kSctp);
    std::unique_ptr<cricket::SctpDataContentDescription> data(
        new cricket::SctpDataContentDescription());
    data->set_rtcp_mux(true);
    description->AddContent(mid, protocol_type,
                                         false, std::move(data));
    AddTransportInfo(description, mid, ufrag, pwd, ice_mode, conn_role, cert);
  }

  void AddTransportInfo(cricket::SessionDescription* description,
                        const std::string& mid,
                        const std::string& ufrag,
                        const std::string& pwd,
                        cricket::IceMode ice_mode,
                        cricket::ConnectionRole conn_role,
                        rtc::scoped_refptr<rtc::RTCCertificate> cert) {
    std::unique_ptr<rtc::SSLFingerprint> fingerprint;
    if (cert) {
      fingerprint = rtc::SSLFingerprint::CreateFromCertificate(*cert);
    }

    cricket::TransportDescription transport_desc(std::vector<std::string>(),
                                                 ufrag, pwd, ice_mode,
                                                 conn_role, fingerprint.get());
    description->AddTransportInfo(cricket::TransportInfo(mid, transport_desc));
  }

  cricket::IceConfig CreateIceConfig(
      int receiving_timeout,
      cricket::ContinualGatheringPolicy continual_gathering_policy) {
    cricket::IceConfig config;
    config.receiving_timeout = receiving_timeout;
    config.continual_gathering_policy = continual_gathering_policy;
    return config;
  }

  Candidate CreateCandidate(const std::string& transport_name, int component) {
    Candidate c;
    c.set_transport_name(transport_name);
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(component);
    c.set_protocol(cricket::UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

  void CreateLocalDescriptionAndCompleteConnectionOnNetworkThread() {
    if (!network_thread_->IsCurrent()) {
      SendTask(network_thread_.get(), [&] {
        CreateLocalDescriptionAndCompleteConnectionOnNetworkThread();
      });
      return;
    }

    auto description = CreateSessionDescriptionWithBundleGroup();
    EXPECT_TRUE(
        transport_controller_
            ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
            .ok());

    transport_controller_->MaybeStartGathering();
    auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
        transport_controller_->GetDtlsTransport(kAudioMid1));
    auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
        transport_controller_->GetDtlsTransport(kVideoMid1));
    fake_audio_dtls->fake_ice_transport()->SignalCandidateGathered(
        fake_audio_dtls->fake_ice_transport(),
        CreateCandidate(kAudioMid1,               1));
    fake_video_dtls->fake_ice_transport()->SignalCandidateGathered(
        fake_video_dtls->fake_ice_transport(),
        CreateCandidate(kVideoMid1,               1));
    fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
    fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
    fake_audio_dtls->fake_ice_transport()->SetConnectionCount(2);
    fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
    fake_audio_dtls->SetReceiving(true);
    fake_video_dtls->SetReceiving(true);
    fake_audio_dtls->SetWritable(true);
    fake_video_dtls->SetWritable(true);
    fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
    fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  }

 protected:
  void OnConnectionState(cricket::IceConnectionState state) {
    ice_signaled_on_thread_ = rtc::Thread::Current();
    connection_state_ = state;
    ++connection_state_signal_count_;
  }

  void OnStandardizedIceConnectionState(
      PeerConnectionInterface::IceConnectionState state) {
    ice_signaled_on_thread_ = rtc::Thread::Current();
    ice_connection_state_ = state;
    ++ice_connection_state_signal_count_;
  }

  void OnCombinedConnectionState(
      PeerConnectionInterface::PeerConnectionState state) {
    RTC_LOG(LS_INFO) << "OnCombinedConnectionState: "
                     << static_cast<int>(state);
    ice_signaled_on_thread_ = rtc::Thread::Current();
    combined_connection_state_ = state;
    ++combined_connection_state_signal_count_;
  }

  void OnGatheringState(cricket::IceGatheringState state) {
    ice_signaled_on_thread_ = rtc::Thread::Current();
    gathering_state_ = state;
    ++gathering_state_signal_count_;
  }

  void OnCandidatesGathered(const std::string& transport_name,
                            const Candidates& candidates) {
    ice_signaled_on_thread_ = rtc::Thread::Current();
    candidates_[transport_name].insert(candidates_[transport_name].end(),
                                       candidates.begin(), candidates.end());
    ++candidates_signal_count_;
  }

  bool OnTransportChanged(
      const std::string& mid,
      RtpTransportInternal* rtp_transport,
      rtc::scoped_refptr<DtlsTransport> dtls_transport,
      DataChannelTransportInterface* data_channel_transport) override {
    changed_rtp_transport_by_mid_[mid] = rtp_transport;
    if (dtls_transport) {
      changed_dtls_transport_by_mid_[mid] = dtls_transport->internal();
    } else {
      changed_dtls_transport_by_mid_[mid] = nullptr;
    }
    return true;
  }

  rtc::AutoThread main_thread_;

  cricket::IceConnectionState connection_state_ =
      cricket::kIceConnectionConnecting;
  PeerConnectionInterface::IceConnectionState ice_connection_state_ =
      PeerConnectionInterface::kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState combined_connection_state_ =
      PeerConnectionInterface::PeerConnectionState::kNew;
  bool receiving_ = false;
  cricket::IceGatheringState gathering_state_ = cricket::kIceGatheringNew;

  std::map<std::string, Candidates> candidates_;

  int connection_state_signal_count_ = 0;
  int ice_connection_state_signal_count_ = 0;
  int combined_connection_state_signal_count_ = 0;
  int receiving_signal_count_ = 0;
  int gathering_state_signal_count_ = 0;
  int candidates_signal_count_ = 0;

  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<FakeIceTransportFactory> fake_ice_transport_factory_;
  std::unique_ptr<FakeDtlsTransportFactory> fake_dtls_transport_factory_;
  rtc::Thread* const signaling_thread_ = nullptr;
  rtc::Thread* ice_signaled_on_thread_ = nullptr;

  std::map<std::string, RtpTransportInternal*> changed_rtp_transport_by_mid_;
  std::map<std::string, cricket::DtlsTransportInternal*>
      changed_dtls_transport_by_mid_;

  std::unique_ptr<JsepTransportController> transport_controller_;
  test::ScopedKeyValueConfig field_trials_;
};

TEST_F(JsepTransportControllerTest, GetRtpTransport) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  auto audio_rtp_transport = transport_controller_->GetRtpTransport(kAudioMid1);
  auto video_rtp_transport = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_NE(nullptr, audio_rtp_transport);
  EXPECT_NE(nullptr, video_rtp_transport);
  EXPECT_NE(audio_rtp_transport, video_rtp_transport);

  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kAudioMid2));
}

TEST_F(JsepTransportControllerTest, GetDtlsTransport) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
  CreateJsepTransportController(std::move(config));
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetRtcpDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr,
            transport_controller_->LookupDtlsTransportByMid(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_NE(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid1));
  EXPECT_NE(nullptr,
            transport_controller_->LookupDtlsTransportByMid(kVideoMid1));

  EXPECT_NE(transport_controller_->LookupDtlsTransportByMid(kAudioMid1),
            transport_controller_->LookupDtlsTransportByMid(kVideoMid1));

  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kVideoMid2));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid2));
  EXPECT_EQ(nullptr,
            transport_controller_->LookupDtlsTransportByMid(kVideoMid2));

  auto dtls_transport =
      transport_controller_->LookupDtlsTransportByMid(kVideoMid1);
  DtlsTransport* my_transport =
      static_cast<DtlsTransport*>(dtls_transport.get());
  EXPECT_NE(nullptr, my_transport->internal());
  transport_controller_.reset();
  EXPECT_EQ(nullptr, my_transport->internal());
}

TEST_F(JsepTransportControllerTest, GetDtlsTransportWithRtcpMux) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(std::move(config));
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kAudioMid1));
  EXPECT_NE(nullptr, transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtcpDtlsTransport(kVideoMid1));
}

TEST_F(JsepTransportControllerTest, SetIceConfig) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  transport_controller_->SetIceConfig(
      CreateIceConfig(kTimeout, cricket::GATHER_CONTINUALLY));
  FakeDtlsTransport* fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());

  AddAudioSection(description.get(), kAudioMid2, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid2));
  ASSERT_NE(nullptr, fake_audio_dtls);
  EXPECT_EQ(kTimeout,
            fake_audio_dtls->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(fake_audio_dtls->fake_ice_transport()->gather_continually());
}

TEST_F(JsepTransportControllerTest, NeedIceRestart) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, description.get(),
                                         description.get())
                  .ok());

  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kVideoMid1));

  transport_controller_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kVideoMid1));

  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kVideoMid2));

  auto audio_transport_info = description->GetTransportInfoByName(kAudioMid1);
  audio_transport_info->description.ice_ufrag = kIceUfrag2;
  audio_transport_info->description.ice_pwd = kIcePwd2;
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, description.get(),
                                        description.get())
                  .ok());

  EXPECT_FALSE(transport_controller_->NeedsIceRestart(kAudioMid1));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart(kVideoMid1));
}

TEST_F(JsepTransportControllerTest, MaybeStartGathering) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  transport_controller_->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, AddRemoveRemoteCandidates) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  transport_controller_->SetLocalDescription(SdpType::kOffer, description.get(),
                                             nullptr);
  transport_controller_->SetRemoteDescription(
      SdpType::kAnswer, description.get(), description.get());
  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  ASSERT_NE(nullptr, fake_audio_dtls);
  Candidates candidates;
  candidates.push_back(
      CreateCandidate(kAudioMid1, cricket::ICE_CANDIDATE_COMPONENT_RTP));
  EXPECT_TRUE(
      transport_controller_->AddRemoteCandidates(kAudioMid1, candidates).ok());
  EXPECT_EQ(1U,
            fake_audio_dtls->fake_ice_transport()->remote_candidates().size());

  EXPECT_TRUE(transport_controller_->RemoveRemoteCandidates(candidates).ok());
  EXPECT_EQ(0U,
            fake_audio_dtls->fake_ice_transport()->remote_candidates().size());
}

TEST_F(JsepTransportControllerTest, SetAndGetLocalCertificate) {
  CreateJsepTransportController(JsepTransportController::Config());

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1 =
      rtc::RTCCertificate::Create(
          rtc::SSLIdentity::Create("session1", rtc::KT_DEFAULT));
  rtc::scoped_refptr<rtc::RTCCertificate> returned_certificate;

  auto description = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(description.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  certificate1);

  EXPECT_TRUE(transport_controller_->SetLocalCertificate(certificate1));

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  returned_certificate = transport_controller_->GetLocalCertificate(kAudioMid1);
  EXPECT_TRUE(returned_certificate);
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  EXPECT_EQ(nullptr, transport_controller_->GetLocalCertificate(kVideoMid1));

  rtc::scoped_refptr<rtc::RTCCertificate> certificate2 =
      rtc::RTCCertificate::Create(
          rtc::SSLIdentity::Create("session2", rtc::KT_DEFAULT));
  EXPECT_FALSE(transport_controller_->SetLocalCertificate(certificate2));
}

TEST_F(JsepTransportControllerTest, GetRemoteSSLCertChain) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  rtc::FakeSSLCertificate fake_certificate("fake_data");

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->SetRemoteSSLCertificate(&fake_certificate);
  std::unique_ptr<rtc::SSLCertChain> returned_cert_chain =
      transport_controller_->GetRemoteSSLCertChain(kAudioMid1);
  ASSERT_TRUE(returned_cert_chain);
  ASSERT_EQ(1u, returned_cert_chain->GetSize());
  EXPECT_EQ(fake_certificate.ToPEMString(),
            returned_cert_chain->Get(0).ToPEMString());

  EXPECT_FALSE(transport_controller_->GetRemoteSSLCertChain(kAudioMid2));
}

TEST_F(JsepTransportControllerTest, GetDtlsRole) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto offer_certificate = rtc::RTCCertificate::Create(
      rtc::SSLIdentity::Create("offer", rtc::KT_DEFAULT));
  auto answer_certificate = rtc::RTCCertificate::Create(
      rtc::SSLIdentity::Create("answer", rtc::KT_DEFAULT));
  transport_controller_->SetLocalCertificate(offer_certificate);

  auto offer_desc = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(offer_desc.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  offer_certificate);
  auto answer_desc = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(answer_desc.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  answer_certificate);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, offer_desc.get(), nullptr)
          .ok());

  absl::optional<rtc::SSLRole> role =
      transport_controller_->GetDtlsRole(kAudioMid1);

  EXPECT_FALSE(role);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, offer_desc.get(),
                                         answer_desc.get())
                  .ok());
  role = transport_controller_->GetDtlsRole(kAudioMid1);

  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);
}

TEST_F(JsepTransportControllerTest, GetStats) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  cricket::TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats(kAudioMid1, &stats));
  EXPECT_EQ(kAudioMid1, stats.transport_name);
  EXPECT_EQ(1u, stats.channel_stats.size());

  EXPECT_FALSE(transport_controller_->GetStats(kAudioMid2, &stats));
}

TEST_F(JsepTransportControllerTest, SignalConnectionStateFailed) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_ice = static_cast<cricket::FakeIceTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1)->ice_transport());
  fake_ice->SetCandidatesGatheringComplete();
  fake_ice->SetConnectionCount(1);

  fake_ice->SetConnectionCount(0);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest,
       SignalConnectionStateConnectedNoMediaTransport) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  fake_video_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_video_dtls->fake_ice_transport()->SetConnectionCount(0);
  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(2, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(2, combined_connection_state_signal_count_);

  fake_audio_dtls->SetDtlsState(DtlsTransportState::kConnected);
  fake_video_dtls->SetDtlsState(DtlsTransportState::kConnected);

  fake_video_dtls->fake_ice_transport()->SetConnectionCount(2);
  fake_video_dtls->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionConnected,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(3, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnected,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(3, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalConnectionStateComplete) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  fake_audio_dtls->fake_ice_transport()->SetTransportState(
      IceTransportState::kCompleted,
      cricket::IceTransportState::STATE_COMPLETED);
  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionChecking,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnecting,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);

  fake_video_dtls->fake_ice_transport()->SetTransportState(
      IceTransportState::kFailed, cricket::IceTransportState::STATE_FAILED);
  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();

  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionFailed,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(2, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kFailed,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(2, combined_connection_state_signal_count_);

  fake_audio_dtls->SetDtlsState(DtlsTransportState::kConnected);
  fake_video_dtls->SetDtlsState(DtlsTransportState::kConnected);

  fake_video_dtls->fake_ice_transport()->SetTransportState(
      IceTransportState::kCompleted,
      cricket::IceTransportState::STATE_COMPLETED);
  fake_video_dtls->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(3, connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionCompleted,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(3, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnected,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(3, combined_connection_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalIceGatheringStateGathering) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();

  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalIceGatheringStateComplete) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithoutBundle();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));

  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_video_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_video_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest,
       SignalingWhenLastIncompleteTransportDestroyed) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  auto fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_NE(fake_audio_dtls, fake_video_dtls);

  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  fake_audio_dtls->SetWritable(true);
  fake_audio_dtls->fake_ice_transport()->SetCandidatesGatheringComplete();
  fake_audio_dtls->fake_ice_transport()->SetConnectionCount(1);
  fake_audio_dtls->SetDtlsState(DtlsTransportState::kConnected);
  EXPECT_EQ(1, gathering_state_signal_count_);

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, description.get(),
                                         description.get())
                  .ok());

  fake_video_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kVideoMid1));
  EXPECT_EQ(fake_audio_dtls, fake_video_dtls);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(PeerConnectionInterface::kIceConnectionCompleted,
            ice_connection_state_);
  EXPECT_EQ(PeerConnectionInterface::PeerConnectionState::kConnected,
            combined_connection_state_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest,
       IceStatesReturnToNewWhenTransportsDiscarded) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(description.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, description.get(),
                                         description.get())
                  .ok());

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->fake_ice_transport()->MaybeStartGathering();
  fake_audio_dtls->fake_ice_transport()->SetTransportState(
      IceTransportState::kChecking,
      cricket::IceTransportState::STATE_CONNECTING);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionChecking,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(1, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnecting,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(1, combined_connection_state_signal_count_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  description->contents()[0].rejected = true;
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, description.get(),
                                         description.get())
                  .ok());
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionNew,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(2, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kNew,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(2, combined_connection_state_signal_count_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringNew, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  EXPECT_TRUE(transport_controller_->RollbackTransports().ok());
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionChecking,
                 ice_connection_state_, kTimeout);
  EXPECT_EQ(3, ice_connection_state_signal_count_);
  EXPECT_EQ_WAIT(PeerConnectionInterface::PeerConnectionState::kConnecting,
                 combined_connection_state_, kTimeout);
  EXPECT_EQ(3, combined_connection_state_signal_count_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(3, gathering_state_signal_count_);
}

TEST_F(JsepTransportControllerTest, SignalCandidatesGathered) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto description = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, description.get(), nullptr)
          .ok());
  transport_controller_->MaybeStartGathering();

  auto fake_audio_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  fake_audio_dtls->fake_ice_transport()->SignalCandidateGathered(
      fake_audio_dtls->fake_ice_transport(), CreateCandidate(kAudioMid1, 1));
  EXPECT_EQ_WAIT(1, candidates_signal_count_, kTimeout);
  EXPECT_EQ(1u, candidates_[kAudioMid1].size());
}

TEST_F(JsepTransportControllerTest, IceSignalingOccursOnNetworkThread) {
  network_thread_ = rtc::Thread::CreateWithSocketServer();
  network_thread_->Start();
  EXPECT_EQ(ice_signaled_on_thread_, nullptr);
  CreateJsepTransportController(JsepTransportController::Config(),
                                network_thread_.get(),
                                                   nullptr);
  CreateLocalDescriptionAndCompleteConnectionOnNetworkThread();

  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  EXPECT_EQ_WAIT(1u, candidates_[kAudioMid1].size(), kTimeout);
  EXPECT_EQ_WAIT(1u, candidates_[kVideoMid1].size(), kTimeout);
  EXPECT_EQ(2, candidates_signal_count_);

  EXPECT_EQ(ice_signaled_on_thread_, network_thread_.get());

  SendTask(network_thread_.get(), [&] { transport_controller_.reset(); });
}

TEST_F(JsepTransportControllerTest, IceRoleNotRedetermined) {
  JsepTransportController::Config config;
  config.redetermine_role_on_ice_restart = false;

  CreateJsepTransportController(std::move(config));

  auto remote_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  EXPECT_TRUE(
      transport_controller_
          ->SetRemoteDescription(SdpType::kOffer, nullptr, remote_offer.get())
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer.get(),
                                        remote_offer.get())
                  .ok());

  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED,
            fake_dtls->fake_ice_transport()->GetIceRole());

  auto restart_local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(restart_local_offer.get(), kAudioMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer,
                                        restart_local_offer.get(),
                                        remote_offer.get())
                  .ok());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED,
            fake_dtls->fake_ice_transport()->GetIceRole());
}

TEST_F(JsepTransportControllerTest, SetIceRoleWhenIceLiteInRemoteAnswer) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL,
            fake_dtls->fake_ice_transport()->remote_ice_mode());

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_LITE,
            fake_dtls->fake_ice_transport()->remote_ice_mode());
}

TEST_F(JsepTransportControllerTest,
       IceRoleIsControllingAfterIceRestartFromIceLiteEndpoint) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto remote_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  auto local_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  EXPECT_TRUE(
      transport_controller_
          ->SetRemoteDescription(SdpType::kOffer, nullptr, remote_offer.get())
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer.get(),
                                        remote_offer.get())
                  .ok());
  auto fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());

  auto remote_offer2 = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_offer2.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_LITE, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, local_answer.get(),
                                         remote_offer2.get())
                  .ok());
  auto local_answer2 = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_answer2.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kAnswer, local_answer2.get(),
                                        remote_offer2.get())
                  .ok());
  fake_dtls = static_cast<FakeDtlsTransport*>(
      transport_controller_->GetDtlsTransport(kAudioMid1));

  EXPECT_EQ(cricket::ICEROLE_CONTROLLING,
            fake_dtls->fake_ice_transport()->GetIceRole());
}

TEST_F(JsepTransportControllerTest, MultipleMediaSectionsOfSameTypeWithBundle) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kAudioMid2);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto transport1 = transport_controller_->GetRtpTransport(kAudioMid1);
  auto transport2 = transport_controller_->GetRtpTransport(kAudioMid2);
  auto transport3 = transport_controller_->GetRtpTransport(kVideoMid1);
  auto transport4 = transport_controller_->GetRtpTransport(kDataMid1);
  EXPECT_EQ(transport1, transport2);
  EXPECT_EQ(transport1, transport3);
  EXPECT_EQ(transport1, transport4);

  EXPECT_EQ(transport_controller_->LookupDtlsTransportByMid(kAudioMid1),
            transport_controller_->LookupDtlsTransportByMid(kVideoMid1));

  auto it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);

  auto it2 = changed_dtls_transport_by_mid_.find(kDataMid1);
  ASSERT_TRUE(it2 != changed_dtls_transport_by_mid_.end());
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroups) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Video[] = "2_video";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  bundle_group1.AddContentName(kMid2Video);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid3Audio);
  bundle_group2.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(bundle_group1);
  remote_answer->AddGroup(bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Video);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  auto mid4_transport = transport_controller_->GetRtpTransport(kMid4Video);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_EQ(mid3_transport, mid4_transport);
  EXPECT_NE(mid1_transport, mid3_transport);

  auto it = changed_rtp_transport_by_mid_.find(kMid1Audio);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(it->second, mid1_transport);

  it = changed_rtp_transport_by_mid_.find(kMid2Video);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(it->second, mid2_transport);

  it = changed_rtp_transport_by_mid_.find(kMid3Audio);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(it->second, mid3_transport);

  it = changed_rtp_transport_by_mid_.find(kMid4Video);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(it->second, mid4_transport);
}

TEST_F(JsepTransportControllerTest,
       MultipleBundleGroupsInOfferButOnlyASingleGroupInAnswer) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Video[] = "2_video";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  bundle_group1.AddContentName(kMid2Video);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid3Audio);
  bundle_group2.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  remote_answer->AddGroup(bundle_group1);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Video);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  auto mid4_transport = transport_controller_->GetRtpTransport(kMid4Video);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_NE(mid3_transport, mid4_transport);
  EXPECT_NE(mid1_transport, mid3_transport);
  EXPECT_NE(mid1_transport, mid4_transport);
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroupsIllegallyChangeGroup) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Video[] = "2_video";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup offer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group1.AddContentName(kMid1Audio);
  offer_bundle_group1.AddContentName(kMid2Video);
  cricket::ContentGroup offer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group2.AddContentName(kMid3Audio);
  offer_bundle_group2.AddContentName(kMid4Video);

  cricket::ContentGroup answer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group1.AddContentName(kMid1Audio);
  answer_bundle_group1.AddContentName(kMid4Video);
  cricket::ContentGroup answer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group2.AddContentName(kMid3Audio);
  answer_bundle_group2.AddContentName(kMid2Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(offer_bundle_group1);
  local_offer->AddGroup(offer_bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(answer_bundle_group1);
  remote_answer->AddGroup(answer_bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());

  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroupsInvalidSubsets) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Video[] = "2_video";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup offer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group1.AddContentName(kMid1Audio);
  offer_bundle_group1.AddContentName(kMid2Video);
  cricket::ContentGroup offer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group2.AddContentName(kMid3Audio);
  offer_bundle_group2.AddContentName(kMid4Video);

  cricket::ContentGroup answer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group1.AddContentName(kMid1Audio);
  cricket::ContentGroup answer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group2.AddContentName(kMid2Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(offer_bundle_group1);
  local_offer->AddGroup(offer_bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag4, kIcePwd4,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(answer_bundle_group1);
  remote_answer->AddGroup(answer_bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());

  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroupsInvalidOverlap) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Video[] = "2_video";
  static const char kMid3Audio[] = "3_audio";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup offer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group1.AddContentName(kMid1Audio);
  offer_bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup offer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group2.AddContentName(kMid2Video);
  offer_bundle_group2.AddContentName(kMid3Audio);

  auto offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(offer.get(), kMid2Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  offer->AddGroup(offer_bundle_group1);
  offer->AddGroup(offer_bundle_group2);

  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer, offer.get(), nullptr)
                   .ok());
  EXPECT_FALSE(
      transport_controller_
          ->SetRemoteDescription(SdpType::kOffer, offer.get(), offer.get())
          .ok());
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroupsUnbundleFirstMid) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";
  static const char kMid5Video[] = "5_video";
  static const char kMid6Video[] = "6_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup offer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group1.AddContentName(kMid1Audio);
  offer_bundle_group1.AddContentName(kMid2Audio);
  offer_bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup offer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group2.AddContentName(kMid4Video);
  offer_bundle_group2.AddContentName(kMid5Video);
  offer_bundle_group2.AddContentName(kMid6Video);

  cricket::ContentGroup answer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group1.AddContentName(kMid2Audio);
  answer_bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup answer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group2.AddContentName(kMid5Video);
  answer_bundle_group2.AddContentName(kMid6Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid6Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(offer_bundle_group1);
  local_offer->AddGroup(offer_bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid6Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(answer_bundle_group1);
  remote_answer->AddGroup(answer_bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  auto mid4_transport = transport_controller_->GetRtpTransport(kMid4Video);
  auto mid5_transport = transport_controller_->GetRtpTransport(kMid5Video);
  auto mid6_transport = transport_controller_->GetRtpTransport(kMid6Video);
  EXPECT_NE(mid1_transport, mid2_transport);
  EXPECT_EQ(mid2_transport, mid3_transport);
  EXPECT_NE(mid4_transport, mid5_transport);
  EXPECT_EQ(mid5_transport, mid6_transport);
  EXPECT_NE(mid1_transport, mid4_transport);
  EXPECT_NE(mid2_transport, mid5_transport);
}

TEST_F(JsepTransportControllerTest, MultipleBundleGroupsChangeFirstMid) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";
  static const char kMid5Video[] = "5_video";
  static const char kMid6Video[] = "6_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup offer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group1.AddContentName(kMid1Audio);
  offer_bundle_group1.AddContentName(kMid2Audio);
  offer_bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup offer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group2.AddContentName(kMid4Video);
  offer_bundle_group2.AddContentName(kMid5Video);
  offer_bundle_group2.AddContentName(kMid6Video);

  cricket::ContentGroup answer_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group1.AddContentName(kMid2Audio);
  answer_bundle_group1.AddContentName(kMid1Audio);
  answer_bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup answer_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group2.AddContentName(kMid5Video);
  answer_bundle_group2.AddContentName(kMid6Video);
  answer_bundle_group2.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid6Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(offer_bundle_group1);
  local_offer->AddGroup(offer_bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid3Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid6Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(answer_bundle_group1);
  remote_answer->AddGroup(answer_bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());
  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  auto mid4_transport = transport_controller_->GetRtpTransport(kMid4Video);
  auto mid5_transport = transport_controller_->GetRtpTransport(kMid5Video);
  auto mid6_transport = transport_controller_->GetRtpTransport(kMid6Video);
  EXPECT_NE(mid1_transport, mid4_transport);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_EQ(mid2_transport, mid3_transport);
  EXPECT_EQ(mid4_transport, mid5_transport);
  EXPECT_EQ(mid5_transport, mid6_transport);
}

TEST_F(JsepTransportControllerTest,
       MultipleBundleGroupsSectionsAddedInSubsequentOffer) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Audio[] = "3_audio";
  static const char kMid4Video[] = "4_video";
  static const char kMid5Video[] = "5_video";
  static const char kMid6Video[] = "6_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  bundle_group1.AddContentName(kMid2Audio);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid4Video);
  bundle_group2.AddContentName(kMid5Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(bundle_group1);
  remote_answer->AddGroup(bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  cricket::ContentGroup new_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid3Audio);
  cricket::ContentGroup new_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid6Video);

  auto subsequent_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(subsequent_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(subsequent_offer.get(), kMid3Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid5Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid6Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer->AddGroup(bundle_group1);
  subsequent_offer->AddGroup(bundle_group2);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, subsequent_offer.get(),
                                        remote_answer.get())
                  .ok());
  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  auto mid4_transport = transport_controller_->GetRtpTransport(kMid4Video);
  auto mid5_transport = transport_controller_->GetRtpTransport(kMid5Video);
  auto mid6_transport = transport_controller_->GetRtpTransport(kMid6Video);
  EXPECT_NE(mid1_transport, mid4_transport);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_EQ(mid2_transport, mid3_transport);
  EXPECT_EQ(mid4_transport, mid5_transport);
  EXPECT_EQ(mid5_transport, mid6_transport);
}

TEST_F(JsepTransportControllerTest,
       MultipleBundleGroupsCombinedInSubsequentOffer) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Video[] = "3_video";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  bundle_group1.AddContentName(kMid2Audio);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid3Video);
  bundle_group2.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(bundle_group1);
  remote_answer->AddGroup(bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  cricket::ContentGroup new_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group.AddContentName(kMid1Audio);
  new_bundle_group.AddContentName(kMid2Audio);
  new_bundle_group.AddContentName(kMid3Video);
  new_bundle_group.AddContentName(kMid4Video);

  auto subsequent_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(subsequent_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer->AddGroup(new_bundle_group);
  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer,
                                         subsequent_offer.get(),
                                         remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest,
       MultipleBundleGroupsSplitInSubsequentOffer) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Video[] = "3_video";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kMid1Audio);
  bundle_group.AddContentName(kMid2Audio);
  bundle_group.AddContentName(kMid3Video);
  bundle_group.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  cricket::ContentGroup new_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group1.AddContentName(kMid1Audio);
  new_bundle_group1.AddContentName(kMid2Audio);
  cricket::ContentGroup new_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group2.AddContentName(kMid3Video);
  new_bundle_group2.AddContentName(kMid4Video);

  auto subsequent_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(subsequent_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer->AddGroup(new_bundle_group1);
  subsequent_offer->AddGroup(new_bundle_group2);
  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer,
                                         subsequent_offer.get(),
                                         remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest,
       MultipleBundleGroupsShuffledInSubsequentOffer) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Video[] = "3_video";
  static const char kMid4Video[] = "4_video";

  CreateJsepTransportController(JsepTransportController::Config());

  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  bundle_group1.AddContentName(kMid2Audio);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid3Video);
  bundle_group2.AddContentName(kMid4Video);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(remote_answer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  remote_answer->AddGroup(bundle_group1);
  remote_answer->AddGroup(bundle_group2);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  cricket::ContentGroup new_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group1.AddContentName(kMid1Audio);
  new_bundle_group1.AddContentName(kMid3Video);
  cricket::ContentGroup new_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group2.AddContentName(kMid2Audio);
  new_bundle_group2.AddContentName(kMid4Video);

  auto subsequent_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(subsequent_offer.get(), kMid2Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid3Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer.get(), kMid4Video, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer->AddGroup(new_bundle_group1);
  subsequent_offer->AddGroup(new_bundle_group2);
  EXPECT_FALSE(transport_controller_
                   ->SetLocalDescription(SdpType::kOffer,
                                         subsequent_offer.get(),
                                         remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, BundleSubsetOfMediaSections) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid2, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto transport1 = transport_controller_->GetRtpTransport(kAudioMid1);
  auto transport2 = transport_controller_->GetRtpTransport(kAudioMid2);
  auto transport3 = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_NE(transport1, transport2);
  EXPECT_EQ(transport1, transport3);

  auto it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(transport1, it->second);
  it = changed_rtp_transport_by_mid_.find(kAudioMid2);
  EXPECT_TRUE(transport2 == it->second);
}

TEST_F(JsepTransportControllerTest, BundleOnDataSectionInSubsequentOffer) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);
  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag1, kIcePwd1,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);
  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());
  auto data_transport = transport_controller_->GetRtpTransport(kDataMid1);

  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  local_offer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  local_offer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get(),
                                        remote_answer.get())
                  .ok());
  remote_answer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto audio_transport = transport_controller_->GetRtpTransport(kAudioMid1);
  auto video_transport = transport_controller_->GetRtpTransport(kVideoMid1);
  EXPECT_EQ(data_transport, audio_transport);
  EXPECT_EQ(data_transport, video_transport);
}

TEST_F(JsepTransportControllerTest, VideoDataRejectedInAnswer) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);

  remote_answer->contents()[1].rejected = true;
  remote_answer->contents()[2].rejected = true;

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kDataMid1));

  auto it = changed_rtp_transport_by_mid_.find(kVideoMid1);
  ASSERT_TRUE(it != changed_rtp_transport_by_mid_.end());
  EXPECT_EQ(nullptr, it->second);
  auto it2 = changed_dtls_transport_by_mid_.find(kDataMid1);
  ASSERT_TRUE(it2 != changed_dtls_transport_by_mid_.end());
  EXPECT_EQ(nullptr, it2->second);
}

TEST_F(JsepTransportControllerTest, ChangeBundledMidNotSupported) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());
  EXPECT_EQ(transport_controller_->GetRtpTransport(kAudioMid1),
            transport_controller_->GetRtpTransport(kVideoMid1));

  EXPECT_TRUE(bundle_group.RemoveContentName(kAudioMid1));
  bundle_group.AddContentName(kAudioMid1);
  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get(),
                                        remote_answer.get())
                  .ok());

  remote_answer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  remote_answer->AddGroup(bundle_group);
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, RejectFirstContentInBundleGroup) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);
  bundle_group.AddContentName(kDataMid1);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddDataSection(local_offer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                 nullptr);

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddVideoSection(remote_answer.get(), kVideoMid1, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);
  AddDataSection(remote_answer.get(), kDataMid1,
                 cricket::MediaProtocolType::kSctp, kIceUfrag3, kIcePwd3,
                 cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                 nullptr);

  remote_answer->contents()[0].rejected = true;

  local_offer->AddGroup(bundle_group);
  remote_answer->AddGroup(bundle_group);

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());

  remote_answer->contents()[1].rejected = true;
  remote_answer->contents()[2].rejected = true;
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kAudioMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kVideoMid1));
  EXPECT_EQ(nullptr, transport_controller_->GetDtlsTransport(kDataMid1));
}

TEST_F(JsepTransportControllerTest, ApplyNonRtcpMuxOfferWhenMuxingRequired) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(std::move(config));
  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);

  local_offer->contents()[0].media_description()->set_rtcp_mux(false);

  EXPECT_FALSE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
}

TEST_F(JsepTransportControllerTest, ApplyNonRtcpMuxAnswerWhenMuxingRequired) {
  JsepTransportController::Config config;
  config.rtcp_mux_policy = PeerConnectionInterface::kRtcpMuxPolicyRequire;
  CreateJsepTransportController(std::move(config));
  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());

  auto remote_answer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(remote_answer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_PASSIVE,
                  nullptr);

  remote_answer->contents()[0].media_description()->set_rtcp_mux(false);
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest,
       AddContentToBundleGroupInAnswerNotSupported) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = CreateSessionDescriptionWithoutBundle();
  auto remote_answer = CreateSessionDescriptionWithoutBundle();

  cricket::ContentGroup offer_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  offer_bundle_group.AddContentName(kAudioMid1);
  local_offer->AddGroup(offer_bundle_group);

  cricket::ContentGroup answer_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  answer_bundle_group.AddContentName(kAudioMid1);
  answer_bundle_group.AddContentName(kVideoMid1);
  remote_answer->AddGroup(answer_bundle_group);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, RejectBundleGroupWithNonExistingMid) {
  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = CreateSessionDescriptionWithoutBundle();
  auto remote_answer = CreateSessionDescriptionWithoutBundle();

  cricket::ContentGroup invalid_bundle_group(cricket::GROUP_TYPE_BUNDLE);

  invalid_bundle_group.AddContentName(kDataMid1);
  local_offer->AddGroup(invalid_bundle_group);
  remote_answer->AddGroup(invalid_bundle_group);

  EXPECT_FALSE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          remote_answer.get())
                   .ok());
}

TEST_F(JsepTransportControllerTest, RemoveContentFromBundleGroup) {
  CreateJsepTransportController(JsepTransportController::Config());

  auto local_offer = CreateSessionDescriptionWithBundleGroup();
  auto remote_answer = CreateSessionDescriptionWithBundleGroup();
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_offer.get(),
                                        remote_answer.get())
                  .ok());
  auto new_answer = CreateSessionDescriptionWithoutBundle();
  cricket::ContentGroup new_bundle_group(cricket::GROUP_TYPE_BUNDLE);

  new_bundle_group.AddContentName(kAudioMid1);
  new_answer->AddGroup(new_bundle_group);

  EXPECT_FALSE(transport_controller_
                   ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                          new_answer.get())
                   .ok());

  auto video_content = new_answer->GetContentByName(kVideoMid1);
  ASSERT_TRUE(video_content);
  video_content->rejected = true;
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         new_answer.get())
                  .ok());
}

TEST_F(JsepTransportControllerTest, ChangeTaggedMediaSectionMaxBundle) {
  CreateJsepTransportController(JsepTransportController::Config());

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  local_offer->AddGroup(bundle_group);
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());

  std::unique_ptr<cricket::SessionDescription> remote_answer(
      local_offer->Clone());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  std::unique_ptr<cricket::SessionDescription> local_reoffer(
      local_offer->Clone());
  local_reoffer->contents()[0].rejected = true;
  AddVideoSection(local_reoffer.get(), kVideoMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_reoffer->RemoveGroupByName(cricket::GROUP_TYPE_BUNDLE);
  cricket::ContentGroup new_bundle_group(cricket::GROUP_TYPE_BUNDLE);
  new_bundle_group.AddContentName(kVideoMid1);
  local_reoffer->AddGroup(new_bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_reoffer.get(),
                                        remote_answer.get())
                  .ok());
  std::unique_ptr<cricket::SessionDescription> remote_reanswer(
      local_reoffer->Clone());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_reoffer.get(),
                                         remote_reanswer.get())
                  .ok());
}

TEST_F(JsepTransportControllerTest, RollbackRestoresRejectedTransport) {
  static const char kMid1Audio[] = "1_audio";

  CreateJsepTransportController(JsepTransportController::Config());
  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  std::unique_ptr<cricket::SessionDescription> remote_answer(
      local_offer->Clone());
  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);

  auto local_reoffer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_reoffer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_reoffer->contents()[0].rejected = true;

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_reoffer.get(),
                                        remote_answer.get())
                  .ok());
  auto old_mid1_transport = mid1_transport;
  mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  EXPECT_EQ(nullptr, mid1_transport);

  EXPECT_TRUE(transport_controller_->RollbackTransports().ok());
  mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  EXPECT_EQ(old_mid1_transport, mid1_transport);
}

TEST_F(JsepTransportControllerTest, RollbackRestoresPreviousTransportMapping) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Audio[] = "3_audio";

  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kMid1Audio);
  bundle_group.AddContentName(kMid2Audio);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Audio, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_offer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group);

  std::unique_ptr<cricket::SessionDescription> remote_answer(
      local_offer->Clone());

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_NE(mid1_transport, mid3_transport);

  bundle_group.AddContentName(kMid3Audio);
  auto local_reoffer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_reoffer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_reoffer.get(), kMid2Audio, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddAudioSection(local_reoffer.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_reoffer->AddGroup(bundle_group);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer, local_reoffer.get(),
                                        remote_answer.get())
                  .ok());

  auto old_mid3_transport = mid3_transport;
  mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  EXPECT_EQ(mid1_transport, mid2_transport);
  EXPECT_EQ(mid1_transport, mid3_transport);

  EXPECT_TRUE(transport_controller_->RollbackTransports().ok());
  mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  EXPECT_EQ(old_mid3_transport, mid3_transport);
}

TEST_F(JsepTransportControllerTest, RollbackAndAddToDifferentBundleGroup) {
  static const char kMid1Audio[] = "1_audio";
  static const char kMid2Audio[] = "2_audio";
  static const char kMid3Audio[] = "3_audio";

  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName(kMid1Audio);
  cricket::ContentGroup bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName(kMid2Audio);

  auto local_offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(local_offer.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(local_offer.get(), kMid2Audio, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  local_offer->AddGroup(bundle_group1);
  local_offer->AddGroup(bundle_group2);

  std::unique_ptr<cricket::SessionDescription> remote_answer(
      local_offer->Clone());

  EXPECT_TRUE(
      transport_controller_
          ->SetLocalDescription(SdpType::kOffer, local_offer.get(), nullptr)
          .ok());
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kAnswer, local_offer.get(),
                                         remote_answer.get())
                  .ok());

  cricket::ContentGroup modified_bundle_group1(cricket::GROUP_TYPE_BUNDLE);
  modified_bundle_group1.AddContentName(kMid1Audio);
  modified_bundle_group1.AddContentName(kMid3Audio);
  auto subsequent_offer_1 = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer_1.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer_1.get(), kMid2Audio, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer_1.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer_1->AddGroup(modified_bundle_group1);
  subsequent_offer_1->AddGroup(bundle_group2);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer,
                                        subsequent_offer_1.get(),
                                        remote_answer.get())
                  .ok());

  auto mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  auto mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  auto mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  EXPECT_NE(mid1_transport, mid2_transport);
  EXPECT_EQ(mid1_transport, mid3_transport);

  EXPECT_TRUE(transport_controller_->RollbackTransports().ok());
  EXPECT_EQ(nullptr, transport_controller_->GetRtpTransport(kMid3Audio));

  cricket::ContentGroup modified_bundle_group2(cricket::GROUP_TYPE_BUNDLE);
  modified_bundle_group2.AddContentName(kMid2Audio);
  modified_bundle_group2.AddContentName(kMid3Audio);
  auto subsequent_offer_2 = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(subsequent_offer_2.get(), kMid1Audio, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer_2.get(), kMid2Audio, kIceUfrag2, kIcePwd2,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(subsequent_offer_2.get(), kMid3Audio, kIceUfrag3, kIcePwd3,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  subsequent_offer_2->AddGroup(bundle_group1);
  subsequent_offer_2->AddGroup(modified_bundle_group2);

  EXPECT_TRUE(transport_controller_
                  ->SetLocalDescription(SdpType::kOffer,
                                        subsequent_offer_2.get(),
                                        remote_answer.get())
                  .ok());

  mid1_transport = transport_controller_->GetRtpTransport(kMid1Audio);
  mid2_transport = transport_controller_->GetRtpTransport(kMid2Audio);
  mid3_transport = transport_controller_->GetRtpTransport(kMid3Audio);
  EXPECT_NE(mid1_transport, mid2_transport);
  EXPECT_EQ(mid2_transport, mid3_transport);
}

TEST_F(JsepTransportControllerTest, BundleOnlySectionDoesNotNeedRtcpMux) {
  CreateJsepTransportController(JsepTransportController::Config());
  cricket::ContentGroup bundle_group(cricket::GROUP_TYPE_BUNDLE);
  bundle_group.AddContentName(kAudioMid1);
  bundle_group.AddContentName(kVideoMid1);

  auto offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  AddVideoSection(offer.get(), kVideoMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  offer->AddGroup(bundle_group);

  offer->contents()[1].media_description()->set_rtcp_mux(false);
  offer->contents()[1].bundle_only = true;

  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, nullptr, offer.get())
                  .ok());
}

TEST_F(JsepTransportControllerTest,
       MaxBundleDoesNotRequireBundleForFirstMline) {
  auto config = JsepTransportController::Config();
  config.bundle_policy = PeerConnectionInterface::kBundlePolicyMaxBundle;
  CreateJsepTransportController(std::move(config));

  auto offer = std::make_unique<cricket::SessionDescription>();
  AddAudioSection(offer.get(), kAudioMid1, kIceUfrag1, kIcePwd1,
                  cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_ACTPASS,
                  nullptr);
  EXPECT_TRUE(transport_controller_
                  ->SetRemoteDescription(SdpType::kOffer, nullptr, offer.get())
                  .ok());
}

}
