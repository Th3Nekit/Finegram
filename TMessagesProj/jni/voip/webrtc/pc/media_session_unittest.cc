/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/media_session.h"

#include <stddef.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/rtp_parameters.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/test_utils.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/media_protocol_names.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_parameters_conversion.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_ssl_identity.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/unique_id_generator.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace cricket {
namespace {

using ::rtc::kCsAeadAes128Gcm;
using ::rtc::kCsAeadAes256Gcm;
using ::rtc::kCsAesCm128HmacSha1_32;
using ::rtc::kCsAesCm128HmacSha1_80;
using ::rtc::UniqueRandomIdGenerator;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Pointwise;
using ::testing::SizeIs;
using ::testing::Values;
using ::testing::ValuesIn;
using ::webrtc::RtpExtension;
using ::webrtc::RtpTransceiverDirection;

using Candidates = std::vector<Candidate>;

AudioCodec CreateRedAudioCodec(absl::string_view encoding_id) {
  AudioCodec red = CreateAudioCodec(63, "red", 48000, 2);
  red.SetParam(kCodecParamNotInNameValueFormat,
               std::string(encoding_id) + '/' + std::string(encoding_id));
  return red;
}

const AudioCodec kAudioCodecs1[] = {CreateAudioCodec(111, "opus", 48000, 2),
                                    CreateRedAudioCodec("111"),
                                    CreateAudioCodec(102, "iLBC", 8000, 1),
                                    CreateAudioCodec(0, "PCMU", 8000, 1),
                                    CreateAudioCodec(8, "PCMA", 8000, 1),
                                    CreateAudioCodec(117, "red", 8000, 1),
                                    CreateAudioCodec(107, "CN", 48000, 1)};

const AudioCodec kAudioCodecs2[] = {
    CreateAudioCodec(126, "foo", 16000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
    CreateAudioCodec(127, "iLBC", 8000, 1),
};

const AudioCodec kAudioCodecsAnswer[] = {
    CreateAudioCodec(102, "iLBC", 8000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
};

const VideoCodec kVideoCodecs1[] = {CreateVideoCodec(96, "H264-SVC"),
                                    CreateVideoCodec(97, "H264")};

const VideoCodec kVideoCodecs1Reverse[] = {CreateVideoCodec(97, "H264"),
                                           CreateVideoCodec(96, "H264-SVC")};

const VideoCodec kVideoCodecs2[] = {CreateVideoCodec(126, "H264"),
                                    CreateVideoCodec(127, "H263")};

const VideoCodec kVideoCodecsAnswer[] = {CreateVideoCodec(97, "H264")};

const RtpExtension kAudioRtpExtension1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
    RtpExtension("http://google.com/testing/audio_something", 10),
};

const RtpExtension kAudioRtpExtensionEncrypted1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
    RtpExtension("http://google.com/testing/audio_something", 10),
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 12, true),
    RtpExtension("http://google.com/testing/audio_something", 11, true),
};

const RtpExtension kAudioRtpExtension2[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 2),
    RtpExtension("http://google.com/testing/audio_something_else", 8),
    RtpExtension("http://google.com/testing/both_audio_and_video", 7),
};

const RtpExtension kAudioRtpExtension3[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),
    RtpExtension("http://google.com/testing/both_audio_and_video", 3),
};

const RtpExtension kAudioRtpExtension3ForEncryption[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),

    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 3),
};

const RtpExtension kAudioRtpExtension3ForEncryptionOffer[] = {
    RtpExtension("http://google.com/testing/audio_something", 2),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 3),
    RtpExtension("http://google.com/testing/audio_something", 14, true),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 13, true),
};

const RtpExtension kVideoRtpExtension3ForEncryptionOffer[] = {
    RtpExtension("http://google.com/testing/video_something", 4),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 3),
    RtpExtension("http://google.com/testing/video_something", 12, true),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 13, true),
};

const RtpExtension kAudioRtpExtensionAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8),
};

const RtpExtension kAudioRtpExtensionEncryptedAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level", 12, true),
};

const RtpExtension kVideoRtpExtension1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
    RtpExtension("http://google.com/testing/video_something", 13),
};

const RtpExtension kVideoRtpExtensionEncrypted1[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
    RtpExtension("http://google.com/testing/video_something", 13),
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 9, true),
    RtpExtension("http://google.com/testing/video_something", 7, true),
};

const RtpExtension kVideoRtpExtension2[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 2),
    RtpExtension("http://google.com/testing/video_something_else", 14),
    RtpExtension("http://google.com/testing/both_audio_and_video", 7),
};

const RtpExtension kVideoRtpExtension3[] = {
    RtpExtension("http://google.com/testing/video_something", 4),
    RtpExtension("http://google.com/testing/both_audio_and_video", 5),
};

const RtpExtension kVideoRtpExtension3ForEncryption[] = {
    RtpExtension("http://google.com/testing/video_something", 4),

    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 5),
};

const RtpExtension kVideoRtpExtensionAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 14),
};

const RtpExtension kVideoRtpExtensionEncryptedAnswer[] = {
    RtpExtension("urn:ietf:params:rtp-hdrext:toffset", 9, true),
};

const RtpExtension kRtpExtensionTransportSequenceNumber01[] = {
    RtpExtension("http://www.ietf.org/id/"
                 "draft-holmer-rmcat-transport-wide-cc-extensions-01",
                 1),
};

const RtpExtension kRtpExtensionTransportSequenceNumber01And02[] = {
    RtpExtension("http://www.ietf.org/id/"
                 "draft-holmer-rmcat-transport-wide-cc-extensions-01",
                 1),
    RtpExtension(
        "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02",
        2),
};

const RtpExtension kRtpExtensionTransportSequenceNumber02[] = {
    RtpExtension(
        "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02",
        2),
};

const RtpExtension kRtpExtensionGenericFrameDescriptorUri00[] = {
    RtpExtension("http://www.webrtc.org/experiments/rtp-hdrext/"
                 "generic-frame-descriptor-00",
                 3),
};

const uint32_t kSimulcastParamsSsrc[] = {10, 11, 20, 21, 30, 31};
const uint32_t kSimSsrc[] = {10, 20, 30};
const uint32_t kFec1Ssrc[] = {10, 11};
const uint32_t kFec2Ssrc[] = {20, 21};
const uint32_t kFec3Ssrc[] = {30, 31};

const char kMediaStream1[] = "stream_1";
const char kMediaStream2[] = "stream_2";
const char kVideoTrack1[] = "video_1";
const char kVideoTrack2[] = "video_2";
const char kAudioTrack1[] = "audio_1";
const char kAudioTrack2[] = "audio_2";
const char kAudioTrack3[] = "audio_3";

const char* kMediaProtocols[] = {"RTP/AVP", "RTP/SAVP", "RTP/AVPF",
                                 "RTP/SAVPF"};
const char* kMediaProtocolsDtls[] = {"TCP/TLS/RTP/SAVPF", "TCP/TLS/RTP/SAVP",
                                     "UDP/TLS/RTP/SAVPF", "UDP/TLS/RTP/SAVP"};

constexpr bool kStopped = true;
constexpr bool kActive = false;

bool IsMediaContentOfType(const ContentInfo* content, MediaType media_type) {
  RTC_DCHECK(content);
  return content->media_description()->type() == media_type;
}

RtpTransceiverDirection GetMediaDirection(const ContentInfo* content) {
  RTC_DCHECK(content);
  return content->media_description()->direction();
}

void AddRtxCodec(const VideoCodec& rtx_codec, std::vector<VideoCodec>* codecs) {
  ASSERT_FALSE(FindCodecById(*codecs, rtx_codec.id));
  codecs->push_back(rtx_codec);
}

std::vector<std::string> GetCodecNames(const std::vector<Codec>& codecs) {
  std::vector<std::string> codec_names;
  codec_names.reserve(codecs.size());
  for (const auto& codec : codecs) {
    codec_names.push_back(codec.name);
  }
  return codec_names;
}

std::vector<MediaDescriptionOptions>::iterator FindFirstMediaDescriptionByMid(
    const std::string& mid,
    MediaSessionOptions* opts) {
  return absl::c_find_if(
      opts->media_description_options,
      [&mid](const MediaDescriptionOptions& t) { return t.mid == mid; });
}

std::vector<MediaDescriptionOptions>::const_iterator
FindFirstMediaDescriptionByMid(const std::string& mid,
                               const MediaSessionOptions& opts) {
  return absl::c_find_if(
      opts.media_description_options,
      [&mid](const MediaDescriptionOptions& t) { return t.mid == mid; });
}

void AddMediaDescriptionOptions(MediaType type,
                                const std::string& mid,
                                RtpTransceiverDirection direction,
                                bool stopped,
                                MediaSessionOptions* opts) {
  opts->media_description_options.push_back(
      MediaDescriptionOptions(type, mid, direction, stopped));
}

void AddAudioVideoSections(RtpTransceiverDirection direction,
                           MediaSessionOptions* opts) {
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio", direction, kActive,
                             opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video", direction, kActive,
                             opts);
}

void AddDataSection(RtpTransceiverDirection direction,
                    MediaSessionOptions* opts) {
  AddMediaDescriptionOptions(MEDIA_TYPE_DATA, "data", direction, kActive, opts);
}

void AttachSenderToMediaDescriptionOptions(
    const std::string& mid,
    MediaType type,
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    const std::vector<RidDescription>& rids,
    const SimulcastLayerList& simulcast_layers,
    int num_sim_layer,
    MediaSessionOptions* session_options) {
  auto it = FindFirstMediaDescriptionByMid(mid, session_options);
  switch (type) {
    case MEDIA_TYPE_AUDIO:
      it->AddAudioSender(track_id, stream_ids);
      break;
    case MEDIA_TYPE_VIDEO:
      it->AddVideoSender(track_id, stream_ids, rids, simulcast_layers,
                         num_sim_layer);
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }
}

void AttachSenderToMediaDescriptionOptions(
    const std::string& mid,
    MediaType type,
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    int num_sim_layer,
    MediaSessionOptions* session_options) {
  AttachSenderToMediaDescriptionOptions(mid, type, track_id, stream_ids, {},
                                        SimulcastLayerList(), num_sim_layer,
                                        session_options);
}

void DetachSenderFromMediaSection(const std::string& mid,
                                  const std::string& track_id,
                                  MediaSessionOptions* session_options) {
  std::vector<SenderOptions>& sender_options_list =
      FindFirstMediaDescriptionByMid(mid, session_options)->sender_options;
  auto sender_it = absl::c_find_if(
      sender_options_list, [track_id](const SenderOptions& sender_options) {
        return sender_options.track_id == track_id;
      });
  RTC_DCHECK(sender_it != sender_options_list.end());
  sender_options_list.erase(sender_it);
}

MediaSessionOptions CreateAudioMediaSession() {
  MediaSessionOptions session_options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &session_options);
  return session_options;
}

class MediaSessionDescriptionFactoryTest : public testing::Test {
 public:
  MediaSessionDescriptionFactoryTest()
      : tdf1_(field_trials),
        tdf2_(field_trials),
        f1_(nullptr, false, &ssrc_generator1, &tdf1_),
        f2_(nullptr, false, &ssrc_generator2, &tdf2_) {
    f1_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs1),
                         MAKE_VECTOR(kAudioCodecs1));
    f1_.set_video_codecs(MAKE_VECTOR(kVideoCodecs1),
                         MAKE_VECTOR(kVideoCodecs1));
    f2_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs2),
                         MAKE_VECTOR(kAudioCodecs2));
    f2_.set_video_codecs(MAKE_VECTOR(kVideoCodecs2),
                         MAKE_VECTOR(kVideoCodecs2));
    tdf1_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id1"))));
    tdf2_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id2"))));
  }

  StreamParamsVec CreateComplexVideoStreamParamsVec() {
    SsrcGroup sim_group("SIM", MAKE_VECTOR(kSimSsrc));
    SsrcGroup fec_group1("FEC", MAKE_VECTOR(kFec1Ssrc));
    SsrcGroup fec_group2("FEC", MAKE_VECTOR(kFec2Ssrc));
    SsrcGroup fec_group3("FEC", MAKE_VECTOR(kFec3Ssrc));

    std::vector<SsrcGroup> ssrc_groups;
    ssrc_groups.push_back(sim_group);
    ssrc_groups.push_back(fec_group1);
    ssrc_groups.push_back(fec_group2);
    ssrc_groups.push_back(fec_group3);

    StreamParams simulcast_params;
    simulcast_params.id = kVideoTrack1;
    simulcast_params.ssrcs = MAKE_VECTOR(kSimulcastParamsSsrc);
    simulcast_params.ssrc_groups = ssrc_groups;
    simulcast_params.cname = "Video_SIM_FEC";
    simulcast_params.set_stream_ids({kMediaStream1});

    StreamParamsVec video_streams;
    video_streams.push_back(simulcast_params);

    return video_streams;
  }

  bool GetIceRenomination(const TransportInfo* transport_info) {
    return absl::c_linear_search(transport_info->description.transport_options,
                                 "renomination");
  }

  void TestTransportInfo(bool offer,
                         const MediaSessionOptions& options,
                         bool has_current_desc) {
    const std::string current_audio_ufrag = "current_audio_ufrag";
    const std::string current_audio_pwd = "current_audio_pwd";
    const std::string current_video_ufrag = "current_video_ufrag";
    const std::string current_video_pwd = "current_video_pwd";
    const std::string current_data_ufrag = "current_data_ufrag";
    const std::string current_data_pwd = "current_data_pwd";
    std::unique_ptr<SessionDescription> current_desc;
    std::unique_ptr<SessionDescription> desc;
    if (has_current_desc) {
      current_desc = std::make_unique<SessionDescription>();
      current_desc->AddTransportInfo(TransportInfo(
          "audio",
          TransportDescription(current_audio_ufrag, current_audio_pwd)));
      current_desc->AddTransportInfo(TransportInfo(
          "video",
          TransportDescription(current_video_ufrag, current_video_pwd)));
      current_desc->AddTransportInfo(TransportInfo(
          "data", TransportDescription(current_data_ufrag, current_data_pwd)));
    }
    if (offer) {
      desc = f1_.CreateOfferOrError(options, current_desc.get()).MoveValue();
    } else {
      std::unique_ptr<SessionDescription> offer;
      offer = f1_.CreateOfferOrError(options, nullptr).MoveValue();
      desc = f1_.CreateAnswerOrError(offer.get(), options, current_desc.get())
                 .MoveValue();
    }
    ASSERT_TRUE(desc);
    const TransportInfo* ti_audio = desc->GetTransportInfoByName("audio");
    if (options.has_audio()) {
      if (has_current_desc) {
        EXPECT_EQ(current_audio_ufrag, ti_audio->description.ice_ufrag);
        EXPECT_EQ(current_audio_pwd, ti_audio->description.ice_pwd);
      } else {
        EXPECT_EQ(static_cast<size_t>(ICE_UFRAG_LENGTH),
                  ti_audio->description.ice_ufrag.size());
        EXPECT_EQ(static_cast<size_t>(ICE_PWD_LENGTH),
                  ti_audio->description.ice_pwd.size());
      }
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("audio", options);
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_audio));
    }
    const TransportInfo* ti_video = desc->GetTransportInfoByName("video");
    if (options.has_video()) {
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("video", options);
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_video->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd, ti_video->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_video_ufrag, ti_video->description.ice_ufrag);
          EXPECT_EQ(current_video_pwd, ti_video->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(ICE_UFRAG_LENGTH),
                    ti_video->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(ICE_PWD_LENGTH),
                    ti_video->description.ice_pwd.size());
        }
      }
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_video));
    }
    const TransportInfo* ti_data = desc->GetTransportInfoByName("data");
    if (options.has_data()) {
      if (options.bundle_enabled) {
        EXPECT_EQ(ti_audio->description.ice_ufrag,
                  ti_data->description.ice_ufrag);
        EXPECT_EQ(ti_audio->description.ice_pwd, ti_data->description.ice_pwd);
      } else {
        if (has_current_desc) {
          EXPECT_EQ(current_data_ufrag, ti_data->description.ice_ufrag);
          EXPECT_EQ(current_data_pwd, ti_data->description.ice_pwd);
        } else {
          EXPECT_EQ(static_cast<size_t>(ICE_UFRAG_LENGTH),
                    ti_data->description.ice_ufrag.size());
          EXPECT_EQ(static_cast<size_t>(ICE_PWD_LENGTH),
                    ti_data->description.ice_pwd.size());
        }
      }
      auto media_desc_options_it =
          FindFirstMediaDescriptionByMid("data", options);
      EXPECT_EQ(
          media_desc_options_it->transport_options.enable_ice_renomination,
          GetIceRenomination(ti_data));
    }
  }

  void TestMediaDirectionInAnswer(
      RtpTransceiverDirection direction_in_offer,
      RtpTransceiverDirection expected_direction_in_answer) {
    MediaSessionOptions offer_opts;
    AddAudioVideoSections(direction_in_offer, &offer_opts);

    std::unique_ptr<SessionDescription> offer =
        f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
    ASSERT_TRUE(offer.get());
    ContentInfo* ac_offer = offer->GetContentByName("audio");
    ASSERT_TRUE(ac_offer);
    ContentInfo* vc_offer = offer->GetContentByName("video");
    ASSERT_TRUE(vc_offer);

    MediaSessionOptions answer_opts;
    AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &answer_opts);
    std::unique_ptr<SessionDescription> answer =
        f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
    const AudioContentDescription* acd_answer =
        GetFirstAudioContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, acd_answer->direction());
    const VideoContentDescription* vcd_answer =
        GetFirstVideoContentDescription(answer.get());
    EXPECT_EQ(expected_direction_in_answer, vcd_answer->direction());
  }

  bool VerifyNoCNCodecs(const ContentInfo* content) {
    RTC_DCHECK(content);
    RTC_CHECK(content->media_description());
    for (const Codec& codec : content->media_description()->codecs()) {
      if (codec.name == "CN") {
        return false;
      }
    }
    return true;
  }

  void TestTransportSequenceNumberNegotiation(
      const RtpHeaderExtensions& local,
      const RtpHeaderExtensions& offered,
      const RtpHeaderExtensions& expectedAnswer) {
    MediaSessionOptions opts;
    AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
    SetAudioVideoRtpHeaderExtensions(offered, offered, &opts);
    std::unique_ptr<SessionDescription> offer =
        f1_.CreateOfferOrError(opts, nullptr).MoveValue();
    ASSERT_TRUE(offer.get());
    SetAudioVideoRtpHeaderExtensions(local, local, &opts);
    std::unique_ptr<SessionDescription> answer =
        f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

    EXPECT_EQ(
        expectedAnswer,
        GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
    EXPECT_EQ(
        expectedAnswer,
        GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
  }

  std::vector<webrtc::RtpHeaderExtensionCapability>
  HeaderExtensionCapabilitiesFromRtpExtensions(RtpHeaderExtensions extensions) {
    std::vector<webrtc::RtpHeaderExtensionCapability> capabilities;
    for (const auto& extension : extensions) {
      webrtc::RtpHeaderExtensionCapability capability(
          extension.uri, extension.id,
          webrtc::RtpTransceiverDirection::kSendRecv);
      capabilities.push_back(capability);
    }
    return capabilities;
  }

  void SetAudioVideoRtpHeaderExtensions(RtpHeaderExtensions audio_exts,
                                        RtpHeaderExtensions video_exts,
                                        MediaSessionOptions* opts) {
    auto audio_caps = HeaderExtensionCapabilitiesFromRtpExtensions(audio_exts);
    auto video_caps = HeaderExtensionCapabilitiesFromRtpExtensions(video_exts);
    for (auto& entry : opts->media_description_options) {
      switch (entry.type) {
        case MEDIA_TYPE_AUDIO:
          entry.header_extensions = audio_caps;
          break;
        case MEDIA_TYPE_VIDEO:
          entry.header_extensions = video_caps;
          break;
        default:
          break;
      }
    }
  }

 protected:
  webrtc::test::ScopedKeyValueConfig field_trials;
  UniqueRandomIdGenerator ssrc_generator1;
  UniqueRandomIdGenerator ssrc_generator2;
  TransportDescriptionFactory tdf1_;
  TransportDescriptionFactory tdf2_;
  MediaSessionDescriptionFactory f1_;
  MediaSessionDescriptionFactory f2_;
};

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioOffer) {
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(CreateAudioMediaSession(), nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  EXPECT_FALSE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd = ac->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());
  EXPECT_EQ(0U, acd->first_ssrc());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, acd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateAudioOfferWithJustOpusAndRed) {

  std::vector<webrtc::RtpCodecCapability> preferences;
  preferences.push_back(
      webrtc::ToRtpCodecCapability(f1_.audio_sendrecv_codecs()[0]));
  preferences.push_back(
      webrtc::ToRtpCodecCapability(f1_.audio_sendrecv_codecs()[1]));
  EXPECT_EQ("opus", preferences[0].name);
  EXPECT_EQ("red", preferences[1].name);

  auto opts = CreateAudioMediaSession();
  opts.media_description_options.at(0).codec_preferences = preferences;
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd = ac->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(2U, acd->codecs().size());
  EXPECT_EQ("opus", acd->codecs()[0].name);
  EXPECT_EQ("red", acd->codecs()[1].name);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioOfferWithRedForOpus) {

  std::vector<webrtc::RtpCodecCapability> preferences;
  preferences.push_back(
      webrtc::ToRtpCodecCapability(f1_.audio_sendrecv_codecs()[1]));
  preferences.push_back(
      webrtc::ToRtpCodecCapability(f1_.audio_sendrecv_codecs()[0]));
  EXPECT_EQ("red", preferences[0].name);
  EXPECT_EQ("opus", preferences[1].name);

  auto opts = CreateAudioMediaSession();
  opts.media_description_options.at(0).codec_preferences = preferences;
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac != NULL);
  ASSERT_TRUE(vc == NULL);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd = ac->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(2U, acd->codecs().size());
  EXPECT_EQ("red", acd->codecs()[0].name);
  EXPECT_EQ("opus", acd->codecs()[1].name);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());
  EXPECT_EQ(0U, acd->first_ssrc());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, acd->protocol());
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_sendrecv_codecs(), vcd->codecs());
  EXPECT_EQ(0U, vcd->first_ssrc());
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());
  EXPECT_TRUE(vcd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, vcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestBundleOfferWithSameCodecPlType) {
  const VideoCodec& offered_video_codec = f2_.video_sendrecv_codecs()[0];
  const AudioCodec& offered_audio_codec = f2_.audio_sendrecv_codecs()[0];
  ASSERT_EQ(offered_video_codec.id, offered_audio_codec.id);

  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> offer =
      f2_.CreateOfferOrError(opts, nullptr).MoveValue();
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(offer.get());
  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(vcd);
  ASSERT_TRUE(acd);
  EXPECT_NE(vcd->codecs()[0].id, acd->codecs()[0].id);
  EXPECT_EQ(vcd->codecs()[0].name, offered_video_codec.name);
  EXPECT_EQ(acd->codecs()[0].name, offered_audio_codec.name);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateUpdatedVideoOfferWithBundle) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kInactive, kStopped,
                             &opts);
  opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  MediaSessionOptions updated_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &updated_opts);
  updated_opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(updated_opts, answer.get()).MoveValue());

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(updated_offer.get());
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_TRUE(vcd);
  EXPECT_TRUE(acd);

  EXPECT_EQ(kMediaProtocolDtlsSavpf, acd->protocol());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, vcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSctpDataOffer) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  EXPECT_TRUE(offer.get());
  EXPECT_TRUE(offer->GetContentByName("data"));
  auto dcd = GetFirstSctpDataContentDescription(offer.get());
  ASSERT_TRUE(dcd);

  EXPECT_EQ(kMediaProtocolUdpDtlsSctp, dcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSecureSctpDataOffer) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  EXPECT_TRUE(offer.get());
  EXPECT_TRUE(offer->GetContentByName("data"));
  auto dcd = GetFirstSctpDataContentDescription(offer.get());
  ASSERT_TRUE(dcd);

  EXPECT_EQ(kMediaProtocolUdpDtlsSctp, dcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateImplicitSctpDataOffer) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer1(
      f1_.CreateOfferOrError(opts, nullptr).MoveValue());
  ASSERT_TRUE(offer1.get());
  const ContentInfo* data = offer1->GetContentByName("data");
  ASSERT_TRUE(data);
  ASSERT_EQ(kMediaProtocolUdpDtlsSctp, data->media_description()->protocol());

  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOfferOrError(opts, offer1.get()).MoveValue());
  data = offer2->GetContentByName("data");
  ASSERT_TRUE(data);
  EXPECT_EQ(kMediaProtocolUdpDtlsSctp, data->media_description()->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, ReOfferNoBundleGroupIfAllRejected) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();

  EXPECT_FALSE(reoffer->GetGroupByName(GROUP_TYPE_BUNDLE));
}

TEST_F(MediaSessionDescriptionFactoryTest, ReAnswerNoBundleGroupIfAllRejected) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();
  std::unique_ptr<SessionDescription> reanswer =
      f2_.CreateAnswerOrError(reoffer.get(), opts, answer.get()).MoveValue();

  EXPECT_FALSE(reanswer->GetGroupByName(GROUP_TYPE_BUNDLE));
}

TEST_F(MediaSessionDescriptionFactoryTest, ReOfferChangeBundleOffererTagged) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();

  const ContentGroup* bundle_group = reoffer->GetGroupByName(GROUP_TYPE_BUNDLE);
  ASSERT_TRUE(bundle_group);
  EXPECT_FALSE(bundle_group->HasContentName("audio"));
  EXPECT_TRUE(bundle_group->HasContentName("video"));
}

TEST_F(MediaSessionDescriptionFactoryTest, ReAnswerChangedBundleOffererTagged) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();
  std::unique_ptr<SessionDescription> reanswer =
      f2_.CreateAnswerOrError(reoffer.get(), opts, answer.get()).MoveValue();

  const ContentGroup* bundle_group =
      reanswer->GetGroupByName(GROUP_TYPE_BUNDLE);
  ASSERT_TRUE(bundle_group);
  EXPECT_FALSE(bundle_group->HasContentName("audio"));
  EXPECT_TRUE(bundle_group->HasContentName("video"));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerForOfferWithMultipleBundleGroups) {

  MediaSessionOptions opts;
  opts.bundle_enabled = false;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "3",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "4",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer->groups().empty());

  ContentGroup bundle_group1(GROUP_TYPE_BUNDLE);
  bundle_group1.AddContentName("1");
  bundle_group1.AddContentName("2");
  ContentGroup bundle_group2(GROUP_TYPE_BUNDLE);
  bundle_group2.AddContentName("3");
  bundle_group2.AddContentName("4");
  offer->AddGroup(bundle_group1);
  offer->AddGroup(bundle_group2);

  opts.bundle_enabled = true;
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  std::vector<const ContentGroup*> answer_groups =
      answer->GetGroupsByName(GROUP_TYPE_BUNDLE);
  ASSERT_EQ(answer_groups.size(), 2u);
  EXPECT_EQ(answer_groups[0]->content_names().size(), 2u);
  EXPECT_TRUE(answer_groups[0]->HasContentName("1"));
  EXPECT_TRUE(answer_groups[0]->HasContentName("2"));
  EXPECT_EQ(answer_groups[1]->content_names().size(), 2u);
  EXPECT_TRUE(answer_groups[1]->HasContentName("3"));
  EXPECT_TRUE(answer_groups[1]->HasContentName("4"));

  opts.bundle_enabled = false;
  answer = f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  answer_groups = answer->GetGroupsByName(GROUP_TYPE_BUNDLE);

  ASSERT_EQ(answer_groups.size(), 2u);
  EXPECT_TRUE(answer_groups[0]->content_names().empty());
  EXPECT_TRUE(answer_groups[1]->content_names().empty());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReOfferChangeBundleOffererTaggedKeepsIceCredentials) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();

  const TransportDescription* offer_tagged =
      offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(offer_tagged);
  const TransportDescription* reoffer_tagged =
      reoffer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(reoffer_tagged);
  EXPECT_EQ(offer_tagged->ice_ufrag, reoffer_tagged->ice_ufrag);
  EXPECT_EQ(offer_tagged->ice_pwd, reoffer_tagged->ice_pwd);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReAnswerChangeBundleOffererTaggedKeepsIceCredentials) {
  MediaSessionOptions opts;
  opts.bundle_enabled = true;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].stopped = true;
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue();
  std::unique_ptr<SessionDescription> reanswer =
      f2_.CreateAnswerOrError(reoffer.get(), opts, answer.get()).MoveValue();

  const TransportDescription* answer_tagged =
      answer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(answer_tagged);
  const TransportDescription* reanswer_tagged =
      reanswer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(reanswer_tagged);
  EXPECT_EQ(answer_tagged->ice_ufrag, reanswer_tagged->ice_ufrag);
  EXPECT_EQ(answer_tagged->ice_pwd, reanswer_tagged->ice_pwd);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateOfferWithoutLegacyStreams) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();

  EXPECT_FALSE(vcd->has_ssrcs());
  EXPECT_FALSE(acd->has_ssrcs());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSendOnlyOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendOnly, &opts);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, 1, &opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                                        {kMediaStream1}, 1, &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  EXPECT_EQ(2u, offer->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer->contents()[0], MEDIA_TYPE_AUDIO));
  EXPECT_TRUE(IsMediaContentOfType(&offer->contents()[1], MEDIA_TYPE_VIDEO));

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaDirection(&offer->contents()[0]));
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly,
            GetMediaDirection(&offer->contents()[1]));
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateOfferContentOrder) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);

  std::unique_ptr<SessionDescription> offer1(
      f1_.CreateOfferOrError(opts, nullptr).MoveValue());
  ASSERT_TRUE(offer1.get());
  EXPECT_EQ(1u, offer1->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer1->contents()[0], MEDIA_TYPE_DATA));

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOfferOrError(opts, offer1.get()).MoveValue());
  ASSERT_TRUE(offer2.get());
  EXPECT_EQ(2u, offer2->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer2->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&offer2->contents()[1], MEDIA_TYPE_VIDEO));

  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer3(
      f1_.CreateOfferOrError(opts, offer2.get()).MoveValue());
  ASSERT_TRUE(offer3.get());
  EXPECT_EQ(3u, offer3->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[1], MEDIA_TYPE_VIDEO));
  EXPECT_TRUE(IsMediaContentOfType(&offer3->contents()[2], MEDIA_TYPE_AUDIO));
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswer) {
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(CreateAudioMediaSession(), nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), CreateAudioMediaSession(), nullptr)
          .MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  EXPECT_FALSE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd = ac->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(0U, acd->first_ssrc());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, acd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswerGcm) {
  MediaSessionOptions opts = CreateAudioMediaSession();
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  EXPECT_FALSE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd = ac->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(0U, acd->first_ssrc());
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, acd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateAudioAnswerWithNoCommonCodecs) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::vector f1_codecs = {CreateAudioCodec(96, "opus", 48000, 1)};
  f1_.set_audio_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateAudioCodec(0, "PCMU", 8000, 1)};
  f2_.set_audio_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  ASSERT_TRUE(ac);
  EXPECT_TRUE(ac->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_EQ(0U, acd->first_ssrc());
  EXPECT_TRUE(acd->rtcp_mux());
  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));
  EXPECT_EQ(0U, vcd->first_ssrc());
  EXPECT_TRUE(vcd->rtcp_mux());
  EXPECT_EQ(kMediaProtocolDtlsSavpf, vcd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateVideoAnswerWithNoCommonCodecs) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::vector f1_codecs = {CreateVideoCodec(96, "H264")};
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(97, "VP8")};
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(vc);
  EXPECT_TRUE(vc->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateVideoAnswerWithOnlyFecCodecsCommon) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::vector f1_codecs = {CreateVideoCodec(96, "H264"),
                           CreateVideoCodec(118, "flexfec-03")};
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(97, "VP8"),
                           CreateVideoCodec(118, "flexfec-03")};
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(vc);
  EXPECT_TRUE(vc->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswerUsesSctpmap) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer);
  SctpDataContentDescription* dcd_offer =
      dc_offer->media_description()->as_sctp();
  EXPECT_TRUE(dcd_offer->use_sctpmap());

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer);
  const SctpDataContentDescription* dcd_answer =
      dc_answer->media_description()->as_sctp();
  EXPECT_TRUE(dcd_answer->use_sctpmap());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateDataAnswerWithoutSctpmap) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer);
  SctpDataContentDescription* dcd_offer =
      dc_offer->media_description()->as_sctp();
  dcd_offer->set_use_sctpmap(false);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer);
  const SctpDataContentDescription* dcd_answer =
      dc_answer->media_description()->as_sctp();
  EXPECT_FALSE(dcd_answer->use_sctpmap());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateDataAnswerToDifferentOfferedProtos) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer);
  SctpDataContentDescription* dcd_offer =
      dc_offer->media_description()->as_sctp();
  ASSERT_TRUE(dcd_offer);

  std::vector<std::string> protos = {"DTLS/SCTP", "UDP/DTLS/SCTP",
                                     "TCP/DTLS/SCTP"};
  for (const std::string& proto : protos) {
    dcd_offer->set_protocol(proto);
    std::unique_ptr<SessionDescription> answer =
        f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
    const ContentInfo* dc_answer = answer->GetContentByName("data");
    ASSERT_TRUE(dc_answer);
    const SctpDataContentDescription* dcd_answer =
        dc_answer->media_description()->as_sctp();
    EXPECT_FALSE(dc_answer->rejected);
    EXPECT_EQ(proto, dcd_answer->protocol());
  }
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateDataAnswerToOfferWithDefinedMessageSize) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer);
  SctpDataContentDescription* dcd_offer =
      dc_offer->media_description()->as_sctp();
  ASSERT_TRUE(dcd_offer);
  dcd_offer->set_max_message_size(1234);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer);
  const SctpDataContentDescription* dcd_answer =
      dc_answer->media_description()->as_sctp();
  EXPECT_FALSE(dc_answer->rejected);
  EXPECT_EQ(1234, dcd_answer->max_message_size());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateDataAnswerToOfferWithZeroMessageSize) {
  MediaSessionOptions opts;
  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* dc_offer = offer->GetContentByName("data");
  ASSERT_TRUE(dc_offer);
  SctpDataContentDescription* dcd_offer =
      dc_offer->media_description()->as_sctp();
  ASSERT_TRUE(dcd_offer);
  dcd_offer->set_max_message_size(0);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* dc_answer = answer->GetContentByName("data");
  ASSERT_TRUE(dc_answer);
  const SctpDataContentDescription* dcd_answer =
      dc_answer->media_description()->as_sctp();
  EXPECT_FALSE(dc_answer->rejected);
  EXPECT_EQ(kSctpSendBufferSize, dcd_answer->max_message_size());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAnswerContentOrder) {
  MediaSessionOptions opts;

  AddDataSection(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer1(
      f1_.CreateOfferOrError(opts, nullptr).MoveValue());
  ASSERT_TRUE(offer1.get());

  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer2(
      f1_.CreateOfferOrError(opts, offer1.get()).MoveValue());
  ASSERT_TRUE(offer2.get());

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer3(
      f1_.CreateOfferOrError(opts, offer2.get()).MoveValue());
  ASSERT_TRUE(offer3.get());

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer3.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer.get());
  EXPECT_EQ(3u, answer->contents().size());
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[0], MEDIA_TYPE_DATA));
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[1], MEDIA_TYPE_AUDIO));
  EXPECT_TRUE(IsMediaContentOfType(&answer->contents()[2], MEDIA_TYPE_VIDEO));
}

TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendReceiveOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kSendRecv,
                             RtpTransceiverDirection::kSendRecv);
}

TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToSendOnlyOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kSendOnly,
                             RtpTransceiverDirection::kRecvOnly);
}

TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToRecvOnlyOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kRecvOnly,
                             RtpTransceiverDirection::kSendOnly);
}

TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerToInactiveOffer) {
  TestMediaDirectionInAnswer(RtpTransceiverDirection::kInactive,
                             RtpTransceiverDirection::kInactive);
}

TEST_F(MediaSessionDescriptionFactoryTest, AudioOfferAnswerWithCryptoDisabled) {
  MediaSessionOptions opts = CreateAudioMediaSession();
  tdf1_.SetInsecureForTesting();
  tdf1_.set_certificate(nullptr);
  tdf2_.SetInsecureForTesting();
  tdf2_.set_certificate(nullptr);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  const AudioContentDescription* offer_acd =
      GetFirstAudioContentDescription(offer.get());
  ASSERT_TRUE(offer_acd);
  EXPECT_EQ(kMediaProtocolAvpf, offer_acd->protocol());

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  const ContentInfo* ac_answer = answer->GetContentByName("audio");
  ASSERT_TRUE(ac_answer);
  EXPECT_FALSE(ac_answer->rejected);

  const AudioContentDescription* answer_acd =
      GetFirstAudioContentDescription(answer.get());
  ASSERT_TRUE(answer_acd);
  EXPECT_EQ(kMediaProtocolAvpf, answer_acd->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestOfferAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension1),
                                   MAKE_VECTOR(kVideoRtpExtension1), &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension2),
                                   MAKE_VECTOR(kVideoRtpExtension2), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtension1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1LocalAndV1InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1LocalAndV1V2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1LocalAndV2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      {});
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV2LocalAndV1InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      {});
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV2LocalAndV1V2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV2LocalAndV2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1V2LocalAndV1InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1V2LocalAndV2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber02));
}
TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithTransportSequenceNumberV1V2LocalAndV1V2InOffer) {
  TestTransportSequenceNumberNegotiation(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01And02),
      MAKE_VECTOR(
          kRtpExtensionTransportSequenceNumber01And02));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestNegotiateFrameDescriptorWhenUnexposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  SetAudioVideoRtpHeaderExtensions(
      MAKE_VECTOR(kRtpExtensionGenericFrameDescriptorUri00),
      MAKE_VECTOR(kRtpExtensionGenericFrameDescriptorUri00), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions(
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01),
      MAKE_VECTOR(kRtpExtensionTransportSequenceNumber01), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(kRtpExtensionGenericFrameDescriptorUri00));
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(kRtpExtensionGenericFrameDescriptorUri00));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestNegotiateFrameDescriptorWhenExposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  SetAudioVideoRtpHeaderExtensions(
      MAKE_VECTOR(kRtpExtensionGenericFrameDescriptorUri00),
      MAKE_VECTOR(kRtpExtensionGenericFrameDescriptorUri00), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(kRtpExtensionGenericFrameDescriptorUri00));
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(kRtpExtensionGenericFrameDescriptorUri00));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       NegotiateDependencyDescriptorWhenUnexposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  RtpExtension offer_dd(RtpExtension::kDependencyDescriptorUri, 7);
  SetAudioVideoRtpHeaderExtensions({}, {offer_dd}, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  RtpExtension local_tsn(RtpExtension::kTransportSequenceNumberUri, 5);
  SetAudioVideoRtpHeaderExtensions({}, {local_tsn}, &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAre(offer_dd));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       NegotiateDependencyDescriptorWhenExposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  RtpExtension offer_dd(RtpExtension::kDependencyDescriptorUri, 7);
  RtpExtension local_dd(RtpExtension::kDependencyDescriptorUri, 5);
  SetAudioVideoRtpHeaderExtensions({}, {offer_dd}, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions({}, {local_dd}, &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAre(offer_dd));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       NegotiateAbsoluteCaptureTimeWhenUnexposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  const RtpHeaderExtensions offered_extensions = {
      RtpExtension(RtpExtension::kAbsoluteCaptureTimeUri, 7)};
  const RtpHeaderExtensions local_extensions = {
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 5)};
  SetAudioVideoRtpHeaderExtensions(offered_extensions, offered_extensions,
                                   &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions(local_extensions, local_extensions, &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(offered_extensions));
  EXPECT_THAT(
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(offered_extensions));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       NegotiateAbsoluteCaptureTimeWhenExposedLocally) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  const RtpHeaderExtensions offered_extensions = {
      RtpExtension(RtpExtension::kAbsoluteCaptureTimeUri, 7)};
  const RtpHeaderExtensions local_extensions = {
      RtpExtension(RtpExtension::kAbsoluteCaptureTimeUri, 5)};
  SetAudioVideoRtpHeaderExtensions(offered_extensions, offered_extensions,
                                   &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions(local_extensions, local_extensions, &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(offered_extensions));
  EXPECT_THAT(
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions(),
      ElementsAreArray(offered_extensions));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       DoNotNegotiateAbsoluteCaptureTimeWhenNotOffered) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  const RtpHeaderExtensions offered_extensions = {
      RtpExtension(RtpExtension::kTransportSequenceNumberUri, 7)};
  const RtpHeaderExtensions local_extensions = {
      RtpExtension(RtpExtension::kAbsoluteCaptureTimeUri, 5)};
  SetAudioVideoRtpHeaderExtensions(offered_extensions, offered_extensions,
                                   &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions(local_extensions, local_extensions, &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions(),
      IsEmpty());
  EXPECT_THAT(
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions(),
      IsEmpty());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OffersUnstoppedExtensionsWithAudioVideoExtensionStopped) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kStopped),
      webrtc::RtpHeaderExtensionCapability("uri2", 3,
                                           RtpTransceiverDirection::kSendOnly)};
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kStopped),
      webrtc::RtpHeaderExtensionCapability("uri3", 7,
                                           RtpTransceiverDirection::kSendOnly)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  EXPECT_THAT(
      offer->contents(),
      ElementsAre(
          Property(&ContentInfo::media_description,
                   Pointee(Property(
                       &MediaContentDescription::rtp_header_extensions,
                       ElementsAre(Field(&RtpExtension::uri, "uri2"))))),
          Property(&ContentInfo::media_description,
                   Pointee(Property(
                       &MediaContentDescription::rtp_header_extensions,
                       ElementsAre(Field(&RtpExtension::uri, "uri3")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OffersUnstoppedExtensionsWithAudioExtensionStopped) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kSendOnly),
      webrtc::RtpHeaderExtensionCapability("uri2", 3,
                                           RtpTransceiverDirection::kStopped)};
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri42", 42,
                                           RtpTransceiverDirection::kSendRecv),
      webrtc::RtpHeaderExtensionCapability("uri3", 7,
                                           RtpTransceiverDirection::kSendOnly)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  EXPECT_THAT(
      offer->contents(),
      ElementsAre(
          Property(&ContentInfo::media_description,
                   Pointee(Property(
                       &MediaContentDescription::rtp_header_extensions,
                       ElementsAre(Field(&RtpExtension::uri, "uri1"))))),
          Property(
              &ContentInfo::media_description,
              Pointee(Property(
                  &MediaContentDescription::rtp_header_extensions,
                  UnorderedElementsAre(Field(&RtpExtension::uri, "uri3"),
                                       Field(&RtpExtension::uri, "uri42")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OffersUnstoppedExtensionsWithVideoExtensionStopped) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 5,
                                           RtpTransceiverDirection::kSendOnly),
      webrtc::RtpHeaderExtensionCapability("uri2", 7,
                                           RtpTransceiverDirection::kSendRecv)};
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri42", 42,
                                           RtpTransceiverDirection::kSendRecv),
      webrtc::RtpHeaderExtensionCapability("uri3", 7,
                                           RtpTransceiverDirection::kStopped)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  EXPECT_THAT(
      offer->contents(),
      ElementsAre(
          Property(
              &ContentInfo::media_description,
              Pointee(Property(
                  &MediaContentDescription::rtp_header_extensions,
                  UnorderedElementsAre(Field(&RtpExtension::uri, "uri1"),
                                       Field(&RtpExtension::uri, "uri2"))))),
          Property(&ContentInfo::media_description,
                   Pointee(Property(
                       &MediaContentDescription::rtp_header_extensions,
                       ElementsAre(Field(&RtpExtension::uri, "uri42")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest, AnswersUnstoppedExtensions) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 4,
                                           RtpTransceiverDirection::kStopped),
      webrtc::RtpHeaderExtensionCapability("uri2", 3,
                                           RtpTransceiverDirection::kSendOnly),
      webrtc::RtpHeaderExtensionCapability("uri3", 2,
                                           RtpTransceiverDirection::kRecvOnly),
      webrtc::RtpHeaderExtensionCapability("uri4", 1,
                                           RtpTransceiverDirection::kSendRecv)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 4,
                                           RtpTransceiverDirection::kSendOnly),
      webrtc::RtpHeaderExtensionCapability("uri2", 3,
                                           RtpTransceiverDirection::kRecvOnly),
      webrtc::RtpHeaderExtensionCapability("uri3", 2,
                                           RtpTransceiverDirection::kStopped),
      webrtc::RtpHeaderExtensionCapability("uri4", 1,
                                           RtpTransceiverDirection::kSendRecv)};
  auto answer = f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  EXPECT_THAT(
      answer->contents(),
      ElementsAre(Property(
          &ContentInfo::media_description,
          Pointee(Property(&MediaContentDescription::rtp_header_extensions,
                           ElementsAre(Field(&RtpExtension::uri, "uri2"),
                                       Field(&RtpExtension::uri, "uri4")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       AppendsUnstoppedExtensionsToCurrentDescription) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kSendRecv)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 2,
                                           RtpTransceiverDirection::kSendRecv),
      webrtc::RtpHeaderExtensionCapability("uri2", 3,
                                           RtpTransceiverDirection::kRecvOnly),
      webrtc::RtpHeaderExtensionCapability("uri3", 5,
                                           RtpTransceiverDirection::kStopped),
      webrtc::RtpHeaderExtensionCapability("uri4", 6,
                                           RtpTransceiverDirection::kSendRecv)};
  auto offer2 = f1_.CreateOfferOrError(opts, offer.get()).MoveValue();
  EXPECT_THAT(
      offer2->contents(),
      ElementsAre(Property(
          &ContentInfo::media_description,
          Pointee(Property(&MediaContentDescription::rtp_header_extensions,
                           ElementsAre(Field(&RtpExtension::uri, "uri1"),
                                       Field(&RtpExtension::uri, "uri2"),
                                       Field(&RtpExtension::uri, "uri4")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       AllowsStoppedExtensionsToBeRemovedFromSubsequentOffer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kSendRecv),
      webrtc::RtpHeaderExtensionCapability("uri2", 2,
                                           RtpTransceiverDirection::kSendRecv)};
  auto offer = f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  opts.media_description_options.back().header_extensions = {
      webrtc::RtpHeaderExtensionCapability("uri1", 1,
                                           RtpTransceiverDirection::kSendRecv),
      webrtc::RtpHeaderExtensionCapability("uri2", 2,
                                           RtpTransceiverDirection::kStopped)};
  auto offer2 = f1_.CreateOfferOrError(opts, offer.get()).MoveValue();
  EXPECT_THAT(
      offer2->contents(),
      ElementsAre(Property(
          &ContentInfo::media_description,
          Pointee(Property(&MediaContentDescription::rtp_header_extensions,
                           ElementsAre(Field(&RtpExtension::uri, "uri1")))))));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsBoth) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);
  f2_.set_enable_encrypted_rtp_header_extensions(true);

  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension1),
                                   MAKE_VECTOR(kVideoRtpExtension1), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension2),
                                   MAKE_VECTOR(kVideoRtpExtension2), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncrypted1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncrypted1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncryptedAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncryptedAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);

  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension1),
                                   MAKE_VECTOR(kVideoRtpExtension1), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension2),
                                   MAKE_VECTOR(kVideoRtpExtension2), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionEncrypted1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionEncrypted1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestOfferAnswerWithEncryptedRtpExtensionsAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f2_.set_enable_encrypted_rtp_header_extensions(true);

  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension1),
                                   MAKE_VECTOR(kVideoRtpExtension1), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension2),
                                   MAKE_VECTOR(kVideoRtpExtension2), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension1),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtension1),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestCreateAnswerWithoutLegacyStreams) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();

  EXPECT_FALSE(acd->has_ssrcs());
  EXPECT_FALSE(vcd->has_ssrcs());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateVideoAnswerRtcpMux) {
  MediaSessionOptions offer_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &offer_opts);

  MediaSessionOptions answer_opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &answer_opts);

  std::unique_ptr<SessionDescription> offer;
  std::unique_ptr<SessionDescription> answer;

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = true;
  offer = f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_TRUE(GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = true;
  answer_opts.rtcp_mux_enabled = false;
  offer = f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_TRUE(GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(answer.get()));
  EXPECT_TRUE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_TRUE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = true;
  offer = f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_TRUE(GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());

  offer_opts.rtcp_mux_enabled = false;
  answer_opts.rtcp_mux_enabled = false;
  offer = f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_TRUE(GetFirstAudioContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(offer.get()));
  ASSERT_TRUE(GetFirstAudioContentDescription(answer.get()));
  ASSERT_TRUE(GetFirstVideoContentDescription(answer.get()));
  EXPECT_FALSE(GetFirstAudioContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(offer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstAudioContentDescription(answer.get())->rtcp_mux());
  EXPECT_FALSE(GetFirstVideoContentDescription(answer.get())->rtcp_mux());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateAudioAnswerToVideo) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  opts.media_description_options[1].stopped = true;
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  ASSERT_TRUE(vc->media_description());
  EXPECT_TRUE(vc->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerToOfferWithRejectedMedia) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* ac = offer->GetContentByName("audio");
  ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  ac->rejected = true;
  vc->rejected = true;
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ac = answer->GetContentByName("audio");
  vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  EXPECT_TRUE(ac->rejected);
  EXPECT_TRUE(vc->rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OfferAndAnswerDoesNotHaveMixedByteSessionAttribute) {
  MediaSessionOptions opts;
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts,                         nullptr).MoveValue();
  offer->set_extmap_allow_mixed(false);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswerOrError(offer.get(), opts,
                                                      nullptr)
          .MoveValue());

  EXPECT_FALSE(answer->extmap_allow_mixed());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OfferAndAnswerHaveMixedByteSessionAttribute) {
  MediaSessionOptions opts;
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts,                         nullptr).MoveValue();
  offer->set_extmap_allow_mixed(true);

  std::unique_ptr<SessionDescription> answer_support(
      f2_.CreateAnswerOrError(offer.get(), opts,
                                                      nullptr)
          .MoveValue());

  EXPECT_TRUE(answer_support->extmap_allow_mixed());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OfferAndAnswerDoesNotHaveMixedByteMediaAttributes) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts,                         nullptr).MoveValue();
  offer->set_extmap_allow_mixed(false);
  MediaContentDescription* audio_offer =
      offer->GetContentDescriptionByName("audio");
  MediaContentDescription* video_offer =
      offer->GetContentDescriptionByName("video");
  ASSERT_EQ(MediaContentDescription::kNo,
            audio_offer->extmap_allow_mixed_enum());
  ASSERT_EQ(MediaContentDescription::kNo,
            video_offer->extmap_allow_mixed_enum());

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswerOrError(offer.get(), opts,
                                                      nullptr)
          .MoveValue());

  MediaContentDescription* audio_answer =
      answer->GetContentDescriptionByName("audio");
  MediaContentDescription* video_answer =
      answer->GetContentDescriptionByName("video");
  EXPECT_EQ(MediaContentDescription::kNo,
            audio_answer->extmap_allow_mixed_enum());
  EXPECT_EQ(MediaContentDescription::kNo,
            video_answer->extmap_allow_mixed_enum());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OfferAndAnswerHaveSameMixedByteMediaAttributes) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts,                         nullptr).MoveValue();
  offer->set_extmap_allow_mixed(false);
  MediaContentDescription* audio_offer =
      offer->GetContentDescriptionByName("audio");
  audio_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);
  MediaContentDescription* video_offer =
      offer->GetContentDescriptionByName("video");
  video_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswerOrError(offer.get(), opts,
                                                      nullptr)
          .MoveValue());

  MediaContentDescription* audio_answer =
      answer->GetContentDescriptionByName("audio");
  MediaContentDescription* video_answer =
      answer->GetContentDescriptionByName("video");
  EXPECT_EQ(MediaContentDescription::kMedia,
            audio_answer->extmap_allow_mixed_enum());
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_answer->extmap_allow_mixed_enum());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       OfferAndAnswerHaveDifferentMixedByteMediaAttributes) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts,                         nullptr).MoveValue();
  offer->set_extmap_allow_mixed(false);
  MediaContentDescription* audio_offer =
      offer->GetContentDescriptionByName("audio");
  audio_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kNo);
  MediaContentDescription* video_offer =
      offer->GetContentDescriptionByName("video");
  video_offer->set_extmap_allow_mixed_enum(MediaContentDescription::kMedia);

  std::unique_ptr<SessionDescription> answer(
      f2_.CreateAnswerOrError(offer.get(), opts,
                                                      nullptr)
          .MoveValue());

  MediaContentDescription* audio_answer =
      answer->GetContentDescriptionByName("audio");
  MediaContentDescription* video_answer =
      answer->GetContentDescriptionByName("video");
  EXPECT_EQ(MediaContentDescription::kNo,
            audio_answer->extmap_allow_mixed_enum());
  EXPECT_EQ(MediaContentDescription::kMedia,
            video_answer->extmap_allow_mixed_enum());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoOffer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kSendRecv, &opts);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, 1, &opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                                        {kMediaStream1}, 1, &opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack2,
                                        {kMediaStream1}, 1, &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  ASSERT_TRUE(offer.get());
  const ContentInfo* ac = offer->GetContentByName("audio");
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();
  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_EQ(f1_.audio_sendrecv_codecs(), acd->codecs());

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_EQ(audio_streams[0].cname, audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_EQ(f1_.video_sendrecv_codecs(), vcd->codecs());

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());
  EXPECT_TRUE(vcd->rtcp_mux());

  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack2,
                                        {kMediaStream2}, 1, &opts);
  DetachSenderFromMediaSection("audio", kAudioTrack2, &opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack3,
                                        {kMediaStream1}, 1, &opts);
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());

  ASSERT_TRUE(updated_offer.get());
  ac = updated_offer->GetContentByName("audio");
  vc = updated_offer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* updated_acd = ac->media_description();
  const MediaContentDescription* updated_vcd = vc->media_description();

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(2U, updated_audio_streams.size());
  EXPECT_EQ(audio_streams[0], updated_audio_streams[0]);
  EXPECT_EQ(kAudioTrack3, updated_audio_streams[1].id);
  ASSERT_EQ(1U, updated_audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, updated_audio_streams[1].ssrcs[0]);
  EXPECT_EQ(updated_audio_streams[0].cname, updated_audio_streams[1].cname);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);

  EXPECT_EQ(updated_video_streams[1].cname, updated_video_streams[0].cname);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateSimulcastVideoOffer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  const int num_sim_layers = 3;
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, num_sim_layers, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  ASSERT_TRUE(offer.get());
  const ContentInfo* vc = offer->GetContentByName("video");
  ASSERT_TRUE(vc);
  const MediaContentDescription* vcd = vc->media_description();

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  const SsrcGroup* sim_ssrc_group =
      video_streams[0].get_ssrc_group(kSimSsrcGroupSemantics);
  ASSERT_TRUE(sim_ssrc_group);
  EXPECT_EQ(static_cast<size_t>(num_sim_layers), sim_ssrc_group->ssrcs.size());
}

MATCHER(RidDescriptionEquals, "Verifies that two RidDescriptions are equal.") {
  const RidDescription& rid1 = std::get<0>(arg);
  const RidDescription& rid2 = std::get<1>(arg);
  return rid1.rid == rid2.rid && rid1.direction == rid2.direction;
}

void CheckSimulcastInSessionDescription(
    const SessionDescription* description,
    const std::string& content_name,
    const std::vector<RidDescription>& send_rids,
    const SimulcastLayerList& send_layers) {
  ASSERT_NE(description, nullptr);
  const ContentInfo* content = description->GetContentByName(content_name);
  ASSERT_NE(content, nullptr);
  const MediaContentDescription* cd = content->media_description();
  ASSERT_NE(cd, nullptr);
  const StreamParamsVec& streams = cd->streams();
  ASSERT_THAT(streams, SizeIs(1));
  const StreamParams& stream = streams[0];
  ASSERT_THAT(stream.ssrcs, IsEmpty());
  EXPECT_TRUE(stream.has_rids());
  const std::vector<RidDescription> rids = stream.rids();

  EXPECT_THAT(rids, Pointwise(RidDescriptionEquals(), send_rids));

  EXPECT_TRUE(cd->HasSimulcast());
  const SimulcastDescription& simulcast = cd->simulcast_description();
  EXPECT_THAT(simulcast.send_layers(), SizeIs(send_layers.size()));
  EXPECT_THAT(simulcast.send_layers(), Pointwise(Eq(), send_layers));

  ASSERT_THAT(simulcast.receive_layers().GetAllLayers(), SizeIs(0));
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateCompliantSimulcastOffer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::vector<RidDescription> send_rids;
  send_rids.push_back(RidDescription("f", RidDirection::kSend));
  send_rids.push_back(RidDescription("h", RidDirection::kSend));
  send_rids.push_back(RidDescription("q", RidDirection::kSend));
  SimulcastLayerList simulcast_layers;
  simulcast_layers.AddLayer(SimulcastLayer(send_rids[0].rid, false));
  simulcast_layers.AddLayer(SimulcastLayer(send_rids[1].rid, true));
  simulcast_layers.AddLayer(SimulcastLayer(send_rids[2].rid, false));
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, send_rids,
                                        simulcast_layers, 0, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  CheckSimulcastInSessionDescription(offer.get(), "video", send_rids,
                                     simulcast_layers);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestOfferWithRidsNoSimulcast) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  RidDescription rid("f", RidDirection::kSend);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, {rid},
                                        SimulcastLayerList(), 0, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  ASSERT_NE(offer.get(), nullptr);
  const ContentInfo* content = offer->GetContentByName("video");
  ASSERT_NE(content, nullptr);
  const MediaContentDescription* cd = content->media_description();
  ASSERT_NE(cd, nullptr);
  const StreamParamsVec& streams = cd->streams();
  ASSERT_THAT(streams, SizeIs(1));
  const StreamParams& stream = streams[0];
  ASSERT_THAT(stream.ssrcs, IsEmpty());
  EXPECT_FALSE(stream.has_rids());
  EXPECT_FALSE(cd->HasSimulcast());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateCompliantSimulcastAnswer) {
  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, 1, &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);

  std::vector<RidDescription> rid_descriptions{
      RidDescription("f", RidDirection::kSend),
      RidDescription("h", RidDirection::kSend),
      RidDescription("q", RidDirection::kSend),
  };
  SimulcastLayerList simulcast_layers;
  simulcast_layers.AddLayer(SimulcastLayer(rid_descriptions[0].rid, false));
  simulcast_layers.AddLayer(SimulcastLayer(rid_descriptions[1].rid, true));
  simulcast_layers.AddLayer(SimulcastLayer(rid_descriptions[2].rid, false));
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, rid_descriptions,
                                        simulcast_layers, 0, &answer_opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();

  CheckSimulcastInSessionDescription(answer.get(), "video", rid_descriptions,
                                     simulcast_layers);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestAnswerWithRidsNoSimulcast) {
  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  RidDescription rid_offer("f", RidDirection::kSend);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, {rid_offer},
                                        SimulcastLayerList(), 0, &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);

  RidDescription rid_answer("f", RidDirection::kReceive);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, {rid_answer},
                                        SimulcastLayerList(), 0, &answer_opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();

  ASSERT_NE(answer.get(), nullptr);
  const ContentInfo* content = offer->GetContentByName("video");
  ASSERT_NE(content, nullptr);
  const MediaContentDescription* cd = content->media_description();
  ASSERT_NE(cd, nullptr);
  const StreamParamsVec& streams = cd->streams();
  ASSERT_THAT(streams, SizeIs(1));
  const StreamParams& stream = streams[0];
  ASSERT_THAT(stream.ssrcs, IsEmpty());
  EXPECT_FALSE(stream.has_rids());
  EXPECT_FALSE(cd->HasSimulcast());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCreateMultiStreamVideoAnswer) {
  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &offer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);
  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack1,
                                        {kMediaStream1}, 1, &answer_opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack1,
                                        {kMediaStream1}, 1, &answer_opts);
  AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO, kAudioTrack2,
                                        {kMediaStream1}, 1, &answer_opts);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();

  ASSERT_TRUE(answer.get());
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();

  EXPECT_EQ(MEDIA_TYPE_AUDIO, acd->type());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const StreamParamsVec& audio_streams = acd->streams();
  ASSERT_EQ(2U, audio_streams.size());
  EXPECT_TRUE(audio_streams[0].cname == audio_streams[1].cname);
  EXPECT_EQ(kAudioTrack1, audio_streams[0].id);
  ASSERT_EQ(1U, audio_streams[0].ssrcs.size());
  EXPECT_NE(0U, audio_streams[0].ssrcs[0]);
  EXPECT_EQ(kAudioTrack2, audio_streams[1].id);
  ASSERT_EQ(1U, audio_streams[1].ssrcs.size());
  EXPECT_NE(0U, audio_streams[1].ssrcs[0]);

  EXPECT_EQ(kAutoBandwidth, acd->bandwidth());
  EXPECT_TRUE(acd->rtcp_mux());

  EXPECT_EQ(MEDIA_TYPE_VIDEO, vcd->type());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));

  const StreamParamsVec& video_streams = vcd->streams();
  ASSERT_EQ(1U, video_streams.size());
  EXPECT_EQ(video_streams[0].cname, audio_streams[0].cname);
  EXPECT_EQ(kVideoTrack1, video_streams[0].id);
  EXPECT_EQ(kAutoBandwidth, vcd->bandwidth());
  EXPECT_TRUE(vcd->rtcp_mux());

  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, kVideoTrack2,
                                        {kMediaStream2}, 1, &answer_opts);
  DetachSenderFromMediaSection("audio", kAudioTrack2, &answer_opts);
  std::unique_ptr<SessionDescription> updated_answer(
      f2_.CreateAnswerOrError(offer.get(), answer_opts, answer.get())
          .MoveValue());

  ASSERT_TRUE(updated_answer.get());
  ac = updated_answer->GetContentByName("audio");
  vc = updated_answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  const MediaContentDescription* updated_acd = ac->media_description();
  const MediaContentDescription* updated_vcd = vc->media_description();

  EXPECT_EQ(acd->type(), updated_acd->type());
  EXPECT_EQ(acd->codecs(), updated_acd->codecs());
  EXPECT_EQ(vcd->type(), updated_vcd->type());
  EXPECT_EQ(vcd->codecs(), updated_vcd->codecs());

  const StreamParamsVec& updated_audio_streams = updated_acd->streams();
  ASSERT_EQ(1U, updated_audio_streams.size());
  EXPECT_TRUE(audio_streams[0] == updated_audio_streams[0]);

  const StreamParamsVec& updated_video_streams = updated_vcd->streams();
  ASSERT_EQ(2U, updated_video_streams.size());
  EXPECT_EQ(video_streams[0], updated_video_streams[0]);
  EXPECT_EQ(kVideoTrack2, updated_video_streams[1].id);

  EXPECT_EQ(updated_video_streams[1].cname, updated_video_streams[0].cname);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswer) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());

  const AudioCodec kUpdatedAudioCodecOffer[] = {
      kAudioCodecsAnswer[0],
      kAudioCodecsAnswer[1],
      kAudioCodecs2[0],
  };

  const VideoCodec kUpdatedVideoCodecOffer[] = {
      kVideoCodecsAnswer[0],
      kVideoCodecs2[1],
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_THAT(updated_acd->codecs(), ElementsAreArray(kUpdatedAudioCodecOffer));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_THAT(updated_vcd->codecs(), ElementsAreArray(kUpdatedVideoCodecOffer));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReOfferDoesNotReUseRecycledAudioCodecs) {
  f1_.set_video_codecs({}, {});
  f2_.set_video_codecs({}, {});

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "a0",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].mid = "a1";
  std::unique_ptr<SessionDescription> reoffer =
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue();

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(reoffer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecs2));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReOfferDoesNotReUseRecycledVideoCodecs) {
  f1_.set_audio_codecs({}, {});
  f2_.set_audio_codecs({}, {});

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "v0",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  auto answer = f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].mid = "v1";
  std::unique_ptr<SessionDescription> reoffer =
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue();

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(reoffer.get());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecs2));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReAnswerDoesNotReUseRecycledAudioCodecs) {
  f1_.set_video_codecs({}, {});
  f2_.set_video_codecs({}, {});

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "a0",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f2_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f1_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].mid = "a1";
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, answer.get()).MoveValue();
  std::unique_ptr<SessionDescription> reanswer =
      f2_.CreateAnswerOrError(reoffer.get(), opts, offer.get()).MoveValue();

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(reanswer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       ReAnswerDoesNotReUseRecycledVideoCodecs) {
  f1_.set_audio_codecs({}, {});
  f2_.set_audio_codecs({}, {});

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "v0",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f2_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f1_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  opts.media_description_options[0].mid = "v1";
  std::unique_ptr<SessionDescription> reoffer =
      f1_.CreateOfferOrError(opts, answer.get()).MoveValue();
  std::unique_ptr<SessionDescription> reanswer =
      f2_.CreateAnswerOrError(reoffer.get(), opts, offer.get()).MoveValue();

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(reanswer.get());
  EXPECT_THAT(vcd->codecs(), ElementsAreArray(kVideoCodecsAnswer));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtx) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &expected_codecs);

  EXPECT_EQ(expected_codecs, vcd->codecs());

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());
  ASSERT_TRUE(updated_offer);
  std::unique_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswerOrError(updated_offer.get(), opts, answer.get())
          .MoveValue());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  EXPECT_EQ(expected_codecs, updated_vcd->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRemappedRtxPayloadType) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);

  VideoCodec vp8_offerer = CreateVideoCodec(100, "VP8");
  VideoCodec vp8_offerer_rtx = CreateVideoRtxCodec(101, vp8_offerer.id);
  VideoCodec vp8_answerer = CreateVideoCodec(110, "VP8");
  VideoCodec vp8_answerer_rtx = CreateVideoRtxCodec(111, vp8_answerer.id);
  VideoCodec vp9 = CreateVideoCodec(120, "VP9");
  VideoCodec vp9_rtx = CreateVideoRtxCodec(121, vp9.id);

  std::vector<VideoCodec> f1_codecs = {vp8_offerer, vp8_offerer_rtx};

  std::vector<VideoCodec> f2_codecs = {vp9, vp9_rtx, vp8_answerer,
                                       vp8_answerer_rtx};

  f1_.set_video_codecs(f1_codecs, f1_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);
  std::vector<AudioCodec> audio_codecs;
  f1_.set_audio_codecs(audio_codecs, audio_codecs);
  f2_.set_audio_codecs(audio_codecs, audio_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  std::vector<VideoCodec> codecs = vcd->codecs();
  ASSERT_EQ(4u, codecs.size());
  EXPECT_EQ(vp8_offerer, codecs[0]);
  EXPECT_EQ(vp8_offerer_rtx, codecs[1]);
  EXPECT_EQ(vp9, codecs[2]);
  EXPECT_EQ(vp9_rtx, codecs[3]);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferWithVideoAndRtxAfterCreatingAudioAnswer) {
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  const AudioContentDescription* acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  opts.media_description_options.clear();
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);
  int used_pl_type = acd->codecs()[0].id;
  f2_codecs[0].id = used_pl_type;
  AddRtxCodec(CreateVideoRtxCodec(125, used_pl_type), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());
  ASSERT_TRUE(updated_offer);
  std::unique_ptr<SessionDescription> updated_answer(
      f1_.CreateAnswerOrError(updated_offer.get(), opts, answer.get())
          .MoveValue());

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(answer.get());
  EXPECT_THAT(updated_acd->codecs(), ElementsAreArray(kAudioCodecsAnswer));

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_answer.get());

  ASSERT_EQ("H264", updated_vcd->codecs()[0].name);
  ASSERT_EQ(kRtxCodecName, updated_vcd->codecs()[1].name);
  int new_h264_pl_type = updated_vcd->codecs()[0].id;
  EXPECT_NE(used_pl_type, new_h264_pl_type);
  VideoCodec rtx = updated_vcd->codecs()[1];
  int pt_referenced_by_rtx =
      rtc::FromString<int>(rtx.params[kCodecParamAssociatedPayloadType]);
  EXPECT_EQ(new_h264_pl_type, pt_referenced_by_rtx);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferWithRtxAfterCreatingAnswerWithoutRtx) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  EXPECT_EQ(expected_codecs, vcd->codecs());

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());
  ASSERT_TRUE(updated_offer);

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());

  expected_codecs.push_back(kVideoCodecs2[1]);
  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs1[1].id), &expected_codecs);
  EXPECT_EQ(expected_codecs, updated_vcd->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest, RtxWithoutApt) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoCodec(126, kRtxCodecName), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(CN_VIDEO);
  ASSERT_TRUE(media_desc);
  std::vector<Codec> codecs = media_desc->codecs();
  for (Codec& codec : codecs) {
    if (absl::StartsWith(codec.name, kRtxCodecName)) {
      codec.params.clear();
    }
  }
  media_desc->set_codecs(codecs);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_THAT(
      GetCodecNames(GetFirstVideoContentDescription(answer.get())->codecs()),
      Not(Contains(kRtxCodecName)));
}

TEST_F(MediaSessionDescriptionFactoryTest, FilterOutRtxIfAptDoesntMatch) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs2[1].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_THAT(
      GetCodecNames(GetFirstVideoContentDescription(answer.get())->codecs()),
      Not(Contains(kRtxCodecName)));
}

TEST_F(MediaSessionDescriptionFactoryTest,
       FilterOutUnsupportedRtxWhenCreatingAnswer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs1[0].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector<VideoCodec> f2_codecs = MAKE_VECTOR(kVideoCodecs2);

  AddRtxCodec(CreateVideoRtxCodec(124, kVideoCodecs2[0].id), &f2_codecs);
  f2_.set_video_codecs(f2_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(answer.get());
  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecsAnswer);
  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &expected_codecs);

  EXPECT_EQ(expected_codecs, vcd->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest, AddSecondRtxInNewOffer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  std::vector<VideoCodec> f1_codecs = MAKE_VECTOR(kVideoCodecs1);

  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  const VideoContentDescription* vcd =
      GetFirstVideoContentDescription(offer.get());

  std::vector<VideoCodec> expected_codecs = MAKE_VECTOR(kVideoCodecs1);
  AddRtxCodec(CreateVideoRtxCodec(126, kVideoCodecs1[1].id), &expected_codecs);
  EXPECT_EQ(expected_codecs, vcd->codecs());

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs1[0].id), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());
  ASSERT_TRUE(updated_offer);
  vcd = GetFirstVideoContentDescription(updated_offer.get());

  AddRtxCodec(CreateVideoRtxCodec(125, kVideoCodecs1[0].id), &expected_codecs);
  EXPECT_EQ(expected_codecs, vcd->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest, SimSsrcsGenerateMultipleRtxSsrcs) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, "stream1",
                                        {"stream1label"}, 3, &opts);

  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(CreateVideoCodec(97, "H264"));
  AddRtxCodec(CreateVideoRtxCodec(125, 97), &f1_codecs);
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(CN_VIDEO);
  ASSERT_TRUE(media_desc);
  const StreamParamsVec& streams = media_desc->streams();

  ASSERT_EQ(1u, streams.size());

  EXPECT_EQ(6u, streams[0].ssrcs.size());

  EXPECT_TRUE(streams[0].has_ssrc_group("SIM"));

  EXPECT_TRUE(streams[0].has_ssrc_group("FID"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  EXPECT_EQ(3u, primary_ssrcs.size());
  std::vector<uint32_t> fid_ssrcs;
  streams[0].GetFidSsrcs(primary_ssrcs, &fid_ssrcs);
  EXPECT_EQ(3u, fid_ssrcs.size());
}

TEST_F(MediaSessionDescriptionFactoryTest, GenerateFlexfecSsrc) {
  webrtc::test::ScopedKeyValueConfig override_field_trials(
      field_trials, "WebRTC-FlexFEC-03/Enabled/");
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, "stream1",
                                        {"stream1label"}, 1, &opts);

  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(CreateVideoCodec(97, "H264"));
  f1_codecs.push_back(CreateVideoCodec(118, "flexfec-03"));
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(CN_VIDEO);
  ASSERT_TRUE(media_desc);
  const StreamParamsVec& streams = media_desc->streams();

  ASSERT_EQ(1u, streams.size());

  EXPECT_EQ(2u, streams[0].ssrcs.size());

  EXPECT_TRUE(streams[0].has_ssrc_group("FEC-FR"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  ASSERT_EQ(1u, primary_ssrcs.size());
  uint32_t flexfec_ssrc;
  EXPECT_TRUE(streams[0].GetFecFrSsrc(primary_ssrcs[0], &flexfec_ssrc));
  EXPECT_NE(flexfec_ssrc, 0u);
}

TEST_F(MediaSessionDescriptionFactoryTest, SimSsrcsGenerateNoFlexfecSsrcs) {
  webrtc::test::ScopedKeyValueConfig override_field_trials(
      field_trials, "WebRTC-FlexFEC-03/Enabled/");
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  AttachSenderToMediaDescriptionOptions("video", MEDIA_TYPE_VIDEO, "stream1",
                                        {"stream1label"}, 3, &opts);

  std::vector<VideoCodec> f1_codecs;
  f1_codecs.push_back(CreateVideoCodec(97, "H264"));
  f1_codecs.push_back(CreateVideoCodec(118, "flexfec-03"));
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  MediaContentDescription* media_desc =
      offer->GetContentDescriptionByName(CN_VIDEO);
  ASSERT_TRUE(media_desc);
  const StreamParamsVec& streams = media_desc->streams();

  ASSERT_EQ(1u, streams.size());

  EXPECT_EQ(3u, streams[0].ssrcs.size());

  EXPECT_TRUE(streams[0].has_ssrc_group("SIM"));

  EXPECT_FALSE(streams[0].has_ssrc_group("FEC-FR"));
  std::vector<uint32_t> primary_ssrcs;
  streams[0].GetPrimarySsrcs(&primary_ssrcs);
  EXPECT_EQ(3u, primary_ssrcs.size());
  for (uint32_t primary_ssrc : primary_ssrcs) {
    uint32_t flexfec_ssrc;
    EXPECT_FALSE(streams[0].GetFecFrSsrc(primary_ssrc, &flexfec_ssrc));
  }
}

TEST_F(MediaSessionDescriptionFactoryTest,
       RespondentCreatesOfferAfterCreatingAnswerWithRtpExtensions) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension1),
                                   MAKE_VECTOR(kVideoRtpExtension1), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension2),
                                   MAKE_VECTOR(kVideoRtpExtension2), &opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtensionAnswer),
      GetFirstAudioContentDescription(answer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtensionAnswer),
      GetFirstVideoContentDescription(answer.get())->rtp_header_extensions());

  std::unique_ptr<SessionDescription> updated_offer(
      f2_.CreateOfferOrError(opts, answer.get()).MoveValue());

  const RtpExtension kUpdatedAudioRtpExtensions[] = {
      kAudioRtpExtensionAnswer[0],
      RtpExtension(kAudioRtpExtension2[1].uri, 13),
      kAudioRtpExtension2[2],
  };

  const RtpExtension kUpdatedVideoRtpExtensions[] = {
      kVideoRtpExtensionAnswer[0],
      RtpExtension(kVideoRtpExtension2[1].uri, 12),
      kVideoRtpExtension2[2],
  };

  const AudioContentDescription* updated_acd =
      GetFirstAudioContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedAudioRtpExtensions),
            updated_acd->rtp_header_extensions());

  const VideoContentDescription* updated_vcd =
      GetFirstVideoContentDescription(updated_offer.get());
  EXPECT_EQ(MAKE_VECTOR(kUpdatedVideoRtpExtensions),
            updated_vcd->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest, RtpExtensionIdReused) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  SetAudioVideoRtpHeaderExtensions(MAKE_VECTOR(kAudioRtpExtension3),
                                   MAKE_VECTOR(kVideoRtpExtension3), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  const RtpExtension kExpectedVideoRtpExtension[] = {
      kVideoRtpExtension3[0],
      kAudioRtpExtension3[1],
  };

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension3),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kExpectedVideoRtpExtension),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());

  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtension3),
            GetFirstAudioContentDescription(updated_offer.get())
                ->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kExpectedVideoRtpExtension),
            GetFirstVideoContentDescription(updated_offer.get())
                ->rtp_header_extensions());
}

TEST_F(MediaSessionDescriptionFactoryTest, RtpExtensionIdReusedEncrypted) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);

  f1_.set_enable_encrypted_rtp_header_extensions(true);
  f2_.set_enable_encrypted_rtp_header_extensions(true);

  SetAudioVideoRtpHeaderExtensions(
      MAKE_VECTOR(kAudioRtpExtension3ForEncryption),
      MAKE_VECTOR(kVideoRtpExtension3ForEncryption), &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  EXPECT_EQ(
      MAKE_VECTOR(kAudioRtpExtension3ForEncryptionOffer),
      GetFirstAudioContentDescription(offer.get())->rtp_header_extensions());
  EXPECT_EQ(
      MAKE_VECTOR(kVideoRtpExtension3ForEncryptionOffer),
      GetFirstVideoContentDescription(offer.get())->rtp_header_extensions());

  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());

  EXPECT_EQ(MAKE_VECTOR(kAudioRtpExtension3ForEncryptionOffer),
            GetFirstAudioContentDescription(updated_offer.get())
                ->rtp_header_extensions());
  EXPECT_EQ(MAKE_VECTOR(kVideoRtpExtension3ForEncryptionOffer),
            GetFirstVideoContentDescription(updated_offer.get())
                ->rtp_header_extensions());
}

TEST(MediaSessionDescription, CopySessionDescription) {
  SessionDescription source;
  ContentGroup group(CN_AUDIO);
  source.AddGroup(group);
  std::unique_ptr<AudioContentDescription> acd =
      std::make_unique<AudioContentDescription>();
  acd->set_codecs(MAKE_VECTOR(kAudioCodecs1));
  acd->AddLegacyStream(1);
  source.AddContent(CN_AUDIO, MediaProtocolType::kRtp, acd->Clone());
  std::unique_ptr<VideoContentDescription> vcd =
      std::make_unique<VideoContentDescription>();
  vcd->set_codecs(MAKE_VECTOR(kVideoCodecs1));
  vcd->AddLegacyStream(2);
  source.AddContent(CN_VIDEO, MediaProtocolType::kRtp, vcd->Clone());

  std::unique_ptr<SessionDescription> copy = source.Clone();
  ASSERT_TRUE(copy.get());
  EXPECT_TRUE(copy->HasGroup(CN_AUDIO));
  const ContentInfo* ac = copy->GetContentByName("audio");
  const ContentInfo* vc = copy->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  EXPECT_EQ(MediaProtocolType::kRtp, ac->type);
  const MediaContentDescription* acd_copy = ac->media_description();
  EXPECT_EQ(acd->codecs(), acd_copy->codecs());
  EXPECT_EQ(1u, acd->first_ssrc());

  EXPECT_EQ(MediaProtocolType::kRtp, vc->type);
  const MediaContentDescription* vcd_copy = vc->media_description();
  EXPECT_EQ(vcd->codecs(), vcd_copy->codecs());
  EXPECT_EQ(2u, vcd->first_ssrc());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudio) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferIceRenomination) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  options.media_description_options[0]
      .transport_options.enable_ice_renomination = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferAudioCurrent) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferMultimedia) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferMultimediaCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoOfferBundle) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  options.bundle_enabled = true;
  TestTransportInfo(true, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoOfferBundleCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  options.bundle_enabled = true;
  TestTransportInfo(true, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerAudio) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerIceRenomination) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  options.media_description_options[0]
      .transport_options.enable_ice_renomination = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerAudioCurrent) {
  MediaSessionOptions options;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &options);
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerMultimedia) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerMultimediaCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestTransportInfoAnswerBundle) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  options.bundle_enabled = true;
  TestTransportInfo(false, options, false);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       TestTransportInfoAnswerBundleCurrent) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  options.bundle_enabled = true;
  TestTransportInfo(false, options, true);
}

TEST_F(MediaSessionDescriptionFactoryTest, TestOfferDtlsSavpfCreateAnswer) {
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(CreateAudioMediaSession(), nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* offer_content = offer->GetContentByName("audio");
  ASSERT_TRUE(offer_content);
  MediaContentDescription* offer_audio_desc =
      offer_content->media_description();
  offer_audio_desc->set_protocol(kMediaProtocolDtlsSavpf);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), CreateAudioMediaSession(), nullptr)
          .MoveValue();
  ASSERT_TRUE(answer);

  const ContentInfo* answer_content = answer->GetContentByName("audio");
  ASSERT_TRUE(answer_content);
  ASSERT_FALSE(answer_content->rejected);

  const MediaContentDescription* answer_audio_desc =
      answer_content->media_description();
  EXPECT_EQ(kMediaProtocolDtlsSavpf, answer_audio_desc->protocol());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestCryptoOfferDtlsButNotSdes) {

  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(options, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  const TransportDescription* audio_offer_trans_desc =
      offer->GetTransportDescriptionByName("audio");
  ASSERT_TRUE(audio_offer_trans_desc->identity_fingerprint.get());
  const TransportDescription* video_offer_trans_desc =
      offer->GetTransportDescriptionByName("video");
  ASSERT_TRUE(video_offer_trans_desc->identity_fingerprint.get());

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), options, nullptr).MoveValue();
  ASSERT_TRUE(answer.get());

  const TransportDescription* audio_answer_trans_desc =
      answer->GetTransportDescriptionByName("audio");
  EXPECT_TRUE(audio_answer_trans_desc->identity_fingerprint.get());
  const TransportDescription* video_answer_trans_desc =
      answer->GetTransportDescriptionByName("video");
  EXPECT_TRUE(video_answer_trans_desc->identity_fingerprint.get());
}

TEST_F(MediaSessionDescriptionFactoryTest, TestVADEnableOption) {
  MediaSessionOptions options;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &options);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(options, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  const ContentInfo* audio_content = offer->GetContentByName("audio");
  EXPECT_FALSE(VerifyNoCNCodecs(audio_content));

  options.vad_enabled = false;
  offer = f1_.CreateOfferOrError(options, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  audio_content = offer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
  std::unique_ptr<SessionDescription> answer =
      f1_.CreateAnswerOrError(offer.get(), options, nullptr).MoveValue();
  ASSERT_TRUE(answer.get());
  audio_content = answer->GetContentByName("audio");
  EXPECT_TRUE(VerifyNoCNCodecs(audio_content));
}

TEST_F(MediaSessionDescriptionFactoryTest, TestMIDsMatchesExistingOffer) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio_modified",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video_modified",
                             RtpTransceiverDirection::kRecvOnly, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_DATA, "data_modified",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());

  const ContentInfo* audio_content = GetFirstAudioContent(updated_offer.get());
  const ContentInfo* video_content = GetFirstVideoContent(updated_offer.get());
  const ContentInfo* data_content = GetFirstDataContent(updated_offer.get());
  ASSERT_TRUE(audio_content);
  ASSERT_TRUE(video_content);
  ASSERT_TRUE(data_content);
  EXPECT_EQ("audio_modified", audio_content->name);
  EXPECT_EQ("video_modified", video_content->name);
  EXPECT_EQ("data_modified", data_content->name);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferWithMultipleAVMediaSections) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio_1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "audio_1", MEDIA_TYPE_AUDIO, kAudioTrack1, {kMediaStream1}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video_1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "video_1", MEDIA_TYPE_VIDEO, kVideoTrack1, {kMediaStream1}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio_2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "audio_2", MEDIA_TYPE_AUDIO, kAudioTrack2, {kMediaStream2}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video_2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "video_2", MEDIA_TYPE_VIDEO, kVideoTrack2, {kMediaStream2}, 1, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);

  ASSERT_EQ(4u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  const MediaContentDescription* acd = offer->contents()[0].media_description();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack1, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(offer->contents()[1].rejected);
  const MediaContentDescription* vcd = offer->contents()[1].media_description();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack1, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());

  EXPECT_FALSE(offer->contents()[2].rejected);
  acd = offer->contents()[2].media_description();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack2, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(offer->contents()[3].rejected);
  vcd = offer->contents()[3].media_description();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack2, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMultipleAVMediaSections) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio_1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "audio_1", MEDIA_TYPE_AUDIO, kAudioTrack1, {kMediaStream1}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video_1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "video_1", MEDIA_TYPE_VIDEO, kVideoTrack1, {kMediaStream1}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio_2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "audio_2", MEDIA_TYPE_AUDIO, kAudioTrack2, {kMediaStream2}, 1, &opts);

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video_2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AttachSenderToMediaDescriptionOptions(
      "video_2", MEDIA_TYPE_VIDEO, kVideoTrack2, {kMediaStream2}, 1, &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();

  ASSERT_EQ(4u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  const MediaContentDescription* acd =
      answer->contents()[0].media_description();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack1, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(answer->contents()[1].rejected);
  const MediaContentDescription* vcd =
      answer->contents()[1].media_description();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack1, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());

  EXPECT_FALSE(answer->contents()[2].rejected);
  acd = answer->contents()[2].media_description();
  ASSERT_EQ(1u, acd->streams().size());
  EXPECT_EQ(kAudioTrack2, acd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, acd->direction());

  EXPECT_FALSE(answer->contents()[3].rejected);
  vcd = answer->contents()[3].media_description();
  ASSERT_EQ(1u, vcd->streams().size());
  EXPECT_EQ(kVideoTrack2, vcd->streams()[0].id);
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, vcd->direction());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferWithMediaSectionStoppedByOfferer) {

  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio2",
                             RtpTransceiverDirection::kInactive, kStopped,
                             &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  EXPECT_TRUE(offer->contents()[1].rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMediaSectionStoppedByOfferer) {

  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio2",
                             RtpTransceiverDirection::kInactive, kStopped,
                             &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_FALSE(offer->contents()[0].rejected);
  EXPECT_TRUE(offer->contents()[1].rejected);

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_EQ(2u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  EXPECT_TRUE(answer->contents()[1].rejected);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerWithMediaSectionRejectedByAnswerer) {

  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &offer_opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  ASSERT_FALSE(offer->contents()[0].rejected);
  ASSERT_FALSE(offer->contents()[1].rejected);

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &answer_opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio2",
                             RtpTransceiverDirection::kInactive, kStopped,
                             &answer_opts);
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), answer_opts, nullptr).MoveValue();
  ASSERT_EQ(2u, answer->contents().size());
  EXPECT_FALSE(answer->contents()[0].rejected);
  EXPECT_TRUE(answer->contents()[1].rejected);

  EXPECT_EQ(offer->transport_infos().size(), answer->transport_infos().size());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferRespectsMediaDescriptionOptionsOrder) {
  MediaSessionOptions opts;

  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();

  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  EXPECT_EQ("video", offer->contents()[0].name);
  EXPECT_EQ("audio", offer->contents()[1].name);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       PayloadTypesSharedByMediaSectionsOfSameType) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  const MediaContentDescription* vcd1 =
      offer->contents()[0].media_description();
  const MediaContentDescription* vcd2 =
      offer->contents()[1].media_description();
  EXPECT_EQ(vcd1->codecs().size(), vcd2->codecs().size());
  ASSERT_EQ(2u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].name, vcd2->codecs()[0].name);
  EXPECT_EQ(vcd1->codecs()[0].id, vcd2->codecs()[0].id);
  EXPECT_EQ(vcd1->codecs()[1].name, vcd2->codecs()[1].name);
  EXPECT_EQ(vcd1->codecs()[1].id, vcd2->codecs()[1].id);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  ASSERT_EQ(2u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description();
  vcd2 = answer->contents()[1].media_description();
  EXPECT_EQ(vcd1->codecs().size(), vcd2->codecs().size());
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].name, vcd2->codecs()[0].name);
  EXPECT_EQ(vcd1->codecs()[0].id, vcd2->codecs()[0].id);
}

#ifdef RTC_ENABLE_H265

TEST_F(MediaSessionDescriptionFactoryTest, H265TxModeIsEqualRetainIt) {
  std::vector f1_codecs = {CreateVideoCodec(96, "H265")};
  f1_codecs.back().tx_mode = "mrst";
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(96, "H265")};
  f2_codecs.back().tx_mode = "mrst";
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(1u, offer->contents().size());
  const MediaContentDescription* vcd1 =
      offer->contents()[0].media_description();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].tx_mode, "mrst");

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  ASSERT_EQ(1u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].tx_mode, "mrst");
}

TEST_F(MediaSessionDescriptionFactoryTest, H265TxModeIsDifferentDropCodecs) {
  std::vector f1_codecs = {CreateVideoCodec(96, "H265")};
  f1_codecs.back().tx_mode = "mrst";
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(96, "H265")};
  f2_codecs.back().tx_mode = "mrmt";
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(1u, offer->contents().size());
  const VideoContentDescription* vcd1 =
      offer->contents()[0].media_description()->as_video();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].tx_mode, "mrst");

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  ASSERT_EQ(1u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description()->as_video();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].tx_mode, absl::nullopt);
}
#endif

TEST_F(MediaSessionDescriptionFactoryTest, PacketizationIsEqual) {
  std::vector f1_codecs = {CreateVideoCodec(96, "H264")};
  f1_codecs.back().packetization = "raw";
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(96, "H264")};
  f2_codecs.back().packetization = "raw";
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(1u, offer->contents().size());
  const MediaContentDescription* vcd1 =
      offer->contents()[0].media_description();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].packetization, "raw");

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  ASSERT_EQ(1u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].packetization, "raw");
}

TEST_F(MediaSessionDescriptionFactoryTest, PacketizationIsDifferent) {
  std::vector f1_codecs = {CreateVideoCodec(96, "H264")};
  f1_codecs.back().packetization = "raw";
  f1_.set_video_codecs(f1_codecs, f1_codecs);

  std::vector f2_codecs = {CreateVideoCodec(96, "H264")};
  f2_codecs.back().packetization = "notraw";
  f2_.set_video_codecs(f2_codecs, f2_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(1u, offer->contents().size());
  const VideoContentDescription* vcd1 =
      offer->contents()[0].media_description()->as_video();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].packetization, "raw");

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  ASSERT_EQ(1u, answer->contents().size());
  vcd1 = answer->contents()[0].media_description()->as_video();
  ASSERT_EQ(1u, vcd1->codecs().size());
  EXPECT_EQ(vcd1->codecs()[0].packetization, absl::nullopt);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateOfferRespectsCodecPreferenceOrder) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  MediaContentDescription* vcd1 = offer->contents()[0].media_description();
  const MediaContentDescription* vcd2 =
      offer->contents()[1].media_description();
  auto video_codecs = MAKE_VECTOR(kVideoCodecs1);
  EXPECT_EQ(video_codecs, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());

  auto video_codecs_reverse = MAKE_VECTOR(kVideoCodecs1Reverse);
  vcd1->set_codecs(video_codecs_reverse);
  std::unique_ptr<SessionDescription> updated_offer(
      f1_.CreateOfferOrError(opts, offer.get()).MoveValue());
  vcd1 = updated_offer->contents()[0].media_description();
  vcd2 = updated_offer->contents()[1].media_description();

  EXPECT_EQ(video_codecs_reverse, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest,
       CreateAnswerRespectsCodecPreferenceOrder) {
  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video1",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video2",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  ASSERT_EQ(2u, offer->contents().size());
  MediaContentDescription* vcd1 = offer->contents()[0].media_description();
  const MediaContentDescription* vcd2 =
      offer->contents()[1].media_description();
  auto video_codecs = MAKE_VECTOR(kVideoCodecs1);
  EXPECT_EQ(video_codecs, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());

  auto video_codecs_reverse = MAKE_VECTOR(kVideoCodecs1Reverse);
  vcd1->set_codecs(video_codecs_reverse);
  std::unique_ptr<SessionDescription> answer =
      f1_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  vcd1 = answer->contents()[0].media_description();
  vcd2 = answer->contents()[1].media_description();

  EXPECT_EQ(video_codecs_reverse, vcd1->codecs());
  EXPECT_EQ(video_codecs, vcd2->codecs());
}

TEST_F(MediaSessionDescriptionFactoryTest, CreateAnswerWithLocalCodecParams) {
  const std::string audio_param_name = "audio_param";
  const std::string audio_value1 = "audio_v1";
  const std::string audio_value2 = "audio_v2";
  const std::string video_param_name = "video_param";
  const std::string video_value1 = "video_v1";
  const std::string video_value2 = "video_v2";

  auto audio_codecs1 = MAKE_VECTOR(kAudioCodecs1);
  auto audio_codecs2 = MAKE_VECTOR(kAudioCodecs1);
  auto video_codecs1 = MAKE_VECTOR(kVideoCodecs1);
  auto video_codecs2 = MAKE_VECTOR(kVideoCodecs1);

  audio_codecs1[0].SetParam(audio_param_name, audio_value1);
  video_codecs1[0].SetParam(video_param_name, video_value1);
  audio_codecs2[0].SetParam(audio_param_name, audio_value2);
  video_codecs2[0].SetParam(video_param_name, video_value2);

  f1_.set_audio_codecs(audio_codecs1, audio_codecs1);
  f1_.set_video_codecs(video_codecs1, video_codecs1);
  f2_.set_audio_codecs(audio_codecs2, audio_codecs2);
  f2_.set_video_codecs(video_codecs2, video_codecs2);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);
  auto offer_acd = offer->contents()[0].media_description();
  auto offer_vcd = offer->contents()[1].media_description();
  std::string value;
  EXPECT_TRUE(offer_acd->codecs()[0].GetParam(audio_param_name, &value));
  EXPECT_EQ(audio_value1, value);
  EXPECT_TRUE(offer_vcd->codecs()[0].GetParam(video_param_name, &value));
  EXPECT_EQ(video_value1, value);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);
  auto answer_acd = answer->contents()[0].media_description();
  auto answer_vcd = answer->contents()[1].media_description();

  EXPECT_TRUE(answer_acd->codecs()[0].GetParam(audio_param_name, &value));
  EXPECT_EQ(audio_value2, value);
  EXPECT_TRUE(answer_vcd->codecs()[0].GetParam(video_param_name, &value));
  EXPECT_EQ(video_value2, value);
}

TEST_F(MediaSessionDescriptionFactoryTest,
       H264MatchCriteriaIncludesPacketizationMode) {

  VideoCodec h264_pm0 = CreateVideoCodec(96, "H264");
  h264_pm0.params[kH264FmtpProfileLevelId] = "42c01f";
  h264_pm0.params[kH264FmtpPacketizationMode] = "0";
  VideoCodec h264_pm1 = CreateVideoCodec(97, "H264");
  h264_pm1.params[kH264FmtpProfileLevelId] = "42c01f";
  h264_pm1.params[kH264FmtpPacketizationMode] = "1";

  f1_.set_video_codecs({h264_pm0, h264_pm1}, {h264_pm0, h264_pm1});
  f2_.set_video_codecs({h264_pm1}, {h264_pm1});

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_VIDEO, "video",
                             RtpTransceiverDirection::kSendRecv, kActive,
                             &opts);

  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer);

  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  ASSERT_TRUE(answer);

  ASSERT_EQ(1u, answer->contents().size());
  auto answer_vcd = answer->contents()[0].media_description();
  ASSERT_EQ(1u, answer_vcd->codecs().size());
  auto answer_codec = answer_vcd->codecs()[0];
  EXPECT_EQ(h264_pm1.id, answer_codec.id);
}

class MediaProtocolTest : public testing::TestWithParam<const char*> {
 public:
  MediaProtocolTest()
      : tdf1_(field_trials_),
        tdf2_(field_trials_),
        f1_(nullptr, false, &ssrc_generator1, &tdf1_),
        f2_(nullptr, false, &ssrc_generator2, &tdf2_) {
    f1_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs1),
                         MAKE_VECTOR(kAudioCodecs1));
    f1_.set_video_codecs(MAKE_VECTOR(kVideoCodecs1),
                         MAKE_VECTOR(kVideoCodecs1));
    f2_.set_audio_codecs(MAKE_VECTOR(kAudioCodecs2),
                         MAKE_VECTOR(kAudioCodecs2));
    f2_.set_video_codecs(MAKE_VECTOR(kVideoCodecs2),
                         MAKE_VECTOR(kVideoCodecs2));
    tdf1_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id1"))));
    tdf2_.set_certificate(rtc::RTCCertificate::Create(
        std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id2"))));
  }

 protected:
  webrtc::test::ScopedKeyValueConfig field_trials_;
  TransportDescriptionFactory tdf1_;
  TransportDescriptionFactory tdf2_;
  MediaSessionDescriptionFactory f1_;
  MediaSessionDescriptionFactory f2_;
  UniqueRandomIdGenerator ssrc_generator1;
  UniqueRandomIdGenerator ssrc_generator2;
};

TEST_P(MediaProtocolTest, TestAudioVideoAcceptance) {
  MediaSessionOptions opts;
  AddAudioVideoSections(RtpTransceiverDirection::kRecvOnly, &opts);
  std::unique_ptr<SessionDescription> offer =
      f1_.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  for (auto& content : offer.get()->contents()) {
    content.media_description()->set_protocol(GetParam());
  }
  std::unique_ptr<SessionDescription> answer =
      f2_.CreateAnswerOrError(offer.get(), opts, nullptr).MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");
  const ContentInfo* vc = answer->GetContentByName("video");
  ASSERT_TRUE(ac);
  ASSERT_TRUE(vc);
  EXPECT_FALSE(ac->rejected);
  EXPECT_FALSE(vc->rejected);
  const MediaContentDescription* acd = ac->media_description();
  const MediaContentDescription* vcd = vc->media_description();
  EXPECT_EQ(GetParam(), acd->protocol());
  EXPECT_EQ(GetParam(), vcd->protocol());
}

INSTANTIATE_TEST_SUITE_P(MediaProtocolPatternTest,
                         MediaProtocolTest,
                         ValuesIn(kMediaProtocols));
INSTANTIATE_TEST_SUITE_P(MediaProtocolDtlsPatternTest,
                         MediaProtocolTest,
                         ValuesIn(kMediaProtocolsDtls));

TEST_F(MediaSessionDescriptionFactoryTest, TestSetAudioCodecs) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  TransportDescriptionFactory tdf(field_trials);
  tdf.set_certificate(rtc::RTCCertificate::Create(
      std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id"))));

  UniqueRandomIdGenerator ssrc_generator;
  MediaSessionDescriptionFactory sf(nullptr, false, &ssrc_generator, &tdf);
  std::vector<AudioCodec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  std::vector<AudioCodec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);

  const std::vector<AudioCodec> sendrecv_codecs =
      MAKE_VECTOR(kAudioCodecsAnswer);
  const std::vector<AudioCodec> no_codecs;

  RTC_CHECK_EQ(send_codecs[2].name, "iLBC")
      << "Please don't change shared test data!";
  RTC_CHECK_EQ(recv_codecs[2].name, "iLBC")
      << "Please don't change shared test data!";

  send_codecs[2].channels = 0;

  recv_codecs[2].name = "ilbc";

  sf.set_audio_codecs(send_codecs, recv_codecs);
  EXPECT_EQ(send_codecs, sf.audio_send_codecs());
  EXPECT_EQ(recv_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(sendrecv_codecs, sf.audio_sendrecv_codecs());

  sf.set_audio_codecs(no_codecs, recv_codecs);
  EXPECT_EQ(no_codecs, sf.audio_send_codecs());
  EXPECT_EQ(recv_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());

  sf.set_audio_codecs(send_codecs, no_codecs);
  EXPECT_EQ(send_codecs, sf.audio_send_codecs());
  EXPECT_EQ(no_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());

  sf.set_audio_codecs(no_codecs, no_codecs);
  EXPECT_EQ(no_codecs, sf.audio_send_codecs());
  EXPECT_EQ(no_codecs, sf.audio_recv_codecs());
  EXPECT_EQ(no_codecs, sf.audio_sendrecv_codecs());
}

bool CodecsMatch(const std::vector<Codec>& codecs1,
                 const std::vector<Codec>& codecs2) {
  if (codecs1.size() != codecs2.size()) {
    return false;
  }

  for (size_t i = 0; i < codecs1.size(); ++i) {
    if (!codecs1[i].Matches(codecs2[i])) {
      return false;
    }
  }
  return true;
}

void TestAudioCodecsOffer(RtpTransceiverDirection direction) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  TransportDescriptionFactory tdf(field_trials);
  tdf.set_certificate(rtc::RTCCertificate::Create(
      std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("id"))));

  UniqueRandomIdGenerator ssrc_generator;
  MediaSessionDescriptionFactory sf(nullptr, false, &ssrc_generator, &tdf);
  const std::vector<AudioCodec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  const std::vector<AudioCodec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);
  const std::vector<AudioCodec> sendrecv_codecs =
      MAKE_VECTOR(kAudioCodecsAnswer);
  sf.set_audio_codecs(send_codecs, recv_codecs);

  MediaSessionOptions opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio", direction, kActive,
                             &opts);

  if (direction == RtpTransceiverDirection::kSendRecv ||
      direction == RtpTransceiverDirection::kSendOnly) {
    AttachSenderToMediaDescriptionOptions(
        "audio", MEDIA_TYPE_AUDIO, kAudioTrack1, {kMediaStream1}, 1, &opts);
  }

  std::unique_ptr<SessionDescription> offer =
      sf.CreateOfferOrError(opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());
  ContentInfo* ac = offer->GetContentByName("audio");

  if (ac) {
    MediaContentDescription* acd = ac->media_description();

    if (acd->direction() == RtpTransceiverDirection::kSendOnly) {
      EXPECT_TRUE(CodecsMatch(send_codecs, acd->codecs()));
    } else if (acd->direction() == RtpTransceiverDirection::kRecvOnly) {
      EXPECT_TRUE(CodecsMatch(recv_codecs, acd->codecs()));
    } else {
      EXPECT_TRUE(CodecsMatch(sendrecv_codecs, acd->codecs()));
    }
  }
}

const AudioCodec kOfferAnswerCodecs[] = {
    CreateAudioCodec(0, "codec0", 16000, 1),
    CreateAudioCodec(1, "codec1", 8000, 1),
    CreateAudioCodec(2, "codec2", 8000, 1),
    CreateAudioCodec(3, "codec3", 8000, 1),
    CreateAudioCodec(4, "codec4", 8000, 2),
    CreateAudioCodec(5, "codec5", 32000, 1),
    CreateAudioCodec(6, "codec6", 48000, 1)};

const int kOfferSendCodecs[] = {0, 1, 3, 5, 6};
const int kOfferRecvCodecs[] = {1, 2, 3, 4, 6};

const int kAnswerSendCodecs[] = {6, 5, 2, 3, 4};
const int kAnswerRecvCodecs[] = {6, 5, 4, 1, 0};

const int kResultSend_RecvCodecs[] = {0, 1, 5, 6};
const int kResultRecv_SendCodecs[] = {2, 3, 4, 6};
const int kResultSendrecv_SendCodecs[] = {3, 6};
const int kResultSendrecv_RecvCodecs[] = {1, 6};
const int kResultSendrecv_SendrecvCodecs[] = {6};

template <typename T, int IDXS>
std::vector<T> VectorFromIndices(const T* array, const int (&indices)[IDXS]) {
  std::vector<T> out;
  out.reserve(IDXS);
  for (int idx : indices)
    out.push_back(array[idx]);

  return out;
}

void TestAudioCodecsAnswer(RtpTransceiverDirection offer_direction,
                           RtpTransceiverDirection answer_direction,
                           bool add_legacy_stream) {
  webrtc::test::ScopedKeyValueConfig field_trials;
  TransportDescriptionFactory offer_tdf(field_trials);
  TransportDescriptionFactory answer_tdf(field_trials);
  offer_tdf.set_certificate(rtc::RTCCertificate::Create(
      std::unique_ptr<rtc::SSLIdentity>(new rtc::FakeSSLIdentity("offer_id"))));
  answer_tdf.set_certificate(
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          new rtc::FakeSSLIdentity("answer_id"))));
  UniqueRandomIdGenerator ssrc_generator1, ssrc_generator2;
  MediaSessionDescriptionFactory offer_factory(nullptr, false, &ssrc_generator1,
                                               &offer_tdf);
  MediaSessionDescriptionFactory answer_factory(nullptr, false,
                                                &ssrc_generator2, &answer_tdf);

  offer_factory.set_audio_codecs(
      VectorFromIndices(kOfferAnswerCodecs, kOfferSendCodecs),
      VectorFromIndices(kOfferAnswerCodecs, kOfferRecvCodecs));
  answer_factory.set_audio_codecs(
      VectorFromIndices(kOfferAnswerCodecs, kAnswerSendCodecs),
      VectorFromIndices(kOfferAnswerCodecs, kAnswerRecvCodecs));

  MediaSessionOptions offer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio", offer_direction,
                             kActive, &offer_opts);

  if (webrtc::RtpTransceiverDirectionHasSend(offer_direction)) {
    AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO,
                                          kAudioTrack1, {kMediaStream1}, 1,
                                          &offer_opts);
  }

  std::unique_ptr<SessionDescription> offer =
      offer_factory.CreateOfferOrError(offer_opts, nullptr).MoveValue();
  ASSERT_TRUE(offer.get());

  MediaSessionOptions answer_opts;
  AddMediaDescriptionOptions(MEDIA_TYPE_AUDIO, "audio", answer_direction,
                             kActive, &answer_opts);

  if (webrtc::RtpTransceiverDirectionHasSend(answer_direction)) {
    AttachSenderToMediaDescriptionOptions("audio", MEDIA_TYPE_AUDIO,
                                          kAudioTrack1, {kMediaStream1}, 1,
                                          &answer_opts);
  }
  std::unique_ptr<SessionDescription> answer =
      answer_factory.CreateAnswerOrError(offer.get(), answer_opts, nullptr)
          .MoveValue();
  const ContentInfo* ac = answer->GetContentByName("audio");

  if (ac) {
    ASSERT_EQ(MEDIA_TYPE_AUDIO, ac->media_description()->type());
    const MediaContentDescription* acd = ac->media_description();

    std::vector<AudioCodec> target_codecs;

    switch (offer_direction) {
      case RtpTransceiverDirection::kInactive:
        target_codecs = VectorFromIndices(kOfferAnswerCodecs,
                                          kResultSendrecv_SendrecvCodecs);
        break;
      case RtpTransceiverDirection::kSendOnly:
        target_codecs =
            VectorFromIndices(kOfferAnswerCodecs, kResultSend_RecvCodecs);
        break;
      case RtpTransceiverDirection::kRecvOnly:
        target_codecs =
            VectorFromIndices(kOfferAnswerCodecs, kResultRecv_SendCodecs);
        break;
      case RtpTransceiverDirection::kSendRecv:
        if (acd->direction() == RtpTransceiverDirection::kSendOnly) {
          target_codecs =
              VectorFromIndices(kOfferAnswerCodecs, kResultSendrecv_SendCodecs);
        } else if (acd->direction() == RtpTransceiverDirection::kRecvOnly) {
          target_codecs =
              VectorFromIndices(kOfferAnswerCodecs, kResultSendrecv_RecvCodecs);
        } else {
          target_codecs = VectorFromIndices(kOfferAnswerCodecs,
                                            kResultSendrecv_SendrecvCodecs);
        }
        break;
      case RtpTransceiverDirection::kStopped:

        RTC_DCHECK_NOTREACHED();
    }

    auto format_codecs = [](const std::vector<AudioCodec>& codecs) {
      rtc::StringBuilder os;
      bool first = true;
      os << "{";
      for (const auto& c : codecs) {
        os << (first ? " " : ", ") << c.id;
        first = false;
      }
      os << " }";
      return os.Release();
    };

    EXPECT_TRUE(acd->codecs() == target_codecs)
        << "Expected: " << format_codecs(target_codecs)
        << ", got: " << format_codecs(acd->codecs()) << "; Offered: "
        << webrtc::RtpTransceiverDirectionToString(offer_direction)
        << ", answerer wants: "
        << webrtc::RtpTransceiverDirectionToString(answer_direction)
        << "; got: "
        << webrtc::RtpTransceiverDirectionToString(acd->direction());
  } else {
    EXPECT_EQ(offer_direction, RtpTransceiverDirection::kInactive)
        << "Only inactive offers are allowed to not generate any audio "
           "content";
  }
}

using AudioCodecsOfferTest = testing::TestWithParam<RtpTransceiverDirection>;

TEST_P(AudioCodecsOfferTest, TestCodecsInOffer) {
  TestAudioCodecsOffer(GetParam());
}

INSTANTIATE_TEST_SUITE_P(MediaSessionDescriptionFactoryTest,
                         AudioCodecsOfferTest,
                         Values(RtpTransceiverDirection::kSendOnly,
                                RtpTransceiverDirection::kRecvOnly,
                                RtpTransceiverDirection::kSendRecv,
                                RtpTransceiverDirection::kInactive));

using AudioCodecsAnswerTest = testing::TestWithParam<
    std::tuple<RtpTransceiverDirection, RtpTransceiverDirection, bool>>;

TEST_P(AudioCodecsAnswerTest, TestCodecsInAnswer) {
  TestAudioCodecsAnswer(std::get<0>(GetParam()), std::get<1>(GetParam()),
                        std::get<2>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(MediaSessionDescriptionFactoryTest,
                         AudioCodecsAnswerTest,
                         Combine(Values(RtpTransceiverDirection::kSendOnly,
                                        RtpTransceiverDirection::kRecvOnly,
                                        RtpTransceiverDirection::kSendRecv,
                                        RtpTransceiverDirection::kInactive),
                                 Values(RtpTransceiverDirection::kSendOnly,
                                        RtpTransceiverDirection::kRecvOnly,
                                        RtpTransceiverDirection::kSendRecv,
                                        RtpTransceiverDirection::kInactive),
                                 Bool()));

}
}
