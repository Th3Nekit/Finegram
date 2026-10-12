/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_offer_answer.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/field_trials_view.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "media/base/codec.h"
#include "media/base/rid_description.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_description_factory.h"
#include "p2p/base/transport_info.h"
#include "pc/channel_interface.h"
#include "pc/dtls_transport.h"
#include "pc/legacy_stats_collector.h"
#include "pc/media_stream.h"
#include "pc/media_stream_proxy.h"
#include "pc/peer_connection_internal.h"
#include "pc/peer_connection_message_handler.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/simulcast_description.h"
#include "pc/usage_pattern.h"
#include "pc/used_ids.h"
#include "pc/webrtc_session_description_factory.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

using cricket::ContentInfo;
using cricket::ContentInfos;
using cricket::MediaContentDescription;
using cricket::MediaProtocolType;
using cricket::RidDescription;
using cricket::RidDirection;
using cricket::SessionDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;
using cricket::StreamParams;
using cricket::TransportInfo;

namespace webrtc {

namespace {

typedef PeerConnectionInterface::RTCOfferAnswerOptions RTCOfferAnswerOptions;

const char kInvalidSdp[] = "Invalid session description.";
const char kInvalidCandidates[] = "Description contains invalid candidates.";
const char kBundleWithoutRtcpMux[] =
    "rtcp-mux must be enabled when BUNDLE "
    "is enabled.";
const char kMlineMismatchInAnswer[] =
    "The order of m-lines in answer doesn't match order in offer. Rejecting "
    "answer.";
const char kMlineMismatchInSubsequentOffer[] =
    "The order of m-lines in subsequent offer doesn't match order from "
    "previous offer/answer.";
const char kSdpWithoutIceUfragPwd[] =
    "Called with SDP without ice-ufrag and ice-pwd.";
const char kSdpWithoutDtlsFingerprint[] =
    "Called with SDP without DTLS fingerprint.";
const char kSdpWithoutCrypto[] = "Called with SDP without crypto setup.";

const char kSessionError[] = "Session error code: ";
const char kSessionErrorDesc[] = "Session error description: ";

static const int kRtcpCnameLength = 16;

static constexpr size_t kMidMaxSize = 16;

const char kDefaultStreamId[] = "default";

static const char kDefaultAudioSenderId[] = "defaulta0";
static const char kDefaultVideoSenderId[] = "defaultv0";

void NoteAddIceCandidateResult(int result) {
  RTC_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.AddIceCandidate", result,
                            kAddIceCandidateMax);
}

std::map<std::string, const cricket::ContentGroup*> GetBundleGroupsByMid(
    const SessionDescription* desc) {
  std::vector<const cricket::ContentGroup*> bundle_groups =
      desc->GetGroupsByName(cricket::GROUP_TYPE_BUNDLE);
  std::map<std::string, const cricket::ContentGroup*> bundle_groups_by_mid;
  for (const cricket::ContentGroup* bundle_group : bundle_groups) {
    for (const std::string& content_name : bundle_group->content_names()) {
      bundle_groups_by_mid[content_name] = bundle_group;
    }
  }
  return bundle_groups_by_mid;
}

bool CheckForRemoteIceRestart(const SessionDescriptionInterface* old_desc,
                              const SessionDescriptionInterface* new_desc,
                              const std::string& content_name) {
  if (!old_desc) {
    return false;
  }
  const SessionDescription* new_sd = new_desc->description();
  const SessionDescription* old_sd = old_desc->description();
  const ContentInfo* cinfo = new_sd->GetContentByName(content_name);
  if (!cinfo || cinfo->rejected) {
    return false;
  }

  const cricket::TransportDescription* new_transport_desc =
      new_sd->GetTransportDescriptionByName(content_name);
  const cricket::TransportDescription* old_transport_desc =
      old_sd->GetTransportDescriptionByName(content_name);
  if (!new_transport_desc || !old_transport_desc) {

    return false;
  }
  if (cricket::IceCredentialsChanged(
          old_transport_desc->ice_ufrag, old_transport_desc->ice_pwd,
          new_transport_desc->ice_ufrag, new_transport_desc->ice_pwd)) {
    RTC_LOG(LS_INFO) << "Remote peer requests ICE restart for " << content_name
                     << ".";
    return true;
  }
  return false;
}

std::string GetSetDescriptionErrorMessage(cricket::ContentSource source,
                                          SdpType type,
                                          const RTCError& error) {
  rtc::StringBuilder oss;
  oss << "Failed to set " << (source == cricket::CS_LOCAL ? "local" : "remote")
      << " " << SdpTypeToString(type) << " sdp: ";
  RTC_DCHECK(!absl::StartsWith(error.message(), oss.str())) << error.message();
  oss << error.message();
  return oss.Release();
}

std::string GetStreamIdsString(rtc::ArrayView<const std::string> stream_ids) {
  std::string output = "streams=[";
  const char* separator = "";
  for (const auto& stream_id : stream_ids) {
    output.append(separator).append(stream_id);
    separator = ", ";
  }
  output.append("]");
  return output;
}

const ContentInfo* FindTransceiverMSection(
    RtpTransceiver* transceiver,
    const SessionDescriptionInterface* session_description) {
  return transceiver->mid()
             ? session_description->description()->GetContentByName(
                   *transceiver->mid())
             : nullptr;
}

std::vector<cricket::StreamParams> GetActiveStreams(
    const cricket::MediaContentDescription* desc) {
  return RtpTransceiverDirectionHasSend(desc->direction())
             ? desc->streams()
             : std::vector<cricket::StreamParams>();
}

bool IsMediaSectionBeingRecycled(SdpType type,
                                 const ContentInfo& content,
                                 const ContentInfo* old_content_one,
                                 const ContentInfo* old_content_two) {
  return type == SdpType::kOffer && !content.rejected &&
         ((old_content_one && old_content_one->rejected) ||
          (old_content_two && old_content_two->rejected));
}

bool MediaSectionsInSameOrder(const SessionDescription& current_desc,
                              const SessionDescription* secondary_current_desc,
                              const SessionDescription& new_desc,
                              const SdpType type) {
  if (current_desc.contents().size() > new_desc.contents().size()) {
    return false;
  }

  for (size_t i = 0; i < current_desc.contents().size(); ++i) {
    const cricket::ContentInfo* secondary_content_info = nullptr;
    if (secondary_current_desc &&
        i < secondary_current_desc->contents().size()) {
      secondary_content_info = &secondary_current_desc->contents()[i];
    }
    if (IsMediaSectionBeingRecycled(type, new_desc.contents()[i],
                                    &current_desc.contents()[i],
                                    secondary_content_info)) {

      continue;
    }
    if (new_desc.contents()[i].name != current_desc.contents()[i].name) {
      return false;
    }
    const MediaContentDescription* new_desc_mdesc =
        new_desc.contents()[i].media_description();
    const MediaContentDescription* current_desc_mdesc =
        current_desc.contents()[i].media_description();
    if (new_desc_mdesc->type() != current_desc_mdesc->type()) {
      return false;
    }
  }
  return true;
}

bool MediaSectionsHaveSameCount(const SessionDescription& desc1,
                                const SessionDescription& desc2) {
  return desc1.contents().size() == desc2.contents().size();
}

RTCError VerifyCrypto(const SessionDescription* desc,
                      bool dtls_enabled,
                      const std::map<std::string, const cricket::ContentGroup*>&
                          bundle_groups_by_mid) {
  for (const cricket::ContentInfo& content_info : desc->contents()) {
    if (content_info.rejected) {
      continue;
    }
    const std::string& mid = content_info.name;
    auto it = bundle_groups_by_mid.find(mid);
    const cricket::ContentGroup* bundle =
        it != bundle_groups_by_mid.end() ? it->second : nullptr;
    if (bundle && mid != *(bundle->FirstContentName())) {

      continue;
    }

    const MediaContentDescription* media = content_info.media_description();
    const TransportInfo* tinfo = desc->GetTransportInfoByName(mid);
    if (!media || !tinfo) {

      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kInvalidSdp);
    }
    if (dtls_enabled) {
      if (!tinfo->description.identity_fingerprint) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             kSdpWithoutDtlsFingerprint);
      }
    } else {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kSdpWithoutCrypto);
    }
  }
  return RTCError::OK();
}

bool VerifyIceUfragPwdPresent(
    const SessionDescription* desc,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  for (const cricket::ContentInfo& content_info : desc->contents()) {
    if (content_info.rejected) {
      continue;
    }
    const std::string& mid = content_info.name;
    auto it = bundle_groups_by_mid.find(mid);
    const cricket::ContentGroup* bundle =
        it != bundle_groups_by_mid.end() ? it->second : nullptr;
    if (bundle && mid != *(bundle->FirstContentName())) {

      continue;
    }

    const TransportInfo* tinfo = desc->GetTransportInfoByName(mid);
    if (!tinfo) {

      RTC_LOG(LS_ERROR) << kInvalidSdp;
      return false;
    }
    if (tinfo->description.ice_ufrag.empty() ||
        tinfo->description.ice_pwd.empty()) {
      RTC_LOG(LS_ERROR) << "Session description must have ice ufrag and pwd.";
      return false;
    }
  }
  return true;
}

RTCError ValidateMids(const cricket::SessionDescription& description) {
  std::set<std::string> mids;
  for (const cricket::ContentInfo& content : description.contents()) {
    if (content.name.empty()) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "A media section is missing a MID attribute.");
    }
    if (content.name.size() > kMidMaxSize) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "The MID attribute exceeds the maximum supported "
                           "length of 16 characters.");
    }
    if (!mids.insert(content.name).second) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Duplicate a=mid value '" + content.name + "'.");
    }
  }
  return RTCError::OK();
}

RTCError FindDuplicateCodecParameters(
    const RtpCodecParameters codec_parameters,
    std::map<int, RtpCodecParameters>& payload_to_codec_parameters) {
  auto existing_codec_parameters =
      payload_to_codec_parameters.find(codec_parameters.payload_type);
  if (existing_codec_parameters != payload_to_codec_parameters.end() &&
      codec_parameters != existing_codec_parameters->second) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "A BUNDLE group contains a codec collision for "
                         "payload_type='" +
                             rtc::ToString(codec_parameters.payload_type) +
                             ". All codecs must share the same type, "
                             "encoding name, clock rate and parameters.");
  }
  payload_to_codec_parameters.insert(
      std::make_pair(codec_parameters.payload_type, codec_parameters));
  return RTCError::OK();
}

RTCError ValidateBundledPayloadTypes(
    const cricket::SessionDescription& description) {

  std::vector<const cricket::ContentGroup*> bundle_groups =
      description.GetGroupsByName(cricket::GROUP_TYPE_BUNDLE);
  for (const cricket::ContentGroup* bundle_group : bundle_groups) {
    std::map<int, RtpCodecParameters> payload_to_codec_parameters;
    for (const std::string& content_name : bundle_group->content_names()) {
      const ContentInfo* content_description =
          description.GetContentByName(content_name);
      if (!content_description) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "A BUNDLE group contains a MID='" + content_name +
                                 "' matching no m= section.");
      }
      const cricket::MediaContentDescription* media_description =
          content_description->media_description();
      RTC_DCHECK(media_description);
      if (content_description->rejected || !media_description ||
          !media_description->has_codecs()) {
        continue;
      }
      const auto type = media_description->type();
      if (type == cricket::MEDIA_TYPE_AUDIO ||
          type == cricket::MEDIA_TYPE_VIDEO) {
        for (const auto& c : media_description->codecs()) {
          auto error = FindDuplicateCodecParameters(
              c.ToCodecParameters(), payload_to_codec_parameters);
          if (!error.ok()) {
            return error;
          }
        }
      }
    }
  }
  return RTCError::OK();
}

RTCError FindDuplicateHeaderExtensionIds(
    const RtpExtension extension,
    std::map<int, RtpExtension>& id_to_extension) {
  auto existing_extension = id_to_extension.find(extension.id);
  if (existing_extension != id_to_extension.end() &&
      !(extension.uri == existing_extension->second.uri &&
        extension.encrypt == existing_extension->second.encrypt)) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "A BUNDLE group contains a codec collision for "
        "header extension id=" +
            rtc::ToString(extension.id) +
            ". The id must be the same across all bundled media descriptions");
  }
  id_to_extension.insert(std::make_pair(extension.id, extension));
  return RTCError::OK();
}

RTCError ValidateBundledRtpHeaderExtensions(
    const cricket::SessionDescription& description) {

  std::vector<const cricket::ContentGroup*> bundle_groups =
      description.GetGroupsByName(cricket::GROUP_TYPE_BUNDLE);
  for (const cricket::ContentGroup* bundle_group : bundle_groups) {
    std::map<int, RtpExtension> id_to_extension;
    for (const std::string& content_name : bundle_group->content_names()) {
      const ContentInfo* content_description =
          description.GetContentByName(content_name);
      if (!content_description) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                             "A BUNDLE group contains a MID='" + content_name +
                                 "' matching no m= section.");
      }
      const cricket::MediaContentDescription* media_description =
          content_description->media_description();
      RTC_DCHECK(media_description);
      if (content_description->rejected || !media_description ||
          !media_description->has_codecs()) {
        continue;
      }

      for (const auto& extension : media_description->rtp_header_extensions()) {
        auto error =
            FindDuplicateHeaderExtensionIds(extension, id_to_extension);
        if (!error.ok()) {
          return error;
        }
      }
    }
  }
  return RTCError::OK();
}

RTCError ValidateRtpHeaderExtensionsForSpecSimulcast(
    const cricket::SessionDescription& description) {
  for (const ContentInfo& content : description.contents()) {
    if (content.type != MediaProtocolType::kRtp || content.rejected) {
      continue;
    }
    const auto media_description = content.media_description();
    if (!media_description->HasSimulcast()) {
      continue;
    }
    auto extensions = media_description->rtp_header_extensions();
    auto it = absl::c_find_if(extensions, [](const RtpExtension& ext) {
      return ext.uri == RtpExtension::kRidUri;
    });
    if (it == extensions.end()) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "The media section with MID='" + content.mid() +
                               "' negotiates simulcast but does not negotiate "
                               "the RID RTP header extension.");
    }
  }
  return RTCError::OK();
}

RTCError ValidateSsrcGroups(const cricket::SessionDescription& description) {
  for (const ContentInfo& content : description.contents()) {
    if (content.type != MediaProtocolType::kRtp) {
      continue;
    }
    for (const StreamParams& stream : content.media_description()->streams()) {
      for (const cricket::SsrcGroup& group : stream.ssrc_groups) {

        if ((group.semantics == cricket::kFidSsrcGroupSemantics &&
             group.ssrcs.size() != 2) ||
            (group.semantics == cricket::kFecFrSsrcGroupSemantics &&
             group.ssrcs.size() != 2) ||
            (group.semantics == cricket::kSimSsrcGroupSemantics &&
             group.ssrcs.size() > kMaxSimulcastStreams)) {
          LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                               "The media section with MID='" + content.mid() +
                                   "' has a ssrc-group with semantics " +
                                   group.semantics +
                                   " and an unexpected number of SSRCs.");
        }
      }
    }
  }
  return RTCError::OK();
}

RTCError ValidatePayloadTypes(const cricket::SessionDescription& description) {
  for (const ContentInfo& content : description.contents()) {
    if (content.type != MediaProtocolType::kRtp) {
      continue;
    }
    const auto media_description = content.media_description();
    RTC_DCHECK(media_description);
    if (content.rejected || !media_description ||
        !media_description->has_codecs()) {
      continue;
    }
    const auto type = media_description->type();
    if (type == cricket::MEDIA_TYPE_AUDIO ||
        type == cricket::MEDIA_TYPE_VIDEO) {
      for (const auto& codec : media_description->codecs()) {
        if (!cricket::UsedPayloadTypes::IsIdValid(
                codec, media_description->rtcp_mux())) {
          LOG_AND_RETURN_ERROR(
              RTCErrorType::INVALID_PARAMETER,
              "The media section with MID='" + content.mid() +
                  "' used an invalid payload type " + rtc::ToString(codec.id) +
                  " for codec '" + codec.name + ", rtcp-mux:" +
                  (media_description->rtcp_mux() ? "enabled" : "disabled"));
        }
      }
    }
  }
  return RTCError::OK();
}

bool IsValidOfferToReceiveMedia(int value) {
  typedef PeerConnectionInterface::RTCOfferAnswerOptions Options;
  return (value >= Options::kUndefined) &&
         (value <= Options::kMaxOfferToReceiveMedia);
}

bool ValidateOfferAnswerOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options) {
  return IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_audio) &&
         IsValidOfferToReceiveMedia(rtc_options.offer_to_receive_video);
}

std::vector<RtpEncodingParameters> GetSendEncodingsFromRemoteDescription(
    const MediaContentDescription& desc) {
  if (!desc.HasSimulcast()) {
    return {};
  }
  std::vector<RtpEncodingParameters> result;
  const SimulcastDescription& simulcast = desc.simulcast_description();

  for (const auto& alternatives : simulcast.receive_layers()) {
    RTC_DCHECK(!alternatives.empty());

    const SimulcastLayer& layer = alternatives[0];
    RtpEncodingParameters parameters;
    parameters.rid = layer.rid;
    parameters.active = !layer.is_paused;
    result.push_back(parameters);
  }

  return result;
}

RTCError UpdateSimulcastLayerStatusInSender(
    const std::vector<SimulcastLayer>& layers,
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternalWithAllLayers();
  std::vector<std::string> disabled_layers;

  for (RtpEncodingParameters& encoding : parameters.encodings) {
    auto iter = std::find_if(layers.begin(), layers.end(),
                             [&encoding](const SimulcastLayer& layer) {
                               return layer.rid == encoding.rid;
                             });

    if (iter == layers.end()) {
      disabled_layers.push_back(encoding.rid);
      continue;
    }

    encoding.active = !iter->is_paused;
  }

  RTCError result = sender->SetParametersInternalWithAllLayers(parameters);
  if (result.ok()) {
    result = sender->DisableEncodingLayers(disabled_layers);
  }

  return result;
}

bool SimulcastIsRejected(const ContentInfo* local_content,
                         const MediaContentDescription& answer_media_desc,
                         bool enable_encrypted_rtp_header_extensions) {
  bool simulcast_offered = local_content &&
                           local_content->media_description() &&
                           local_content->media_description()->HasSimulcast();
  bool simulcast_answered = answer_media_desc.HasSimulcast();
  bool rids_supported = RtpExtension::FindHeaderExtensionByUri(
      answer_media_desc.rtp_header_extensions(), RtpExtension::kRidUri,
      enable_encrypted_rtp_header_extensions
          ? RtpExtension::Filter::kPreferEncryptedExtension
          : RtpExtension::Filter::kDiscardEncryptedExtension);
  return simulcast_offered && (!simulcast_answered || !rids_supported);
}

RTCError DisableSimulcastInSender(
    rtc::scoped_refptr<RtpSenderInternal> sender) {
  RTC_DCHECK(sender);
  RtpParameters parameters = sender->GetParametersInternalWithAllLayers();
  if (parameters.encodings.size() <= 1) {
    return RTCError::OK();
  }

  std::vector<std::string> disabled_layers;
  std::transform(
      parameters.encodings.begin() + 1, parameters.encodings.end(),
      std::back_inserter(disabled_layers),
      [](const RtpEncodingParameters& encoding) { return encoding.rid; });
  return sender->DisableEncodingLayers(disabled_layers);
}

absl::string_view GetDefaultMidForPlanB(cricket::MediaType media_type) {
  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO:
      return cricket::CN_AUDIO;
    case cricket::MEDIA_TYPE_VIDEO:
      return cricket::CN_VIDEO;
    case cricket::MEDIA_TYPE_DATA:
      return cricket::CN_DATA;
    case cricket::MEDIA_TYPE_UNSUPPORTED:
      return "not supported";
  }
  RTC_DCHECK_NOTREACHED();
  return "";
}

void AddPlanBRtpSenderOptions(
    const std::vector<rtc::scoped_refptr<
        RtpSenderProxyWithInternal<RtpSenderInternal>>>& senders,
    cricket::MediaDescriptionOptions* audio_media_description_options,
    cricket::MediaDescriptionOptions* video_media_description_options,
    int num_sim_layers) {
  for (const auto& sender : senders) {
    if (sender->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      if (audio_media_description_options) {
        audio_media_description_options->AddAudioSender(
            sender->id(), sender->internal()->stream_ids());
      }
    } else {
      RTC_DCHECK(sender->media_type() == cricket::MEDIA_TYPE_VIDEO);
      if (video_media_description_options) {
        video_media_description_options->AddVideoSender(
            sender->id(), sender->internal()->stream_ids(), {},
            SimulcastLayerList(), num_sim_layers);
      }
    }
  }
}

cricket::MediaDescriptionOptions GetMediaDescriptionOptionsForTransceiver(
    RtpTransceiver* transceiver,
    const std::string& mid,
    bool is_create_offer) {

  bool stopped =
      is_create_offer ? transceiver->stopping() : transceiver->stopped();
  cricket::MediaDescriptionOptions media_description_options(
      transceiver->media_type(), mid, transceiver->direction(), stopped);
  media_description_options.codec_preferences =
      transceiver->codec_preferences();
  media_description_options.header_extensions =
      transceiver->GetHeaderExtensionsToNegotiate();

  if (stopped || (!RtpTransceiverDirectionHasSend(transceiver->direction()) &&
                  !transceiver->has_ever_been_used_to_send())) {
    return media_description_options;
  }

  cricket::SenderOptions sender_options;
  sender_options.track_id = transceiver->sender()->id();
  sender_options.stream_ids = transceiver->sender()->stream_ids();

  RtpParameters send_parameters =
      transceiver->sender_internal()->GetParametersInternalWithAllLayers();
  bool has_rids = std::any_of(send_parameters.encodings.begin(),
                              send_parameters.encodings.end(),
                              [](const RtpEncodingParameters& encoding) {
                                return !encoding.rid.empty();
                              });

  std::vector<RidDescription> send_rids;
  SimulcastLayerList send_layers;
  for (const RtpEncodingParameters& encoding : send_parameters.encodings) {
    if (encoding.rid.empty()) {
      continue;
    }
    send_rids.push_back(RidDescription(encoding.rid, RidDirection::kSend));
    send_layers.AddLayer(SimulcastLayer(encoding.rid, !encoding.active));
  }

  if (has_rids) {
    sender_options.rids = send_rids;
  }

  sender_options.simulcast_layers = send_layers;

  sender_options.num_sim_layers = has_rids ? 0 : 1;
  media_description_options.sender_options.push_back(sender_options);

  return media_description_options;
}

const ContentInfo* GetContentByIndex(const SessionDescriptionInterface* sdesc,
                                     size_t i) {
  if (!sdesc) {
    return nullptr;
  }
  const ContentInfos& contents = sdesc->description()->contents();
  return (i < contents.size() ? &contents[i] : nullptr);
}

void ExtractSharedMediaSessionOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& rtc_options,
    cricket::MediaSessionOptions* session_options) {
  session_options->vad_enabled = rtc_options.voice_activity_detection;
  session_options->bundle_enabled = rtc_options.use_rtp_mux;
  session_options->raw_packetization_for_video =
      rtc_options.raw_packetization_for_video;
}

std::string GenerateRtcpCname() {
  std::string cname;
  if (!rtc::CreateRandomString(kRtcpCnameLength, &cname)) {
    RTC_LOG(LS_ERROR) << "Failed to generate CNAME.";
    RTC_DCHECK_NOTREACHED();
  }
  return cname;
}

bool CanAddLocalMediaStream(StreamCollectionInterface* current_streams,
                            MediaStreamInterface* new_stream) {
  if (!new_stream || !current_streams) {
    return false;
  }
  if (current_streams->find(new_stream->id()) != nullptr) {
    RTC_LOG(LS_ERROR) << "MediaStream with ID " << new_stream->id()
                      << " is already added.";
    return false;
  }
  return true;
}

rtc::scoped_refptr<DtlsTransport> LookupDtlsTransportByMid(
    rtc::Thread* network_thread,
    JsepTransportController* controller,
    const std::string& mid) {

  return network_thread->BlockingCall(
      [controller, &mid] { return controller->LookupDtlsTransportByMid(mid); });
}

bool ContentHasHeaderExtension(const cricket::ContentInfo& content_info,
                               absl::string_view header_extension_uri) {
  for (const RtpExtension& rtp_header_extension :
       content_info.media_description()->rtp_header_extensions()) {
    if (rtp_header_extension.uri == header_extension_uri) {
      return true;
    }
  }
  return false;
}

}

void UpdateRtpHeaderExtensionPreferencesFromSdpMunging(
    const cricket::SessionDescription* description,
    TransceiverList* transceivers) {

  RTC_DCHECK(description);
  RTC_DCHECK(transceivers);
  for (const auto& content : description->contents()) {
    auto transceiver = transceivers->FindByMid(content.name);
    if (!transceiver) {
      continue;
    }
    auto extension_capabilities = transceiver->GetHeaderExtensionsToNegotiate();

    for (auto& ext : content.media_description()->rtp_header_extensions()) {
      auto it = absl::c_find_if(extension_capabilities,
                                [&ext](const RtpHeaderExtensionCapability c) {
                                  return ext.uri == c.uri;
                                });
      if (it != extension_capabilities.end()) {
        it->direction = RtpTransceiverDirection::kSendRecv;
      }
    }
    transceiver->SetHeaderExtensionsToNegotiate(extension_capabilities);
  }
}

class SdpOfferAnswerHandler::RemoteDescriptionOperation {
 public:
  RemoteDescriptionOperation(
      SdpOfferAnswerHandler* handler,
      std::unique_ptr<SessionDescriptionInterface> desc,
      rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer,
      std::function<void()> operations_chain_callback)
      : handler_(handler),
        desc_(std::move(desc)),
        observer_(std::move(observer)),
        operations_chain_callback_(std::move(operations_chain_callback)),
        unified_plan_(handler_->IsUnifiedPlan()) {
    if (!desc_) {
      type_ = static_cast<SdpType>(-1);
      InvalidParam("SessionDescription is NULL.");
    } else {
      type_ = desc_->GetType();
    }
  }

  ~RemoteDescriptionOperation() {
    RTC_DCHECK_RUN_ON(handler_->signaling_thread());
    SignalCompletion();
    operations_chain_callback_();
  }

  bool ok() const { return error_.ok(); }

  void SignalCompletion() {
    if (!observer_)
      return;

    if (!error_.ok() && type_ != static_cast<SdpType>(-1)) {
      std::string error_message =
          GetSetDescriptionErrorMessage(cricket::CS_REMOTE, type_, error_);
      RTC_LOG(LS_ERROR) << error_message;
      error_.set_message(error_message);
    }

    observer_->OnSetRemoteDescriptionComplete(error_);
    observer_ = nullptr;
  }

  bool HaveSessionError() {
    RTC_DCHECK(ok());
    if (handler_->session_error() != SessionError::kNone)
      InternalError(handler_->GetSessionErrorMsg());
    return !ok();
  }

  bool MaybeRollback() {
    RTC_DCHECK_RUN_ON(handler_->signaling_thread());
    RTC_DCHECK(ok());
    if (type_ != SdpType::kRollback) {

      if (type_ == SdpType::kOffer && unified_plan_ &&
          handler_->pc_->configuration()->enable_implicit_rollback &&
          handler_->signaling_state() ==
              PeerConnectionInterface::kHaveLocalOffer) {
        handler_->Rollback(type_);
      }
      return false;
    }

    if (unified_plan_) {
      error_ = handler_->Rollback(type_);
    } else if (type_ == SdpType::kRollback) {
      Unsupported("Rollback not supported in Plan B");
    }

    return true;
  }

  void ReportOfferAnswerUma() {
    RTC_DCHECK(ok());
    if (type_ == SdpType::kOffer || type_ == SdpType::kAnswer) {
      handler_->pc_->ReportSdpBundleUsage(*desc_.get());
    }
  }

  bool IsDescriptionValid() {
    RTC_DCHECK_RUN_ON(handler_->signaling_thread());
    RTC_DCHECK(ok());
    RTC_DCHECK(bundle_groups_by_mid_.empty()) << "Already called?";
    bundle_groups_by_mid_ = GetBundleGroupsByMid(description());
    error_ = handler_->ValidateSessionDescription(
        desc_.get(), cricket::CS_REMOTE, bundle_groups_by_mid_);
    return ok();
  }

  bool ReplaceRemoteDescriptionAndCheckError() {
    RTC_DCHECK_RUN_ON(handler_->signaling_thread());
    RTC_DCHECK(ok());
    RTC_DCHECK(desc_);
    RTC_DCHECK(!replaced_remote_description_);
#if RTC_DCHECK_IS_ON
    const auto* existing_remote_description = handler_->remote_description();
#endif

    error_ = handler_->ReplaceRemoteDescription(std::move(desc_), type_,
                                                &replaced_remote_description_);

    if (ok()) {
#if RTC_DCHECK_IS_ON

      RTC_DCHECK_EQ(existing_remote_description, old_remote_description());
#endif
    } else {
      SetAsSessionError();
    }

    return ok();
  }

  bool UpdateChannels() {
    RTC_DCHECK(ok());
    RTC_DCHECK(!desc_) << "ReplaceRemoteDescription hasn't been called";

    const auto* remote_description = handler_->remote_description();

    const cricket::SessionDescription* session_desc =
        remote_description->description();

    if (unified_plan_) {
      error_ = handler_->UpdateTransceiversAndDataChannels(
          cricket::CS_REMOTE, *remote_description,
          handler_->local_description(), old_remote_description(),
          bundle_groups_by_mid_);
    } else {

      if (type_ == SdpType::kOffer) {

        error_ = handler_->CreateChannels(*session_desc);
      }

      handler_->RemoveUnusedChannels(session_desc);
    }

    return ok();
  }

  bool UpdateSessionState() {
    RTC_DCHECK(ok());
    error_ = handler_->UpdateSessionState(
        type_, cricket::CS_REMOTE,
        handler_->remote_description()->description(), bundle_groups_by_mid_);
    if (!ok())
      SetAsSessionError();
    return ok();
  }

  bool UseCandidatesInRemoteDescription() {
    RTC_DCHECK(ok());
    if (handler_->local_description() &&
        !handler_->UseCandidatesInRemoteDescription()) {
      InvalidParam(kInvalidCandidates);
    }
    return ok();
  }

  SdpType type() const { return type_; }
  bool unified_plan() const { return unified_plan_; }
  cricket::SessionDescription* description() { return desc_->description(); }

  const SessionDescriptionInterface* old_remote_description() const {
    RTC_DCHECK(!desc_) << "Called before replacing the remote description";
    if (type_ == SdpType::kAnswer)
      return replaced_remote_description_.get();
    return replaced_remote_description_
               ? replaced_remote_description_.get()
               : handler_->current_remote_description();
  }

  const std::map<std::string, const cricket::ContentGroup*>&
  bundle_groups_by_mid() const {
    RTC_DCHECK(ok());
    return bundle_groups_by_mid_;
  }

 private:

  void Unsupported(std::string message) {
    SetError(RTCErrorType::UNSUPPORTED_OPERATION, std::move(message));
  }

  void InvalidParam(std::string message) {
    SetError(RTCErrorType::INVALID_PARAMETER, std::move(message));
  }

  void InternalError(std::string message) {
    SetError(RTCErrorType::INTERNAL_ERROR, std::move(message));
  }

  void SetError(RTCErrorType type, std::string message) {
    RTC_DCHECK(ok()) << "Overwriting an existing error?";
    error_ = RTCError(type, std::move(message));
  }

  void SetAsSessionError() {
    RTC_DCHECK(!ok());
    handler_->SetSessionError(SessionError::kContent, error_.message());
  }

  SdpOfferAnswerHandler* const handler_;
  std::unique_ptr<SessionDescriptionInterface> desc_;

  std::unique_ptr<SessionDescriptionInterface> replaced_remote_description_;
  rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer_;
  std::function<void()> operations_chain_callback_;
  RTCError error_ = RTCError::OK();
  std::map<std::string, const cricket::ContentGroup*> bundle_groups_by_mid_;
  SdpType type_;
  const bool unified_plan_;
};

class SdpOfferAnswerHandler::ImplicitCreateSessionDescriptionObserver
    : public CreateSessionDescriptionObserver {
 public:
  ImplicitCreateSessionDescriptionObserver(
      rtc::WeakPtr<SdpOfferAnswerHandler> sdp_handler,
      rtc::scoped_refptr<SetLocalDescriptionObserverInterface>
          set_local_description_observer)
      : sdp_handler_(std::move(sdp_handler)),
        set_local_description_observer_(
            std::move(set_local_description_observer)) {}
  ~ImplicitCreateSessionDescriptionObserver() override {
    RTC_DCHECK(was_called_);
  }

  void SetOperationCompleteCallback(
      std::function<void()> operation_complete_callback) {
    operation_complete_callback_ = std::move(operation_complete_callback);
  }

  bool was_called() const { return was_called_; }

  void OnSuccess(SessionDescriptionInterface* desc_ptr) override {
    RTC_DCHECK(!was_called_);
    std::unique_ptr<SessionDescriptionInterface> desc(desc_ptr);
    was_called_ = true;

    if (!sdp_handler_) {
      operation_complete_callback_();
      return;
    }

    sdp_handler_->DoSetLocalDescription(
        std::move(desc), std::move(set_local_description_observer_));
    operation_complete_callback_();
  }

  void OnFailure(RTCError error) override {
    RTC_DCHECK(!was_called_);
    was_called_ = true;
    set_local_description_observer_->OnSetLocalDescriptionComplete(RTCError(
        error.type(), std::string("SetLocalDescription failed to create "
                                  "session description - ") +
                          error.message()));
    operation_complete_callback_();
  }

 private:
  bool was_called_ = false;
  rtc::WeakPtr<SdpOfferAnswerHandler> sdp_handler_;
  rtc::scoped_refptr<SetLocalDescriptionObserverInterface>
      set_local_description_observer_;
  std::function<void()> operation_complete_callback_;
};

class CreateSessionDescriptionObserverOperationWrapper
    : public CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserverOperationWrapper(
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer,
      std::function<void()> operation_complete_callback)
      : observer_(std::move(observer)),
        operation_complete_callback_(std::move(operation_complete_callback)) {
    RTC_DCHECK(observer_);
  }
  ~CreateSessionDescriptionObserverOperationWrapper() override {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(was_called_);
#endif
  }

  void OnSuccess(SessionDescriptionInterface* desc) override {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(!was_called_);
    was_called_ = true;
#endif

    operation_complete_callback_();
    observer_->OnSuccess(desc);
  }

  void OnFailure(RTCError error) override {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(!was_called_);
    was_called_ = true;
#endif
    operation_complete_callback_();
    observer_->OnFailure(std::move(error));
  }

 private:
#if RTC_DCHECK_IS_ON
  bool was_called_ = false;
#endif
  rtc::scoped_refptr<CreateSessionDescriptionObserver> observer_;
  std::function<void()> operation_complete_callback_;
};

class SdpOfferAnswerHandler::SetSessionDescriptionObserverAdapter
    : public SetLocalDescriptionObserverInterface,
      public SetRemoteDescriptionObserverInterface {
 public:
  SetSessionDescriptionObserverAdapter(
      rtc::WeakPtr<SdpOfferAnswerHandler> handler,
      rtc::scoped_refptr<SetSessionDescriptionObserver> inner_observer)
      : handler_(std::move(handler)),
        inner_observer_(std::move(inner_observer)) {}

  void OnSetLocalDescriptionComplete(RTCError error) override {
    OnSetDescriptionComplete(std::move(error));
  }

  void OnSetRemoteDescriptionComplete(RTCError error) override {
    OnSetDescriptionComplete(std::move(error));
  }

 private:
  void OnSetDescriptionComplete(RTCError error) {
    if (!handler_)
      return;
    if (error.ok()) {
      handler_->pc_->message_handler()->PostSetSessionDescriptionSuccess(
          inner_observer_.get());
    } else {
      handler_->pc_->message_handler()->PostSetSessionDescriptionFailure(
          inner_observer_.get(), std::move(error));
    }
  }

  rtc::WeakPtr<SdpOfferAnswerHandler> handler_;
  rtc::scoped_refptr<SetSessionDescriptionObserver> inner_observer_;
};

class SdpOfferAnswerHandler::LocalIceCredentialsToReplace {
 public:

  void SetIceCredentialsFromLocalDescriptions(
      const SessionDescriptionInterface* current_local_description,
      const SessionDescriptionInterface* pending_local_description) {
    ice_credentials_.clear();
    if (current_local_description) {
      AppendIceCredentialsFromSessionDescription(*current_local_description);
    }
    if (pending_local_description) {
      AppendIceCredentialsFromSessionDescription(*pending_local_description);
    }
  }

  void ClearIceCredentials() { ice_credentials_.clear(); }

  bool HasIceCredentials() const { return !ice_credentials_.empty(); }

  bool SatisfiesIceRestart(
      const SessionDescriptionInterface& local_description) const {
    for (const auto& transport_info :
         local_description.description()->transport_infos()) {
      if (ice_credentials_.find(std::make_pair(
              transport_info.description.ice_ufrag,
              transport_info.description.ice_pwd)) != ice_credentials_.end()) {
        return false;
      }
    }
    return true;
  }

 private:
  void AppendIceCredentialsFromSessionDescription(
      const SessionDescriptionInterface& desc) {
    for (const auto& transport_info : desc.description()->transport_infos()) {
      ice_credentials_.insert(
          std::make_pair(transport_info.description.ice_ufrag,
                         transport_info.description.ice_pwd));
    }
  }

  std::set<std::pair<std::string, std::string>> ice_credentials_;
};

SdpOfferAnswerHandler::SdpOfferAnswerHandler(PeerConnectionSdpMethods* pc,
                                             ConnectionContext* context)
    : pc_(pc),
      context_(context),
      local_streams_(StreamCollection::Create()),
      remote_streams_(StreamCollection::Create()),
      operations_chain_(rtc::OperationsChain::Create()),
      rtcp_cname_(GenerateRtcpCname()),
      local_ice_credentials_to_replace_(new LocalIceCredentialsToReplace()),
      weak_ptr_factory_(this) {
  operations_chain_->SetOnChainEmptyCallback(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr()]() {
        if (!this_weak_ptr)
          return;
        this_weak_ptr->OnOperationsChainEmpty();
      });
}

SdpOfferAnswerHandler::~SdpOfferAnswerHandler() {}

std::unique_ptr<SdpOfferAnswerHandler> SdpOfferAnswerHandler::Create(
    PeerConnectionSdpMethods* pc,
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies& dependencies,
    ConnectionContext* context) {
  auto handler = absl::WrapUnique(new SdpOfferAnswerHandler(pc, context));
  handler->Initialize(configuration, dependencies, context);
  return handler;
}

void SdpOfferAnswerHandler::Initialize(
    const PeerConnectionInterface::RTCConfiguration& configuration,
    PeerConnectionDependencies& dependencies,
    ConnectionContext* context) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  video_options_.screencast_min_bitrate_kbps =
      configuration.screencast_min_bitrate.value_or(100);

  audio_options_.audio_jitter_buffer_max_packets =
      configuration.audio_jitter_buffer_max_packets;

  audio_options_.audio_jitter_buffer_fast_accelerate =
      configuration.audio_jitter_buffer_fast_accelerate;

  audio_options_.audio_jitter_buffer_min_delay_ms =
      configuration.audio_jitter_buffer_min_delay_ms;

  rtc::scoped_refptr<rtc::RTCCertificate> certificate;
  if (!configuration.certificates.empty()) {

    certificate = configuration.certificates[0];
  }

  webrtc_session_desc_factory_ =
      std::make_unique<WebRtcSessionDescriptionFactory>(
          context, this, pc_->session_id(), pc_->dtls_enabled(),
          std::move(dependencies.cert_generator), std::move(certificate),
          [this](const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
            RTC_DCHECK_RUN_ON(signaling_thread());
            transport_controller_s()->SetLocalCertificate(certificate);
          },
          pc_->trials());

  if (pc_->options()->disable_encryption) {
    RTC_LOG(LS_INFO)
        << "Disabling encryption. This should only be done in tests.";
    webrtc_session_desc_factory_->SetInsecureForTesting();
  }

  webrtc_session_desc_factory_->set_enable_encrypted_rtp_header_extensions(
      pc_->GetCryptoOptions().srtp.enable_encrypted_rtp_header_extensions);
  webrtc_session_desc_factory_->set_is_unified_plan(IsUnifiedPlan());

  if (dependencies.video_bitrate_allocator_factory) {
    video_bitrate_allocator_factory_ =
        std::move(dependencies.video_bitrate_allocator_factory);
  } else {
    video_bitrate_allocator_factory_ =
        CreateBuiltinVideoBitrateAllocatorFactory();
  }
}

cricket::MediaEngineInterface* SdpOfferAnswerHandler::media_engine() const {
  RTC_DCHECK(context_);
  return context_->media_engine();
}

TransceiverList* SdpOfferAnswerHandler::transceivers() {
  if (!pc_->rtp_manager()) {
    return nullptr;
  }
  return pc_->rtp_manager()->transceivers();
}

const TransceiverList* SdpOfferAnswerHandler::transceivers() const {
  if (!pc_->rtp_manager()) {
    return nullptr;
  }
  return pc_->rtp_manager()->transceivers();
}
JsepTransportController* SdpOfferAnswerHandler::transport_controller_s() {
  return pc_->transport_controller_s();
}
JsepTransportController* SdpOfferAnswerHandler::transport_controller_n() {
  return pc_->transport_controller_n();
}
const JsepTransportController* SdpOfferAnswerHandler::transport_controller_s()
    const {
  return pc_->transport_controller_s();
}
const JsepTransportController* SdpOfferAnswerHandler::transport_controller_n()
    const {
  return pc_->transport_controller_n();
}
DataChannelController* SdpOfferAnswerHandler::data_channel_controller() {
  return pc_->data_channel_controller();
}
const DataChannelController* SdpOfferAnswerHandler::data_channel_controller()
    const {
  return pc_->data_channel_controller();
}
cricket::PortAllocator* SdpOfferAnswerHandler::port_allocator() {
  return pc_->port_allocator();
}
const cricket::PortAllocator* SdpOfferAnswerHandler::port_allocator() const {
  return pc_->port_allocator();
}
RtpTransmissionManager* SdpOfferAnswerHandler::rtp_manager() {
  return pc_->rtp_manager();
}
const RtpTransmissionManager* SdpOfferAnswerHandler::rtp_manager() const {
  return pc_->rtp_manager();
}

void SdpOfferAnswerHandler::PrepareForShutdown() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SdpOfferAnswerHandler::Close() {
  ChangeSignalingState(PeerConnectionInterface::kClosed);
}

void SdpOfferAnswerHandler::RestartIce() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  local_ice_credentials_to_replace_->SetIceCredentialsFromLocalDescriptions(
      current_local_description(), pending_local_description());
  UpdateNegotiationNeeded();
}

rtc::Thread* SdpOfferAnswerHandler::signaling_thread() const {
  return context_->signaling_thread();
}

rtc::Thread* SdpOfferAnswerHandler::network_thread() const {
  return context_->network_thread();
}

void SdpOfferAnswerHandler::CreateOffer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<CreateSessionDescriptionObserver>(observer),
       options](std::function<void()> operations_chain_callback) {

        if (!this_weak_ptr) {
          observer_refptr->OnFailure(
              RTCError(RTCErrorType::INTERNAL_ERROR,
                       "CreateOffer failed because the session was shut down"));
          operations_chain_callback();
          return;
        }

        auto observer_wrapper = rtc::make_ref_counted<
            CreateSessionDescriptionObserverOperationWrapper>(
            std::move(observer_refptr), std::move(operations_chain_callback));
        this_weak_ptr->DoCreateOffer(options, observer_wrapper);
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<SetSessionDescriptionObserver>(observer),
       desc = std::unique_ptr<SessionDescriptionInterface>(desc_ptr)](
          std::function<void()> operations_chain_callback) mutable {

        if (!this_weak_ptr) {

          operations_chain_callback();
          return;
        }

        this_weak_ptr->DoSetLocalDescription(
            std::move(desc),
            rtc::make_ref_counted<SetSessionDescriptionObserverAdapter>(
                this_weak_ptr, observer_refptr));

        operations_chain_callback();
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(), observer,
       desc = std::move(desc)](
          std::function<void()> operations_chain_callback) mutable {

        if (!this_weak_ptr) {
          observer->OnSetLocalDescriptionComplete(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetLocalDescription failed because the session was shut down"));
          operations_chain_callback();
          return;
        }
        this_weak_ptr->DoSetLocalDescription(std::move(desc), observer);

        operations_chain_callback();
      });
}

void SdpOfferAnswerHandler::SetLocalDescription(
    SetSessionDescriptionObserver* observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  SetLocalDescription(
      rtc::make_ref_counted<SetSessionDescriptionObserverAdapter>(
          weak_ptr_factory_.GetWeakPtr(),
          rtc::scoped_refptr<SetSessionDescriptionObserver>(observer)));
}

void SdpOfferAnswerHandler::SetLocalDescription(
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  auto create_sdp_observer =
      rtc::make_ref_counted<ImplicitCreateSessionDescriptionObserver>(
          weak_ptr_factory_.GetWeakPtr(), observer);

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       create_sdp_observer](std::function<void()> operations_chain_callback) {

        create_sdp_observer->SetOperationCompleteCallback(
            std::move(operations_chain_callback));

        if (!this_weak_ptr) {
          create_sdp_observer->OnFailure(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetLocalDescription failed because the session was shut down"));
          return;
        }
        switch (this_weak_ptr->signaling_state()) {
          case PeerConnectionInterface::kStable:
          case PeerConnectionInterface::kHaveLocalOffer:
          case PeerConnectionInterface::kHaveRemotePrAnswer:

            this_weak_ptr->DoCreateOffer(
                PeerConnectionInterface::RTCOfferAnswerOptions(),
                create_sdp_observer);
            break;
          case PeerConnectionInterface::kHaveLocalPrAnswer:
          case PeerConnectionInterface::kHaveRemoteOffer:

            this_weak_ptr->DoCreateAnswer(
                PeerConnectionInterface::RTCOfferAnswerOptions(),
                create_sdp_observer);
            break;
          case PeerConnectionInterface::kClosed:
            create_sdp_observer->OnFailure(RTCError(
                RTCErrorType::INVALID_STATE,
                "SetLocalDescription called when PeerConnection is closed."));
            break;
        }
      });
}

RTCError SdpOfferAnswerHandler::ApplyLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::ApplyLocalDescription");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(desc);

  pc_->ClearStatsCache();

  const SessionDescriptionInterface* old_local_description =
      local_description();
  std::unique_ptr<SessionDescriptionInterface> replaced_local_description;
  SdpType type = desc->GetType();
  if (type == SdpType::kAnswer) {
    replaced_local_description = pending_local_description_
                                     ? std::move(pending_local_description_)
                                     : std::move(current_local_description_);
    current_local_description_ = std::move(desc);
    pending_local_description_ = nullptr;
    current_remote_description_ = std::move(pending_remote_description_);
  } else {
    replaced_local_description = std::move(pending_local_description_);
    pending_local_description_ = std::move(desc);
  }
  if (!initial_offerer_) {
    initial_offerer_.emplace(type == SdpType::kOffer);
  }

  RTC_DCHECK(local_description());

  if (!is_caller_) {
    if (remote_description()) {

      is_caller_ = false;
    } else {

      is_caller_ = true;
    }
  }

  RTCError error = PushdownTransportDescription(cricket::CS_LOCAL, type);
  if (!error.ok()) {
    return error;
  }

  if (IsUnifiedPlan()) {
    error = UpdateTransceiversAndDataChannels(
        cricket::CS_LOCAL, *local_description(), old_local_description,
        remote_description(), bundle_groups_by_mid);
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << error.message() << " (" << SdpTypeToString(type)
                        << ")";
      return error;
    }
    if (ConfiguredForMedia()) {
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
      for (const auto& transceiver_ext : transceivers()->List()) {
        auto transceiver = transceiver_ext->internal();
        if (transceiver->stopped()) {
          continue;
        }

        if (transceiver->mid()) {
          auto dtls_transport = LookupDtlsTransportByMid(
              context_->network_thread(), transport_controller_s(),
              *transceiver->mid());
          transceiver->sender_internal()->set_transport(dtls_transport);
          transceiver->receiver_internal()->set_transport(dtls_transport);
        }

        const ContentInfo* content =
            FindMediaSectionForTransceiver(transceiver, local_description());
        if (!content) {
          continue;
        }
        const MediaContentDescription* media_desc =
            content->media_description();

        if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {

          if (!RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
              (transceiver->fired_direction() &&
               RtpTransceiverDirectionHasRecv(
                   *transceiver->fired_direction()))) {
            ProcessRemovalOfRemoteTrack(transceiver_ext, &remove_list,
                                        &removed_streams);
          }

          transceiver->set_current_direction(media_desc->direction());
          transceiver->set_fired_direction(media_desc->direction());
        }
      }
      auto observer = pc_->Observer();
      for (const auto& transceiver : remove_list) {
        observer->OnRemoveTrack(transceiver->receiver());
      }
      for (const auto& stream : removed_streams) {
        observer->OnRemoveStream(stream);
      }
    }
  } else {

    if (type == SdpType::kOffer) {

      error = CreateChannels(*local_description()->description());
      if (!error.ok()) {
        RTC_LOG(LS_ERROR) << error.message() << " (" << SdpTypeToString(type)
                          << ")";
        return error;
      }
    }

    RemoveUnusedChannels(local_description()->description());
  }

  error = UpdateSessionState(type, cricket::CS_LOCAL,
                             local_description()->description(),
                             bundle_groups_by_mid);
  if (!error.ok()) {
    RTC_LOG(LS_ERROR) << error.message() << " (" << SdpTypeToString(type)
                      << ")";
    return error;
  }

  UseCandidatesInRemoteDescription();

  pending_ice_restarts_.clear();
  if (session_error() != SessionError::kNone) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, GetSessionErrorMsg());
  }

  AllocateSctpSids();

  if (ConfiguredForMedia()) {
    std::set<uint32_t> used_ssrcs;
    for (const auto& content : local_description()->description()->contents()) {
      for (const auto& stream : content.media_description()->streams()) {
        for (uint32_t ssrc : stream.ssrcs) {
          auto result = used_ssrcs.insert(ssrc);
          if (!result.second) {
            LOG_AND_RETURN_ERROR(
                RTCErrorType::INVALID_PARAMETER,
                "Duplicate ssrc " + rtc::ToString(ssrc) + " is not allowed");
          }
        }
      }
    }
  }

  if (IsUnifiedPlan()) {
    if (ConfiguredForMedia()) {

      for (const auto& transceiver_ext : transceivers()->List()) {
        auto transceiver = transceiver_ext->internal();
        if (transceiver->stopped()) {
          continue;
        }
        const ContentInfo* content =
            FindMediaSectionForTransceiver(transceiver, local_description());
        if (!content) {
          continue;
        }
        cricket::ChannelInterface* channel = transceiver->channel();
        if (content->rejected || !channel || channel->local_streams().empty()) {

          transceiver->sender_internal()->SetSsrc(0);
        } else {

          const std::vector<StreamParams>& streams = channel->local_streams();
          transceiver->sender_internal()->set_stream_ids(
              streams[0].stream_ids());
          auto encodings =
              transceiver->sender_internal()->init_send_encodings();
          transceiver->sender_internal()->SetSsrc(streams[0].first_ssrc());
          if (!encodings.empty()) {
            transceivers()
                ->StableState(transceiver_ext)
                ->SetInitSendEncodings(encodings);
          }
        }
      }
    }
  } else {

    const cricket::ContentInfo* audio_content =
        GetFirstAudioContent(local_description()->description());
    if (audio_content) {
      if (audio_content->rejected) {
        RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
      } else {
        const cricket::MediaContentDescription* audio_desc =
            audio_content->media_description();
        UpdateLocalSenders(audio_desc->streams(), audio_desc->type());
      }
    }

    const cricket::ContentInfo* video_content =
        GetFirstVideoContent(local_description()->description());
    if (video_content) {
      if (video_content->rejected) {
        RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
      } else {
        const cricket::MediaContentDescription* video_desc =
            video_content->media_description();
        UpdateLocalSenders(video_desc->streams(), video_desc->type());
      }
    }
  }

  if (type == SdpType::kAnswer &&
      local_ice_credentials_to_replace_->SatisfiesIceRestart(
          *current_local_description_)) {
    local_ice_credentials_to_replace_->ClearIceCredentials();
  }

  if (IsUnifiedPlan()) {
    UpdateRtpHeaderExtensionPreferencesFromSdpMunging(
        local_description()->description(), transceivers());
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandler::SetRemoteDescription(
    SetSessionDescriptionObserver* observer,
    SessionDescriptionInterface* desc_ptr) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<SetSessionDescriptionObserver>(observer),
       desc = std::unique_ptr<SessionDescriptionInterface>(desc_ptr)](
          std::function<void()> operations_chain_callback) mutable {

        if (!this_weak_ptr) {

          operations_chain_callback();
          return;
        }

        this_weak_ptr->DoSetRemoteDescription(
            std::make_unique<RemoteDescriptionOperation>(
                this_weak_ptr.get(), std::move(desc),
                rtc::make_ref_counted<SetSessionDescriptionObserverAdapter>(
                    this_weak_ptr, observer_refptr),
                std::move(operations_chain_callback)));
      });
}

void SdpOfferAnswerHandler::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(), observer,
       desc = std::move(desc)](
          std::function<void()> operations_chain_callback) mutable {
        if (!observer) {
          RTC_DLOG(LS_ERROR) << "SetRemoteDescription - observer is NULL.";
          operations_chain_callback();
          return;
        }

        if (!this_weak_ptr) {
          observer->OnSetRemoteDescriptionComplete(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "SetRemoteDescription failed because the session was shut down"));
          operations_chain_callback();
          return;
        }

        this_weak_ptr->DoSetRemoteDescription(
            std::make_unique<RemoteDescriptionOperation>(
                this_weak_ptr.get(), std::move(desc), std::move(observer),
                std::move(operations_chain_callback)));
      });
}

RTCError SdpOfferAnswerHandler::ReplaceRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    SdpType sdp_type,
    std::unique_ptr<SessionDescriptionInterface>* replaced_description) {
  RTC_DCHECK(replaced_description);
  if (sdp_type == SdpType::kAnswer) {
    *replaced_description = pending_remote_description_
                                ? std::move(pending_remote_description_)
                                : std::move(current_remote_description_);
    current_remote_description_ = std::move(desc);
    pending_remote_description_ = nullptr;
    current_local_description_ = std::move(pending_local_description_);
  } else {
    *replaced_description = std::move(pending_remote_description_);
    pending_remote_description_ = std::move(desc);
  }

  const cricket::SessionDescription* session_desc =
      remote_description()->description();

  const auto* local = local_description();

  return transport_controller_s()->SetRemoteDescription(
      sdp_type, local ? local->description() : nullptr, session_desc);
}

void SdpOfferAnswerHandler::ApplyRemoteDescription(
    std::unique_ptr<RemoteDescriptionOperation> operation) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::ApplyRemoteDescription");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(operation->description());

  pc_->ClearStatsCache();

  if (!operation->ReplaceRemoteDescriptionAndCheckError())
    return;

  if (!operation->UpdateChannels())
    return;

  if (!operation->UpdateSessionState())
    return;

  if (!operation->UseCandidatesInRemoteDescription())
    return;

  if (operation->old_remote_description()) {
    for (const cricket::ContentInfo& content :
         operation->old_remote_description()->description()->contents()) {

      if (CheckForRemoteIceRestart(operation->old_remote_description(),
                                   remote_description(), content.name)) {
        if (operation->type() == SdpType::kOffer) {
          pending_ice_restarts_.insert(content.name);
        }
      } else {

        WebRtcSessionDescriptionFactory::CopyCandidatesFromSessionDescription(
            operation->old_remote_description(), content.name,
            mutable_remote_description());
      }
    }
  }

  if (operation->HaveSessionError())
    return;

  if (remote_description()->GetType() != SdpType::kOffer &&
      remote_description()->number_of_mediasections() > 0u &&
      pc_->ice_connection_state_internal() ==
          PeerConnectionInterface::kIceConnectionNew) {
    pc_->SetIceConnectionState(PeerConnectionInterface::kIceConnectionChecking);
  }

  AllocateSctpSids();

  if (operation->unified_plan()) {
    ApplyRemoteDescriptionUpdateTransceiverState(operation->type());
  }
  remote_peer_supports_msid_ =
      remote_description()->description()->msid_signaling() !=
      cricket::kMsidSignalingNotUsed;

  if (!operation->unified_plan()) {
    PlanBUpdateSendersAndReceivers(
        GetFirstAudioContent(remote_description()->description()),
        GetFirstAudioContentDescription(remote_description()->description()),
        GetFirstVideoContent(remote_description()->description()),
        GetFirstVideoContentDescription(remote_description()->description()));
  }

  if (operation->type() == SdpType::kAnswer) {
    if (local_ice_credentials_to_replace_->SatisfiesIceRestart(
            *current_local_description_)) {
      local_ice_credentials_to_replace_->ClearIceCredentials();
    }

    RemoveStoppedTransceivers();
  }

  operation->SignalCompletion();

  SetRemoteDescriptionPostProcess(operation->type() == SdpType::kAnswer);
}

void SdpOfferAnswerHandler::ApplyRemoteDescriptionUpdateTransceiverState(
    SdpType sdp_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  if (!ConfiguredForMedia()) {
    return;
  }
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
      now_receiving_transceivers;
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>> remove_list;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
  for (const auto& transceiver_ext : transceivers()->List()) {
    const auto transceiver = transceiver_ext->internal();
    const ContentInfo* content =
        FindMediaSectionForTransceiver(transceiver, remote_description());
    if (!content) {
      continue;
    }
    const MediaContentDescription* media_desc = content->media_description();
    RtpTransceiverDirection local_direction =
        RtpTransceiverDirectionReversed(media_desc->direction());

    if (sdp_type == SdpType::kOffer) {
      transceivers()
          ->StableState(transceiver_ext)
          ->SetRemoteStreamIds(transceiver->receiver()->stream_ids());
    }

    if (RtpTransceiverDirectionHasRecv(local_direction)) {
      std::vector<std::string> stream_ids;
      if (!media_desc->streams().empty()) {

        stream_ids = media_desc->streams()[0].stream_ids();
      }

      RTC_LOG(LS_INFO) << "Processing the MSIDs for MID=" << content->name
                       << " (" << GetStreamIdsString(stream_ids) << ").";
      SetAssociatedRemoteStreams(transceiver->receiver_internal(), stream_ids,
                                 &added_streams, &removed_streams);

      if (!transceiver->fired_direction() ||
          !RtpTransceiverDirectionHasRecv(*transceiver->fired_direction())) {
        RTC_LOG(LS_INFO) << "Processing the addition of a remote track for MID="
                         << content->name << ".";

        now_receiving_transceivers.push_back(transceiver_ext);
      }
    }

    if (!RtpTransceiverDirectionHasRecv(local_direction) &&
        (transceiver->fired_direction() &&
         RtpTransceiverDirectionHasRecv(*transceiver->fired_direction()))) {
      ProcessRemovalOfRemoteTrack(transceiver_ext, &remove_list,
                                  &removed_streams);
    }

    if (sdp_type == SdpType::kOffer) {

      transceivers()
          ->StableState(transceiver_ext)
          ->SetFiredDirection(transceiver->fired_direction());
    }
    transceiver->set_fired_direction(local_direction);

    if (sdp_type == SdpType::kPrAnswer || sdp_type == SdpType::kAnswer) {

      transceiver->set_current_direction(local_direction);

      if (transceiver->mid()) {
        auto dtls_transport = LookupDtlsTransportByMid(
            context_->network_thread(), transport_controller_s(),
            *transceiver->mid());
        transceiver->sender_internal()->set_transport(dtls_transport);
        transceiver->receiver_internal()->set_transport(dtls_transport);
      }
    }

    if (content->rejected && !transceiver->stopped()) {
      RTC_LOG(LS_INFO) << "Stopping transceiver for MID=" << content->name
                       << " since the media section was rejected.";
      transceiver->StopTransceiverProcedure();
    }
    if (!content->rejected && RtpTransceiverDirectionHasRecv(local_direction)) {
      if (!media_desc->streams().empty() &&
          media_desc->streams()[0].has_ssrcs()) {
        uint32_t ssrc = media_desc->streams()[0].first_ssrc();
        transceiver->receiver_internal()->SetupMediaChannel(ssrc);
      } else {
        transceiver->receiver_internal()->SetupUnsignaledMediaChannel();
      }
    }
  }

  auto observer = pc_->Observer();
  for (const auto& transceiver : now_receiving_transceivers) {
    pc_->legacy_stats()->AddTrack(transceiver->receiver()->track().get());
    observer->OnTrack(transceiver);
    observer->OnAddTrack(transceiver->receiver(),
                         transceiver->receiver()->streams());
  }
  for (const auto& stream : added_streams) {
    observer->OnAddStream(stream);
  }
  for (const auto& transceiver : remove_list) {
    observer->OnRemoveTrack(transceiver->receiver());
  }
  for (const auto& stream : removed_streams) {
    observer->OnRemoveStream(stream);
  }
}

void SdpOfferAnswerHandler::PlanBUpdateSendersAndReceivers(
    const cricket::ContentInfo* audio_content,
    const cricket::AudioContentDescription* audio_desc,
    const cricket::ContentInfo* video_content,
    const cricket::VideoContentDescription* video_desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsUnifiedPlan());

  rtc::scoped_refptr<StreamCollection> new_streams(StreamCollection::Create());

  if (audio_content) {
    if (audio_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_AUDIO);
    } else {
      bool default_audio_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(audio_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(audio_desc),
                              default_audio_track_needed, audio_desc->type(),
                              new_streams.get());
    }
  }

  if (video_content) {
    if (video_content->rejected) {
      RemoveSenders(cricket::MEDIA_TYPE_VIDEO);
    } else {
      bool default_video_track_needed =
          !remote_peer_supports_msid_ &&
          RtpTransceiverDirectionHasSend(video_desc->direction());
      UpdateRemoteSendersList(GetActiveStreams(video_desc),
                              default_video_track_needed, video_desc->type(),
                              new_streams.get());
    }
  }

  auto observer = pc_->Observer();
  for (size_t i = 0; i < new_streams->count(); ++i) {
    MediaStreamInterface* new_stream = new_streams->at(i);
    pc_->legacy_stats()->AddStream(new_stream);
    observer->OnAddStream(rtc::scoped_refptr<MediaStreamInterface>(new_stream));
  }

  UpdateEndedRemoteMediaStreams();
}

void SdpOfferAnswerHandler::DoSetLocalDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    rtc::scoped_refptr<SetLocalDescriptionObserverInterface> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoSetLocalDescription");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "SetLocalDescription - observer is NULL.";
    return;
  }

  if (!desc) {
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, "SessionDescription is NULL."));
    return;
  }

  if (session_error() != SessionError::kNone) {
    std::string error_message = GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "SetLocalDescription: " << error_message;
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (desc->GetType() == SdpType::kRollback) {
    if (IsUnifiedPlan()) {
      observer->OnSetLocalDescriptionComplete(Rollback(desc->GetType()));
    } else {
      observer->OnSetLocalDescriptionComplete(
          RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                   "Rollback not supported in Plan B"));
    }
    return;
  }

  std::map<std::string, const cricket::ContentGroup*> bundle_groups_by_mid =
      GetBundleGroupsByMid(desc->description());
  RTCError error = ValidateSessionDescription(desc.get(), cricket::CS_LOCAL,
                                              bundle_groups_by_mid);
  if (!error.ok()) {
    std::string error_message = GetSetDescriptionErrorMessage(
        cricket::CS_LOCAL, desc->GetType(), error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetLocalDescriptionComplete(
        RTCError(error.type(), std::move(error_message)));
    return;
  }

  const SdpType type = desc->GetType();

  error = ApplyLocalDescription(std::move(desc), bundle_groups_by_mid);

  if (!error.ok()) {

    SetSessionError(SessionError::kContent, error.message());
    std::string error_message =
        GetSetDescriptionErrorMessage(cricket::CS_LOCAL, type, error);
    RTC_LOG(LS_ERROR) << error_message;
    observer->OnSetLocalDescriptionComplete(
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }
  RTC_DCHECK(local_description());

  if (local_description()->GetType() == SdpType::kAnswer) {
    RemoveStoppedTransceivers();

    context_->network_thread()->BlockingCall(
        [this] { port_allocator()->DiscardCandidatePool(); });
  }

  observer->OnSetLocalDescriptionComplete(RTCError::OK());
  pc_->NoteUsageEvent(UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED);

  if (IsUnifiedPlan()) {
    bool was_negotiation_needed = is_negotiation_needed_;
    UpdateNegotiationNeeded();
    if (signaling_state() == PeerConnectionInterface::kStable &&
        was_negotiation_needed && is_negotiation_needed_) {

      pc_->Observer()->OnRenegotiationNeeded();

      GenerateNegotiationNeededEvent();
    }
  }

  transport_controller_s()->MaybeStartGathering();
}

void SdpOfferAnswerHandler::DoCreateOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoCreateOffer");

  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateOffer - observer is NULL.";
    return;
  }

  if (pc_->IsClosed()) {
    std::string error = "CreateOffer called when PeerConnection is closed.";
    RTC_LOG(LS_ERROR) << error;
    pc_->message_handler()->PostCreateSessionDescriptionFailure(
        observer.get(),
        RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  if (session_error() != SessionError::kNone) {
    std::string error_message = GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "CreateOffer: " << error_message;
    pc_->message_handler()->PostCreateSessionDescriptionFailure(
        observer.get(),
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (!ValidateOfferAnswerOptions(options)) {
    std::string error = "CreateOffer called with invalid options.";
    RTC_LOG(LS_ERROR) << error;
    pc_->message_handler()->PostCreateSessionDescriptionFailure(
        observer.get(),
        RTCError(RTCErrorType::INVALID_PARAMETER, std::move(error)));
    return;
  }

  if (IsUnifiedPlan()) {
    RTCError error = HandleLegacyOfferOptions(options);
    if (!error.ok()) {
      pc_->message_handler()->PostCreateSessionDescriptionFailure(
          observer.get(), std::move(error));
      return;
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForOffer(options, &session_options);
  webrtc_session_desc_factory_->CreateOffer(observer.get(), options,
                                            session_options);
}

void SdpOfferAnswerHandler::CreateAnswer(
    CreateSessionDescriptionObserver* observer,
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::CreateAnswer");
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       observer_refptr =
           rtc::scoped_refptr<CreateSessionDescriptionObserver>(observer),
       options](std::function<void()> operations_chain_callback) {

        if (!this_weak_ptr) {
          observer_refptr->OnFailure(RTCError(
              RTCErrorType::INTERNAL_ERROR,
              "CreateAnswer failed because the session was shut down"));
          operations_chain_callback();
          return;
        }

        auto observer_wrapper = rtc::make_ref_counted<
            CreateSessionDescriptionObserverOperationWrapper>(
            std::move(observer_refptr), std::move(operations_chain_callback));
        this_weak_ptr->DoCreateAnswer(options, observer_wrapper);
      });
}

void SdpOfferAnswerHandler::DoCreateAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options,
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoCreateAnswer");
  if (!observer) {
    RTC_LOG(LS_ERROR) << "CreateAnswer - observer is NULL.";
    return;
  }

  if (session_error() != SessionError::kNone) {
    std::string error_message = GetSessionErrorMsg();
    RTC_LOG(LS_ERROR) << "CreateAnswer: " << error_message;
    pc_->message_handler()->PostCreateSessionDescriptionFailure(
        observer.get(),
        RTCError(RTCErrorType::INTERNAL_ERROR, std::move(error_message)));
    return;
  }

  if (!(signaling_state_ == PeerConnectionInterface::kHaveRemoteOffer ||
        signaling_state_ == PeerConnectionInterface::kHaveLocalPrAnswer)) {
    std::string error =
        "PeerConnection cannot create an answer in a state other than "
        "have-remote-offer or have-local-pranswer.";
    RTC_LOG(LS_ERROR) << error;
    pc_->message_handler()->PostCreateSessionDescriptionFailure(
        observer.get(),
        RTCError(RTCErrorType::INVALID_STATE, std::move(error)));
    return;
  }

  RTC_DCHECK(remote_description());

  if (IsUnifiedPlan()) {
    if (options.offer_to_receive_audio !=
        PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_audio is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
    if (options.offer_to_receive_video !=
        PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
      RTC_LOG(LS_WARNING) << "CreateAnswer: offer_to_receive_video is not "
                             "supported with Unified Plan semantics. Use the "
                             "RtpTransceiver API instead.";
    }
  }

  cricket::MediaSessionOptions session_options;
  GetOptionsForAnswer(options, &session_options);
  webrtc_session_desc_factory_->CreateAnswer(observer.get(), session_options);
}

void SdpOfferAnswerHandler::DoSetRemoteDescription(
    std::unique_ptr<RemoteDescriptionOperation> operation) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::DoSetRemoteDescription");

  if (!operation->ok())
    return;

  if (operation->HaveSessionError())
    return;

  if (operation->MaybeRollback())
    return;

  operation->ReportOfferAnswerUma();

  FillInMissingRemoteMids(operation->description());
  if (!operation->IsDescriptionValid())
    return;

  ApplyRemoteDescription(std::move(operation));
}

void SdpOfferAnswerHandler::SetRemoteDescriptionPostProcess(bool was_answer) {
  RTC_DCHECK(remote_description());

  if (was_answer) {

    context_->network_thread()->BlockingCall(
        [this] { port_allocator()->DiscardCandidatePool(); });
  }

  pc_->NoteUsageEvent(UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED);

  if (IsUnifiedPlan()) {
    bool was_negotiation_needed = is_negotiation_needed_;
    UpdateNegotiationNeeded();
    if (signaling_state() == PeerConnectionInterface::kStable &&
        was_negotiation_needed && is_negotiation_needed_) {

      pc_->Observer()->OnRenegotiationNeeded();

      GenerateNegotiationNeededEvent();
    }
  }
}

void SdpOfferAnswerHandler::SetAssociatedRemoteStreams(
    rtc::scoped_refptr<RtpReceiverInternal> receiver,
    const std::vector<std::string>& stream_ids,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* added_streams,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> media_streams;
  for (const std::string& stream_id : stream_ids) {
    rtc::scoped_refptr<MediaStreamInterface> stream(
        remote_streams_->find(stream_id));
    if (!stream) {
      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_id));
      remote_streams_->AddStream(stream);
      added_streams->push_back(stream);
    }
    media_streams.push_back(stream);
  }

  if (media_streams.empty() &&
      !(remote_description()->description()->msid_signaling() &
        cricket::kMsidSignalingMediaSection)) {
    if (!missing_msid_default_stream_) {
      missing_msid_default_stream_ = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(rtc::CreateRandomUuid()));
      added_streams->push_back(missing_msid_default_stream_);
    }
    media_streams.push_back(missing_msid_default_stream_);
  }
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> previous_streams =
      receiver->streams();

  receiver->SetStreams(media_streams);
  RemoveRemoteStreamsIfEmpty(previous_streams, removed_streams);
}

bool SdpOfferAnswerHandler::AddIceCandidate(
    const IceCandidateInterface* ice_candidate) {
  const AddIceCandidateResult result = AddIceCandidateInternal(ice_candidate);
  NoteAddIceCandidateResult(result);

  return result == kAddIceCandidateSuccess ||
         result == kAddIceCandidateFailNotReady;
}

AddIceCandidateResult SdpOfferAnswerHandler::AddIceCandidateInternal(
    const IceCandidateInterface* ice_candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::AddIceCandidate");
  if (pc_->IsClosed()) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: PeerConnection is closed.";
    return kAddIceCandidateFailClosed;
  }

  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: ICE candidates can't be added "
                         "without any remote session description.";
    return kAddIceCandidateFailNoRemoteDescription;
  }

  if (!ice_candidate) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: Candidate is null.";
    return kAddIceCandidateFailNullCandidate;
  }

  bool valid = false;
  bool ready = ReadyToUseRemoteCandidate(ice_candidate, nullptr, &valid);
  if (!valid) {
    return kAddIceCandidateFailNotValid;
  }

  if (!mutable_remote_description()->AddCandidate(ice_candidate)) {
    RTC_LOG(LS_ERROR) << "AddIceCandidate: Candidate cannot be used.";
    return kAddIceCandidateFailInAddition;
  }

  if (!ready) {
    RTC_LOG(LS_INFO) << "AddIceCandidate: Not ready to use candidate.";
    return kAddIceCandidateFailNotReady;
  }

  if (!UseCandidate(ice_candidate)) {
    return kAddIceCandidateFailNotUsable;
  }

  pc_->NoteUsageEvent(UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED);

  return kAddIceCandidateSuccess;
}

void SdpOfferAnswerHandler::AddIceCandidate(
    std::unique_ptr<IceCandidateInterface> candidate,
    std::function<void(RTCError)> callback) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::AddIceCandidate");
  RTC_DCHECK_RUN_ON(signaling_thread());

  operations_chain_->ChainOperation(
      [this_weak_ptr = weak_ptr_factory_.GetWeakPtr(),
       candidate = std::move(candidate), callback = std::move(callback)](
          std::function<void()> operations_chain_callback) {
        auto result =
            this_weak_ptr
                ? this_weak_ptr->AddIceCandidateInternal(candidate.get())
                : kAddIceCandidateFailClosed;
        NoteAddIceCandidateResult(result);
        operations_chain_callback();
        switch (result) {
          case AddIceCandidateResult::kAddIceCandidateSuccess:
          case AddIceCandidateResult::kAddIceCandidateFailNotReady:

            callback(RTCError::OK());
            break;
          case AddIceCandidateResult::kAddIceCandidateFailClosed:

            callback(RTCError(
                RTCErrorType::INVALID_STATE,
                "AddIceCandidate failed because the session was shut down"));
            break;
          case AddIceCandidateResult::kAddIceCandidateFailNoRemoteDescription:

            callback(RTCError(RTCErrorType::INVALID_STATE,
                              "The remote description was null"));
            break;
          case AddIceCandidateResult::kAddIceCandidateFailNullCandidate:

            callback(RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                              "Error processing ICE candidate"));
            break;
          case AddIceCandidateResult::kAddIceCandidateFailNotValid:
          case AddIceCandidateResult::kAddIceCandidateFailInAddition:
          case AddIceCandidateResult::kAddIceCandidateFailNotUsable:

            callback(RTCError(RTCErrorType::UNSUPPORTED_OPERATION,
                              "Error processing ICE candidate"));
            break;
          default:
            RTC_DCHECK_NOTREACHED();
        }
      });
}

bool SdpOfferAnswerHandler::RemoveIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::RemoveIceCandidates");
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: PeerConnection is closed.";
    return false;
  }

  if (!remote_description()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: ICE candidates can't be removed "
                         "without any remote session description.";
    return false;
  }

  if (candidates.empty()) {
    RTC_LOG(LS_ERROR) << "RemoveIceCandidates: candidates are empty.";
    return false;
  }

  size_t number_removed =
      mutable_remote_description()->RemoveCandidates(candidates);
  if (number_removed != candidates.size()) {
    RTC_LOG(LS_ERROR)
        << "RemoveIceCandidates: Failed to remove candidates. Requested "
        << candidates.size() << " but only " << number_removed
        << " are removed.";
  }

  RTCError error = transport_controller_s()->RemoveRemoteCandidates(candidates);
  if (!error.ok()) {
    RTC_LOG(LS_ERROR)
        << "RemoveIceCandidates: Error when removing remote candidates: "
        << error.message();
  }
  return true;
}

void SdpOfferAnswerHandler::AddLocalIceCandidate(
    const JsepIceCandidate* candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (local_description()) {
    mutable_local_description()->AddCandidate(candidate);
  }
}

void SdpOfferAnswerHandler::RemoveLocalIceCandidates(
    const std::vector<cricket::Candidate>& candidates) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (local_description()) {
    mutable_local_description()->RemoveCandidates(candidates);
  }
}

const SessionDescriptionInterface* SdpOfferAnswerHandler::local_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_local_description_ ? pending_local_description_.get()
                                    : current_local_description_.get();
}

const SessionDescriptionInterface* SdpOfferAnswerHandler::remote_description()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_remote_description_ ? pending_remote_description_.get()
                                     : current_remote_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::current_local_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return current_local_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::current_remote_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return current_remote_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::pending_local_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_local_description_.get();
}

const SessionDescriptionInterface*
SdpOfferAnswerHandler::pending_remote_description() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_remote_description_.get();
}

PeerConnectionInterface::SignalingState SdpOfferAnswerHandler::signaling_state()
    const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return signaling_state_;
}

void SdpOfferAnswerHandler::ChangeSignalingState(
    PeerConnectionInterface::SignalingState signaling_state) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::ChangeSignalingState");
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (signaling_state_ == signaling_state) {
    return;
  }
  RTC_LOG(LS_INFO) << "Session: " << pc_->session_id() << " Old state: "
                   << PeerConnectionInterface::AsString(signaling_state_)
                   << " New state: "
                   << PeerConnectionInterface::AsString(signaling_state);
  signaling_state_ = signaling_state;
  pc_->Observer()->OnSignalingChange(signaling_state_);
}

RTCError SdpOfferAnswerHandler::UpdateSessionState(
    SdpType type,
    cricket::ContentSource source,
    const cricket::SessionDescription* description,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  RTC_DCHECK(session_error() == SessionError::kNone);

  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    EnableSending();
  }

  if (type == SdpType::kOffer) {
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalOffer
                             : PeerConnectionInterface::kHaveRemoteOffer);
  } else if (type == SdpType::kPrAnswer) {
    ChangeSignalingState(source == cricket::CS_LOCAL
                             ? PeerConnectionInterface::kHaveLocalPrAnswer
                             : PeerConnectionInterface::kHaveRemotePrAnswer);
  } else {
    RTC_DCHECK(type == SdpType::kAnswer);
    ChangeSignalingState(PeerConnectionInterface::kStable);
    if (ConfiguredForMedia()) {
      transceivers()->DiscardStableStates();
    }
  }

  return PushdownMediaDescription(type, source, bundle_groups_by_mid);
}

bool SdpOfferAnswerHandler::ShouldFireNegotiationNeededEvent(
    uint32_t event_id) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (!IsUnifiedPlan()) {
    return true;
  }

  if (event_id != negotiation_needed_event_id_) {
    return false;
  }

  if (!operations_chain_->IsEmpty()) {

    is_negotiation_needed_ = false;
    update_negotiation_needed_on_empty_chain_ = true;
    return false;
  }

  if (signaling_state_ != PeerConnectionInterface::kStable) {
    return false;
  }

  return true;
}

rtc::scoped_refptr<StreamCollectionInterface>
SdpOfferAnswerHandler::local_streams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "local_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetSenders "
                                 "instead.";
  return local_streams_;
}

rtc::scoped_refptr<StreamCollectionInterface>
SdpOfferAnswerHandler::remote_streams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "remote_streams is not available with Unified "
                                 "Plan SdpSemantics. Please use GetReceivers "
                                 "instead.";
  return remote_streams_;
}

bool SdpOfferAnswerHandler::AddStream(MediaStreamInterface* local_stream) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "AddStream is not available with Unified Plan "
                                 "SdpSemantics. Please use AddTrack instead.";
  if (pc_->IsClosed()) {
    return false;
  }
  if (!CanAddLocalMediaStream(local_streams_.get(), local_stream)) {
    return false;
  }

  local_streams_->AddStream(
      rtc::scoped_refptr<MediaStreamInterface>(local_stream));
  auto observer = std::make_unique<MediaStreamObserver>(
      local_stream,
      [this](AudioTrackInterface* audio_track,
             MediaStreamInterface* media_stream) {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnAudioTrackAdded(audio_track, media_stream);
      },
      [this](AudioTrackInterface* audio_track,
             MediaStreamInterface* media_stream) {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnAudioTrackRemoved(audio_track, media_stream);
      },
      [this](VideoTrackInterface* video_track,
             MediaStreamInterface* media_stream) {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnVideoTrackAdded(video_track, media_stream);
      },
      [this](VideoTrackInterface* video_track,
             MediaStreamInterface* media_stream) {
        RTC_DCHECK_RUN_ON(signaling_thread());
        OnVideoTrackRemoved(video_track, media_stream);
      });
  stream_observers_.push_back(std::move(observer));

  for (const auto& track : local_stream->GetAudioTracks()) {
    rtp_manager()->AddAudioTrack(track.get(), local_stream);
  }
  for (const auto& track : local_stream->GetVideoTracks()) {
    rtp_manager()->AddVideoTrack(track.get(), local_stream);
  }

  pc_->legacy_stats()->AddStream(local_stream);
  UpdateNegotiationNeeded();
  return true;
}

void SdpOfferAnswerHandler::RemoveStream(MediaStreamInterface* local_stream) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_CHECK(!IsUnifiedPlan()) << "RemoveStream is not available with Unified "
                                 "Plan SdpSemantics. Please use RemoveTrack "
                                 "instead.";
  TRACE_EVENT0("webrtc", "PeerConnection::RemoveStream");
  if (!pc_->IsClosed()) {
    for (const auto& track : local_stream->GetAudioTracks()) {
      rtp_manager()->RemoveAudioTrack(track.get(), local_stream);
    }
    for (const auto& track : local_stream->GetVideoTracks()) {
      rtp_manager()->RemoveVideoTrack(track.get(), local_stream);
    }
  }
  local_streams_->RemoveStream(local_stream);
  stream_observers_.erase(
      std::remove_if(
          stream_observers_.begin(), stream_observers_.end(),
          [local_stream](const std::unique_ptr<MediaStreamObserver>& observer) {
            return observer->stream()->id().compare(local_stream->id()) == 0;
          }),
      stream_observers_.end());

  if (pc_->IsClosed()) {
    return;
  }
  UpdateNegotiationNeeded();
}

void SdpOfferAnswerHandler::OnAudioTrackAdded(AudioTrackInterface* track,
                                              MediaStreamInterface* stream) {
  if (pc_->IsClosed()) {
    return;
  }
  rtp_manager()->AddAudioTrack(track, stream);
  UpdateNegotiationNeeded();
}

void SdpOfferAnswerHandler::OnAudioTrackRemoved(AudioTrackInterface* track,
                                                MediaStreamInterface* stream) {
  if (pc_->IsClosed()) {
    return;
  }
  rtp_manager()->RemoveAudioTrack(track, stream);
  UpdateNegotiationNeeded();
}

void SdpOfferAnswerHandler::OnVideoTrackAdded(VideoTrackInterface* track,
                                              MediaStreamInterface* stream) {
  if (pc_->IsClosed()) {
    return;
  }
  rtp_manager()->AddVideoTrack(track, stream);
  UpdateNegotiationNeeded();
}

void SdpOfferAnswerHandler::OnVideoTrackRemoved(VideoTrackInterface* track,
                                                MediaStreamInterface* stream) {
  if (pc_->IsClosed()) {
    return;
  }
  rtp_manager()->RemoveVideoTrack(track, stream);
  UpdateNegotiationNeeded();
}

RTCError SdpOfferAnswerHandler::Rollback(SdpType desc_type) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::Rollback");
  auto state = signaling_state();
  if (state != PeerConnectionInterface::kHaveLocalOffer &&
      state != PeerConnectionInterface::kHaveRemoteOffer) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_STATE,
        (rtc::StringBuilder("Called in wrong signalingState: ")
         << (PeerConnectionInterface::AsString(signaling_state())))
            .Release());
  }
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());
  std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>
      now_receiving_transceivers;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_added_streams;
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> all_removed_streams;
  std::vector<rtc::scoped_refptr<RtpReceiverInterface>> removed_receivers;

  for (auto&& transceivers_stable_state_pair : transceivers()->StableStates()) {
    auto transceiver = transceivers_stable_state_pair.first;
    auto state = transceivers_stable_state_pair.second;

    if (state.did_set_fired_direction()) {

      bool previously_fired_direction_is_recv =
          transceiver->fired_direction().has_value() &&
          RtpTransceiverDirectionHasRecv(*transceiver->fired_direction());
      bool currently_fired_direction_is_recv =
          state.fired_direction().has_value() &&
          RtpTransceiverDirectionHasRecv(state.fired_direction().value());
      if (!previously_fired_direction_is_recv &&
          currently_fired_direction_is_recv) {
        now_receiving_transceivers.push_back(transceiver);
      }
      transceiver->internal()->set_fired_direction(state.fired_direction());
    }

    if (state.remote_stream_ids()) {
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> added_streams;
      std::vector<rtc::scoped_refptr<MediaStreamInterface>> removed_streams;
      SetAssociatedRemoteStreams(transceiver->internal()->receiver_internal(),
                                 state.remote_stream_ids().value(),
                                 &added_streams, &removed_streams);
      all_added_streams.insert(all_added_streams.end(), added_streams.begin(),
                               added_streams.end());
      all_removed_streams.insert(all_removed_streams.end(),
                                 removed_streams.begin(),
                                 removed_streams.end());
      if (!state.has_m_section() && !state.newly_created()) {
        continue;
      }
    }

    RTC_DCHECK(transceiver->internal()->mid().has_value());
    transceiver->internal()->ClearChannel();

    if (signaling_state() == PeerConnectionInterface::kHaveRemoteOffer &&
        transceiver->receiver()) {
      removed_receivers.push_back(transceiver->receiver());
    }
    if (state.newly_created()) {
      if (transceiver->internal()->reused_for_addtrack()) {
        transceiver->internal()->set_created_by_addtrack(true);
      } else {
        transceiver->internal()->StopTransceiverProcedure();
        transceivers()->Remove(transceiver);
      }
    }
    if (state.init_send_encodings()) {
      transceiver->internal()->sender_internal()->set_init_send_encodings(
          state.init_send_encodings().value());
    }
    transceiver->internal()->sender_internal()->set_transport(nullptr);
    transceiver->internal()->receiver_internal()->set_transport(nullptr);
    if (state.has_m_section()) {
      transceiver->internal()->set_mid(state.mid());
      transceiver->internal()->set_mline_index(state.mline_index());
    }
  }
  RTCError e = transport_controller_s()->RollbackTransports();
  if (!e.ok()) {
    return e;
  }
  transceivers()->DiscardStableStates();
  pending_local_description_.reset();
  pending_remote_description_.reset();
  ChangeSignalingState(PeerConnectionInterface::kStable);

  for (const auto& transceiver : now_receiving_transceivers) {
    pc_->Observer()->OnTrack(transceiver);
    pc_->Observer()->OnAddTrack(transceiver->receiver(),
                                transceiver->receiver()->streams());
  }
  for (const auto& receiver : removed_receivers) {
    pc_->Observer()->OnRemoveTrack(receiver);
  }
  for (const auto& stream : all_added_streams) {
    pc_->Observer()->OnAddStream(stream);
  }
  for (const auto& stream : all_removed_streams) {
    pc_->Observer()->OnRemoveStream(stream);
  }

  if (desc_type == SdpType::kRollback) {
    UpdateNegotiationNeeded();
    if (is_negotiation_needed_) {

      pc_->Observer()->OnRenegotiationNeeded();

      GenerateNegotiationNeededEvent();
    }
  }
  return RTCError::OK();
}

bool SdpOfferAnswerHandler::IsUnifiedPlan() const {
  return pc_->IsUnifiedPlan();
}

void SdpOfferAnswerHandler::OnOperationsChainEmpty() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (pc_->IsClosed() || !update_negotiation_needed_on_empty_chain_)
    return;
  update_negotiation_needed_on_empty_chain_ = false;

  if (IsUnifiedPlan()) {
    UpdateNegotiationNeeded();
  }
}

absl::optional<bool> SdpOfferAnswerHandler::is_caller() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return is_caller_;
}

bool SdpOfferAnswerHandler::HasNewIceCredentials() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return local_ice_credentials_to_replace_->HasIceCredentials();
}

bool SdpOfferAnswerHandler::IceRestartPending(
    const std::string& content_name) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return pending_ice_restarts_.find(content_name) !=
         pending_ice_restarts_.end();
}

bool SdpOfferAnswerHandler::NeedsIceRestart(
    const std::string& content_name) const {
  return pc_->NeedsIceRestart(content_name);
}

absl::optional<rtc::SSLRole> SdpOfferAnswerHandler::GetDtlsRole(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  return transport_controller_s()->GetDtlsRole(mid);
}

void SdpOfferAnswerHandler::UpdateNegotiationNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!IsUnifiedPlan()) {
    pc_->Observer()->OnRenegotiationNeeded();
    GenerateNegotiationNeededEvent();
    return;
  }

  if (pc_->IsClosed())
    return;

  if (signaling_state() != PeerConnectionInterface::kStable)
    return;

  bool is_negotiation_needed = CheckIfNegotiationIsNeeded();
  if (!is_negotiation_needed) {
    is_negotiation_needed_ = false;

    ++negotiation_needed_event_id_;
    return;
  }

  if (is_negotiation_needed_)
    return;

  is_negotiation_needed_ = true;

  pc_->Observer()->OnRenegotiationNeeded();

  GenerateNegotiationNeededEvent();
}

void SdpOfferAnswerHandler::AllocateSctpSids() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!local_description() || !remote_description()) {
    RTC_DLOG(LS_VERBOSE)
        << "Local and Remote descriptions must be applied to get the "
           "SSL Role of the SCTP transport.";
    return;
  }

  absl::optional<rtc::SSLRole> guessed_role = GuessSslRole();
  network_thread()->BlockingCall(
      [&, data_channel_controller = data_channel_controller()] {
        RTC_DCHECK_RUN_ON(network_thread());
        absl::optional<rtc::SSLRole> role = pc_->GetSctpSslRole_n();
        if (!role)
          role = guessed_role;
        if (role)
          data_channel_controller->AllocateSctpSids(*role);
      });
}

absl::optional<rtc::SSLRole> SdpOfferAnswerHandler::GuessSslRole() const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!pc_->sctp_mid())
    return absl::nullopt;

  return is_caller() ? rtc::SSL_SERVER : rtc::SSL_CLIENT;
}

bool SdpOfferAnswerHandler::CheckIfNegotiationIsNeeded() {
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (local_ice_credentials_to_replace_->HasIceCredentials()) {
    return true;
  }

  const SessionDescriptionInterface* description = current_local_description();
  if (!description)
    return true;

  if (data_channel_controller()->HasUsedDataChannels()) {
    const cricket::ContentInfo* data_content =
        cricket::GetFirstDataContent(description->description()->contents());
    if (!data_content) {
      return true;
    }

    const cricket::ContentInfo* remote_data_content =
        current_remote_description()
            ? current_remote_description()->description()->GetContentByName(
                  data_content->name)
            : nullptr;
    if (remote_data_content && remote_data_content->rejected) {
      return true;
    }
  }
  if (!ConfiguredForMedia()) {
    return false;
  }

  for (const auto& transceiver : transceivers()->ListInternal()) {
    const ContentInfo* current_local_msection =
        FindTransceiverMSection(transceiver, description);

    const ContentInfo* current_remote_msection =
        FindTransceiverMSection(transceiver, current_remote_description());

    if (transceiver->stopped()) {
      RTC_DCHECK(transceiver->stopping());
      if (current_local_msection && !current_local_msection->rejected &&
          ((current_remote_msection && !current_remote_msection->rejected) ||
           !current_remote_msection)) {
        return true;
      }
      continue;
    }

    if (transceiver->stopping() && !transceiver->stopped())
      return true;

    if (!current_local_msection)
      return true;

    const MediaContentDescription* current_local_media_description =
        current_local_msection->media_description();

    if (RtpTransceiverDirectionHasSend(transceiver->direction())) {
      if (current_local_media_description->streams().size() == 0)
        return true;

      std::vector<std::string> msection_msids;
      for (const auto& stream : current_local_media_description->streams()) {
        for (const std::string& msid : stream.stream_ids())
          msection_msids.push_back(msid);
      }

      std::vector<std::string> transceiver_msids =
          transceiver->sender()->stream_ids();
      if (msection_msids.size() != transceiver_msids.size())
        return true;

      absl::c_sort(transceiver_msids);
      absl::c_sort(msection_msids);
      if (transceiver_msids != msection_msids)
        return true;
    }

    if (description->GetType() == SdpType::kOffer) {
      if (!current_remote_description())
        return true;

      if (!current_remote_msection)
        return true;

      RtpTransceiverDirection current_local_direction =
          current_local_media_description->direction();
      RtpTransceiverDirection current_remote_direction =
          current_remote_msection->media_description()->direction();
      if (transceiver->direction() != current_local_direction &&
          transceiver->direction() !=
              RtpTransceiverDirectionReversed(current_remote_direction)) {
        return true;
      }
    }

    if (description->GetType() == SdpType::kAnswer) {
      if (!remote_description())
        return true;

      const ContentInfo* offered_remote_msection =
          FindTransceiverMSection(transceiver, remote_description());

      RtpTransceiverDirection offered_direction =
          offered_remote_msection
              ? offered_remote_msection->media_description()->direction()
              : RtpTransceiverDirection::kInactive;

      if (current_local_media_description->direction() !=
          (RtpTransceiverDirectionIntersection(
              transceiver->direction(),
              RtpTransceiverDirectionReversed(offered_direction)))) {
        return true;
      }
    }
  }

  return false;
}

void SdpOfferAnswerHandler::GenerateNegotiationNeededEvent() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  ++negotiation_needed_event_id_;
  pc_->Observer()->OnNegotiationNeededEvent(negotiation_needed_event_id_);
}

RTCError SdpOfferAnswerHandler::ValidateSessionDescription(
    const SessionDescriptionInterface* sdesc,
    cricket::ContentSource source,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {

  RTC_DCHECK_EQ(SessionError::kNone, session_error());

  if (!sdesc || !sdesc->description()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, kInvalidSdp);
  }

  SdpType type = sdesc->GetType();
  if ((source == cricket::CS_LOCAL && !ExpectSetLocalDescription(type)) ||
      (source == cricket::CS_REMOTE && !ExpectSetRemoteDescription(type))) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_STATE,
        (rtc::StringBuilder("Called in wrong state: ")
         << PeerConnectionInterface::AsString(signaling_state()))
            .Release());
  }

  RTCError error = ValidateMids(*sdesc->description());
  if (!error.ok()) {
    return error;
  }

  std::string crypto_error;
  if (pc_->dtls_enabled()) {
    RTCError crypto_error = VerifyCrypto(
        sdesc->description(), pc_->dtls_enabled(), bundle_groups_by_mid);
    if (!crypto_error.ok()) {
      return crypto_error;
    }
  }

  if (!VerifyIceUfragPwdPresent(sdesc->description(), bundle_groups_by_mid)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         kSdpWithoutIceUfragPwd);
  }

  error = ValidateBundledPayloadTypes(*sdesc->description());

  RTC_HISTOGRAM_BOOLEAN("WebRTC.PeerConnection.ValidBundledPayloadTypes",
                        error.ok());

  error = ValidateBundledRtpHeaderExtensions(*sdesc->description());
  if (!error.ok()) {
    return error;
  }

  error = ValidateSsrcGroups(*sdesc->description());
  if (!error.ok() &&
      !pc_->trials().IsDisabled("WebRTC-PreventSsrcGroupsWithUnexpectedSize")) {
    return error;
  }

  if (!pc_->ValidateBundleSettings(sdesc->description(),
                                   bundle_groups_by_mid)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         kBundleWithoutRtcpMux);
  }

  error = ValidatePayloadTypes(*sdesc->description());
  if (!error.ok()) {
    return error;
  }

  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {

    const cricket::SessionDescription* offer_desc =
        (source == cricket::CS_LOCAL) ? remote_description()->description()
                                      : local_description()->description();
    if (!MediaSectionsHaveSameCount(*offer_desc, *sdesc->description()) ||
        !MediaSectionsInSameOrder(*offer_desc, nullptr, *sdesc->description(),
                                  type)) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           kMlineMismatchInAnswer);
    }
  } else {

    const cricket::SessionDescription* current_desc = nullptr;
    const cricket::SessionDescription* secondary_current_desc = nullptr;
    if (local_description()) {
      current_desc = local_description()->description();
      if (remote_description()) {
        secondary_current_desc = remote_description()->description();
      }
    } else if (remote_description()) {
      current_desc = remote_description()->description();
    }
    if (current_desc &&
        !MediaSectionsInSameOrder(*current_desc, secondary_current_desc,
                                  *sdesc->description(), type)) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           kMlineMismatchInSubsequentOffer);
    }
  }

  if (IsUnifiedPlan()) {

    for (const ContentInfo& content : sdesc->description()->contents()) {
      const MediaContentDescription& desc = *content.media_description();
      if ((desc.type() == cricket::MEDIA_TYPE_AUDIO ||
           desc.type() == cricket::MEDIA_TYPE_VIDEO) &&
          desc.streams().size() > 1u) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INVALID_PARAMETER,
            "Media section has more than one track specified with a=ssrc lines "
            "which is not supported with Unified Plan.");
      }
    }

    error = ValidateRtpHeaderExtensionsForSpecSimulcast(*sdesc->description());
    if (!error.ok()) {
      return error;
    }
  }

  return RTCError::OK();
}

RTCError SdpOfferAnswerHandler::UpdateTransceiversAndDataChannels(
    cricket::ContentSource source,
    const SessionDescriptionInterface& new_session,
    const SessionDescriptionInterface* old_local_description,
    const SessionDescriptionInterface* old_remote_description,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  TRACE_EVENT0("webrtc",
               "SdpOfferAnswerHandler::UpdateTransceiversAndDataChannels");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  if (new_session.GetType() == SdpType::kOffer) {

    if (pc_->configuration()->bundle_policy ==
            PeerConnectionInterface::kBundlePolicyMaxBundle &&
        bundle_groups_by_mid.empty()) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_PARAMETER,
          "max-bundle configured but session description has no BUNDLE group");
    }
  }

  const ContentInfos& new_contents = new_session.description()->contents();
  for (size_t i = 0; i < new_contents.size(); ++i) {
    const cricket::ContentInfo& new_content = new_contents[i];
    cricket::MediaType media_type = new_content.media_description()->type();
    mid_generator_.AddKnownId(new_content.name);
    auto it = bundle_groups_by_mid.find(new_content.name);
    const cricket::ContentGroup* bundle_group =
        it != bundle_groups_by_mid.end() ? it->second : nullptr;
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      const cricket::ContentInfo* old_local_content = nullptr;
      if (old_local_description &&
          i < old_local_description->description()->contents().size()) {
        old_local_content =
            &old_local_description->description()->contents()[i];
      }
      const cricket::ContentInfo* old_remote_content = nullptr;
      if (old_remote_description &&
          i < old_remote_description->description()->contents().size()) {
        old_remote_content =
            &old_remote_description->description()->contents()[i];
      }
      auto transceiver_or_error =
          AssociateTransceiver(source, new_session.GetType(), i, new_content,
                               old_local_content, old_remote_content);
      if (!transceiver_or_error.ok()) {

        if (new_content.rejected) {
          continue;
        }
        return transceiver_or_error.MoveError();
      }
      auto transceiver = transceiver_or_error.MoveValue();
      RTCError error =
          UpdateTransceiverChannel(transceiver, new_content, bundle_group);

      if (source == cricket::ContentSource::CS_LOCAL && new_content.rejected) {

        if (new_session.GetType() == SdpType::kOffer) {

          if (!transceiver->internal()->stopping()) {
            transceiver->internal()->StopStandard();
          }
          RTC_DCHECK(transceiver->internal()->stopping());
        } else {

          RTC_DCHECK(new_session.GetType() == SdpType::kAnswer ||
                     new_session.GetType() == SdpType::kPrAnswer);

          if (!transceiver->internal()->stopped()) {
            transceiver->internal()->StopTransceiverProcedure();
          }
          RTC_DCHECK(transceiver->internal()->stopped());
        }
      }
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_DATA) {
      const auto data_mid = pc_->sctp_mid();
      if (data_mid && new_content.name != data_mid.value()) {

        RTC_LOG(LS_INFO) << "Ignoring data media section with MID="
                         << new_content.name;
        continue;
      }
      RTCError error =
          UpdateDataChannelTransport(source, new_content, bundle_group);
      if (!error.ok()) {
        return error;
      }
    } else if (media_type == cricket::MEDIA_TYPE_UNSUPPORTED) {
      RTC_LOG(LS_INFO) << "Ignoring unsupported media type";
    } else {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Unknown section type.");
    }
  }

  return RTCError::OK();
}

RTCErrorOr<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
SdpOfferAnswerHandler::AssociateTransceiver(
    cricket::ContentSource source,
    SdpType type,
    size_t mline_index,
    const ContentInfo& content,
    const ContentInfo* old_local_content,
    const ContentInfo* old_remote_content) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::AssociateTransceiver");
  RTC_DCHECK(IsUnifiedPlan());
#if RTC_DCHECK_IS_ON

  if (IsMediaSectionBeingRecycled(type, content, old_local_content,
                                  old_remote_content)) {
    const std::string& old_mid =
        (old_local_content && old_local_content->rejected)
            ? old_local_content->name
            : old_remote_content->name;
    auto old_transceiver = transceivers()->FindByMid(old_mid);

    RTC_DCHECK(!old_transceiver);
  }
#endif

  const MediaContentDescription* media_desc = content.media_description();
  auto transceiver = transceivers()->FindByMid(content.name);
  if (source == cricket::CS_LOCAL) {

    if (!transceiver) {
      transceiver = transceivers()->FindByMLineIndex(mline_index);
    }
    if (!transceiver) {

      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                           "Transceiver not found based on m-line index");
    }
  } else {
    RTC_DCHECK_EQ(source, cricket::CS_REMOTE);

    if (!transceiver &&
        RtpTransceiverDirectionHasRecv(media_desc->direction()) &&
        !media_desc->HasSimulcast()) {
      transceiver = FindAvailableTransceiverToReceive(media_desc->type());
    }

    if (!transceiver) {
      RTC_LOG(LS_INFO) << "Adding "
                       << cricket::MediaTypeToString(media_desc->type())
                       << " transceiver for MID=" << content.name
                       << " at i=" << mline_index
                       << " in response to the remote description.";
      std::string sender_id = rtc::CreateRandomUuid();
      std::vector<RtpEncodingParameters> send_encodings =
          GetSendEncodingsFromRemoteDescription(*media_desc);
      auto sender = rtp_manager()->CreateSender(media_desc->type(), sender_id,
                                                nullptr, {}, send_encodings);
      std::string receiver_id;
      if (!media_desc->streams().empty()) {
        receiver_id = media_desc->streams()[0].id;
      } else {
        receiver_id = rtc::CreateRandomUuid();
      }
      auto receiver =
          rtp_manager()->CreateReceiver(media_desc->type(), receiver_id);
      transceiver = rtp_manager()->CreateAndAddTransceiver(sender, receiver);
      transceiver->internal()->set_direction(
          RtpTransceiverDirection::kRecvOnly);
      if (type == SdpType::kOffer) {
        transceivers()->StableState(transceiver)->set_newly_created();
      }
    }

    RTC_DCHECK(transceiver);

    if (SimulcastIsRejected(old_local_content, *media_desc,
                            pc_->GetCryptoOptions()
                                .srtp.enable_encrypted_rtp_header_extensions)) {
      RTCError error =
          DisableSimulcastInSender(transceiver->internal()->sender_internal());
      if (!error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to remove rejected simulcast.";
        return std::move(error);
      }
    }
  }

  if (transceiver->media_type() != media_desc->type()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Transceiver type does not match media description type.");
  }

  if (media_desc->HasSimulcast()) {
    std::vector<SimulcastLayer> layers =
        source == cricket::CS_LOCAL
            ? media_desc->simulcast_description().send_layers().GetAllLayers()
            : media_desc->simulcast_description()
                  .receive_layers()
                  .GetAllLayers();
    RTCError error = UpdateSimulcastLayerStatusInSender(
        layers, transceiver->internal()->sender_internal());
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed updating status for simulcast layers.";
      return std::move(error);
    }
  }
  if (type == SdpType::kOffer) {
    bool state_changes = transceiver->internal()->mid() != content.name ||
                         transceiver->internal()->mline_index() != mline_index;
    if (state_changes) {
      transceivers()
          ->StableState(transceiver)
          ->SetMSectionIfUnset(transceiver->internal()->mid(),
                               transceiver->internal()->mline_index());
    }
  }

  transceiver->internal()->set_mid(content.name);
  transceiver->internal()->set_mline_index(mline_index);
  return std::move(transceiver);
}

RTCError SdpOfferAnswerHandler::UpdateTransceiverChannel(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::UpdateTransceiverChannel");
  RTC_DCHECK(IsUnifiedPlan());
  RTC_DCHECK(transceiver);
  cricket::ChannelInterface* channel = transceiver->internal()->channel();
  if (content.rejected) {
    if (channel) {
      transceiver->internal()->ClearChannel();
    }
  } else {
    if (!channel) {
      auto error = transceiver->internal()->CreateChannel(
          content.name, pc_->call_ptr(), pc_->configuration()->media_config,
          pc_->SrtpRequired(), pc_->GetCryptoOptions(), audio_options(),
          video_options(), video_bitrate_allocator_factory_.get(),
          [&](absl::string_view mid) {
            RTC_DCHECK_RUN_ON(network_thread());
            return transport_controller_n()->GetRtpTransport(mid);
          });
      if (!error.ok()) {
        return error;
      }
    }
  }
  return RTCError::OK();
}

RTCError SdpOfferAnswerHandler::UpdateDataChannelTransport(
    cricket::ContentSource source,
    const cricket::ContentInfo& content,
    const cricket::ContentGroup* bundle_group) {
  if (content.rejected) {
    RTC_LOG(LS_INFO) << "Rejected data channel transport with mid="
                     << content.mid();

    rtc::StringBuilder sb;
    sb << "Rejected data channel transport with mid=" << content.mid();
    RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA, sb.Release());
    error.set_error_detail(RTCErrorDetailType::DATA_CHANNEL_FAILURE);
    pc_->DestroyDataChannelTransport(error);
  } else if (!pc_->CreateDataChannelTransport(content.name)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to create data channel.");
  }
  return RTCError::OK();
}

bool SdpOfferAnswerHandler::ExpectSetLocalDescription(SdpType type) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (type == SdpType::kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveLocalOffer);
  } else {
    RTC_DCHECK(type == SdpType::kPrAnswer || type == SdpType::kAnswer);
    return (state == PeerConnectionInterface::kHaveRemoteOffer) ||
           (state == PeerConnectionInterface::kHaveLocalPrAnswer);
  }
}

bool SdpOfferAnswerHandler::ExpectSetRemoteDescription(SdpType type) {
  PeerConnectionInterface::SignalingState state = signaling_state();
  if (type == SdpType::kOffer) {
    return (state == PeerConnectionInterface::kStable) ||
           (state == PeerConnectionInterface::kHaveRemoteOffer);
  } else {
    RTC_DCHECK(type == SdpType::kPrAnswer || type == SdpType::kAnswer);
    return (state == PeerConnectionInterface::kHaveLocalOffer) ||
           (state == PeerConnectionInterface::kHaveRemotePrAnswer);
  }
}

void SdpOfferAnswerHandler::FillInMissingRemoteMids(
    cricket::SessionDescription* new_remote_description) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(new_remote_description);
  const cricket::ContentInfos no_infos;
  const cricket::ContentInfos& local_contents =
      (local_description() ? local_description()->description()->contents()
                           : no_infos);
  const cricket::ContentInfos& remote_contents =
      (remote_description() ? remote_description()->description()->contents()
                            : no_infos);
  for (size_t i = 0; i < new_remote_description->contents().size(); ++i) {
    cricket::ContentInfo& content = new_remote_description->contents()[i];
    if (!content.name.empty()) {
      continue;
    }
    std::string new_mid;
    absl::string_view source_explanation;
    if (IsUnifiedPlan()) {
      if (i < local_contents.size()) {
        new_mid = local_contents[i].name;
        source_explanation = "from the matching local media section";
      } else if (i < remote_contents.size()) {
        new_mid = remote_contents[i].name;
        source_explanation = "from the matching previous remote media section";
      } else {
        new_mid = mid_generator_.GenerateString();
        source_explanation = "generated just now";
      }
    } else {
      new_mid = std::string(
          GetDefaultMidForPlanB(content.media_description()->type()));
      source_explanation = "to match pre-existing behavior";
    }
    RTC_DCHECK(!new_mid.empty());
    content.name = new_mid;
    new_remote_description->transport_infos()[i].content_name = new_mid;
    RTC_LOG(LS_INFO) << "SetRemoteDescription: Remote media section at i=" << i
                     << " is missing an a=mid line. Filling in the value '"
                     << new_mid << "' " << source_explanation << ".";
  }
}

rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
SdpOfferAnswerHandler::FindAvailableTransceiverToReceive(
    cricket::MediaType media_type) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  for (auto transceiver : transceivers()->List()) {
    if (transceiver->media_type() == media_type &&
        transceiver->internal()->created_by_addtrack() && !transceiver->mid() &&
        !transceiver->stopped()) {
      return transceiver;
    }
  }
  return nullptr;
}

const cricket::ContentInfo*
SdpOfferAnswerHandler::FindMediaSectionForTransceiver(
    const RtpTransceiver* transceiver,
    const SessionDescriptionInterface* sdesc) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(transceiver);
  RTC_DCHECK(sdesc);
  if (IsUnifiedPlan()) {
    if (!transceiver->mid()) {

      return nullptr;
    }
    return sdesc->description()->GetContentByName(*transceiver->mid());
  } else {

    return cricket::GetFirstMediaContent(sdesc->description()->contents(),
                                         transceiver->media_type());
  }
}

void SdpOfferAnswerHandler::GetOptionsForOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  ExtractSharedMediaSessionOptions(offer_answer_options, session_options);

  if (IsUnifiedPlan()) {
    GetOptionsForUnifiedPlanOffer(offer_answer_options, session_options);
  } else {
    GetOptionsForPlanBOffer(offer_answer_options, session_options);
  }

  bool ice_restart = offer_answer_options.ice_restart || HasNewIceCredentials();
  for (auto& options : session_options->media_description_options) {
    options.transport_options.ice_restart = ice_restart;
    options.transport_options.enable_ice_renomination =
        pc_->configuration()->enable_ice_renomination;
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = pc_->GetCryptoOptions();
  session_options->pooled_ice_credentials =
      context_->network_thread()->BlockingCall(
          [this] { return port_allocator()->GetPooledIceCredentials(); });
  session_options->offer_extmap_allow_mixed =
      pc_->configuration()->offer_extmap_allow_mixed;

  session_options->use_obsolete_sctp_sdp =
      offer_answer_options.use_obsolete_sctp_sdp;
}

void SdpOfferAnswerHandler::GetOptionsForPlanBOffer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  bool offer_new_data_description =
      data_channel_controller()->HasUsedDataChannels();
  bool send_audio = false;
  bool send_video = false;
  bool recv_audio = false;
  bool recv_video = false;
  if (ConfiguredForMedia()) {

    send_audio =
        !rtp_manager()->GetAudioTransceiver()->internal()->senders().empty();
    send_video =
        !rtp_manager()->GetVideoTransceiver()->internal()->senders().empty();

    recv_audio = true;
    recv_video = true;
  }

  bool offer_new_audio_description = send_audio;
  bool offer_new_video_description = send_video;
  if (ConfiguredForMedia()) {

    if (offer_answer_options.offer_to_receive_audio !=
        PeerConnectionInterface::RTCOfferAnswerOptions::kUndefined) {
      recv_audio = (offer_answer_options.offer_to_receive_audio > 0);
      offer_new_audio_description =
          offer_new_audio_description ||
          (offer_answer_options.offer_to_receive_audio > 0);
    }
    if (offer_answer_options.offer_to_receive_video !=
        RTCOfferAnswerOptions::kUndefined) {
      recv_video = (offer_answer_options.offer_to_receive_video > 0);
      offer_new_video_description =
          offer_new_video_description ||
          (offer_answer_options.offer_to_receive_video > 0);
    }
  }
  absl::optional<size_t> audio_index;
  absl::optional<size_t> video_index;
  absl::optional<size_t> data_index;

  if (local_description()) {
    GenerateMediaDescriptionOptions(
        local_description(),
        RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
        RtpTransceiverDirectionFromSendRecv(send_video, recv_video),
        &audio_index, &video_index, &data_index, session_options);
  }

  if (ConfiguredForMedia()) {

    if (!audio_index && offer_new_audio_description) {
      cricket::MediaDescriptionOptions options(
          cricket::MEDIA_TYPE_AUDIO, cricket::CN_AUDIO,
          RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio), false);
      options.header_extensions =
          media_engine()->voice().GetRtpHeaderExtensions();
      session_options->media_description_options.push_back(options);
      audio_index = session_options->media_description_options.size() - 1;
    }
    if (!video_index && offer_new_video_description) {
      cricket::MediaDescriptionOptions options(
          cricket::MEDIA_TYPE_VIDEO, cricket::CN_VIDEO,
          RtpTransceiverDirectionFromSendRecv(send_video, recv_video), false);
      options.header_extensions =
          media_engine()->video().GetRtpHeaderExtensions();
      session_options->media_description_options.push_back(options);
      video_index = session_options->media_description_options.size() - 1;
    }
    cricket::MediaDescriptionOptions* audio_media_description_options =
        !audio_index
            ? nullptr
            : &session_options->media_description_options[*audio_index];
    cricket::MediaDescriptionOptions* video_media_description_options =
        !video_index
            ? nullptr
            : &session_options->media_description_options[*video_index];

    AddPlanBRtpSenderOptions(rtp_manager()->GetSendersInternal(),
                             audio_media_description_options,
                             video_media_description_options,
                             offer_answer_options.num_simulcast_layers);
  }
  if (!data_index && offer_new_data_description) {
    session_options->media_description_options.push_back(
        GetMediaDescriptionOptionsForActiveData(cricket::CN_DATA));
  }
}

void SdpOfferAnswerHandler::GetOptionsForUnifiedPlanOffer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {

  RTC_DCHECK_EQ(session_options->media_description_options.size(), 0);
  const ContentInfos no_infos;
  const ContentInfos& local_contents =
      (local_description() ? local_description()->description()->contents()
                           : no_infos);
  const ContentInfos& remote_contents =
      (remote_description() ? remote_description()->description()->contents()
                            : no_infos);

  std::queue<size_t> recycleable_mline_indices;

  for (size_t i = 0;
       i < std::max(local_contents.size(), remote_contents.size()); ++i) {

    const ContentInfo* local_content =
        (i < local_contents.size() ? &local_contents[i] : nullptr);
    const ContentInfo* current_local_content =
        GetContentByIndex(current_local_description(), i);
    const ContentInfo* remote_content =
        (i < remote_contents.size() ? &remote_contents[i] : nullptr);
    const ContentInfo* current_remote_content =
        GetContentByIndex(current_remote_description(), i);
    bool had_been_rejected =
        (current_local_content && current_local_content->rejected) ||
        (current_remote_content && current_remote_content->rejected);
    const std::string& mid =
        (local_content ? local_content->name : remote_content->name);
    cricket::MediaType media_type =
        (local_content ? local_content->media_description()->type()
                       : remote_content->media_description()->type());
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {

      auto transceiver = transceivers()->FindByMid(mid);
      if (!transceiver) {

        recycleable_mline_indices.push(i);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(media_type, mid,
                                             RtpTransceiverDirection::kInactive,
                                                         true));
      } else {

        if (had_been_rejected && transceiver->stopping()) {
          session_options->media_description_options.push_back(
              cricket::MediaDescriptionOptions(
                  transceiver->media_type(), mid,
                  RtpTransceiverDirection::kInactive,
                              true));
          recycleable_mline_indices.push(i);
        } else {
          session_options->media_description_options.push_back(
              GetMediaDescriptionOptionsForTransceiver(
                  transceiver->internal(), mid,
                                      true));

          transceiver->internal()->set_mline_index(i);
        }
      }
    } else if (media_type == cricket::MEDIA_TYPE_UNSUPPORTED) {
      RTC_DCHECK(local_content->rejected);
      session_options->media_description_options.push_back(
          cricket::MediaDescriptionOptions(media_type, mid,
                                           RtpTransceiverDirection::kInactive,
                                                       true));
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);
      if (had_been_rejected) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(mid));
      } else {
        const auto data_mid = pc_->sctp_mid();
        if (data_mid.has_value() && mid == data_mid.value()) {
          session_options->media_description_options.push_back(
              GetMediaDescriptionOptionsForActiveData(mid));
        } else {
          if (!data_mid.has_value()) {
            RTC_LOG(LS_ERROR) << "Datachannel transport not available: " << mid;
          }
          session_options->media_description_options.push_back(
              GetMediaDescriptionOptionsForRejectedData(mid));
        }
      }
    }
  }

  if (ConfiguredForMedia()) {
    for (const auto& transceiver : transceivers()->ListInternal()) {
      if (transceiver->mid() || transceiver->stopping()) {
        continue;
      }
      size_t mline_index;
      if (!recycleable_mline_indices.empty()) {
        mline_index = recycleable_mline_indices.front();
        recycleable_mline_indices.pop();
        session_options->media_description_options[mline_index] =
            GetMediaDescriptionOptionsForTransceiver(
                transceiver, mid_generator_.GenerateString(),
                                    true);
      } else {
        mline_index = session_options->media_description_options.size();
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForTransceiver(
                transceiver, mid_generator_.GenerateString(),
                                    true));
      }

      transceiver->set_mline_index(mline_index);
    }
  }

  if (!pc_->sctp_mid() && data_channel_controller()->HasDataChannels()) {

    bool recycled = false;
    for (size_t i = 0; i < session_options->media_description_options.size();
         i++) {
      auto media_description = session_options->media_description_options[i];
      if (media_description.type == cricket::MEDIA_TYPE_DATA &&
          media_description.stopped) {
        session_options->media_description_options[i] =
            GetMediaDescriptionOptionsForActiveData(media_description.mid);
        recycled = true;
        break;
      }
    }
    if (!recycled) {
      session_options->media_description_options.push_back(
          GetMediaDescriptionOptionsForActiveData(
              mid_generator_.GenerateString()));
    }
  }
}

void SdpOfferAnswerHandler::GetOptionsForAnswer(
    const RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  ExtractSharedMediaSessionOptions(offer_answer_options, session_options);

  if (IsUnifiedPlan()) {
    GetOptionsForUnifiedPlanAnswer(offer_answer_options, session_options);
  } else {
    GetOptionsForPlanBAnswer(offer_answer_options, session_options);
  }

  for (auto& options : session_options->media_description_options) {
    options.transport_options.enable_ice_renomination =
        pc_->configuration()->enable_ice_renomination;
  }

  session_options->rtcp_cname = rtcp_cname_;
  session_options->crypto_options = pc_->GetCryptoOptions();
  session_options->pooled_ice_credentials =
      context_->network_thread()->BlockingCall(
          [this] { return port_allocator()->GetPooledIceCredentials(); });
}

void SdpOfferAnswerHandler::GetOptionsForPlanBAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {
  bool send_audio = false;
  bool recv_audio = false;
  bool send_video = false;
  bool recv_video = false;

  if (ConfiguredForMedia()) {

    send_audio =
        !rtp_manager()->GetAudioTransceiver()->internal()->senders().empty();
    send_video =
        !rtp_manager()->GetVideoTransceiver()->internal()->senders().empty();

    recv_audio = true;
    recv_video = true;

    if (offer_answer_options.offer_to_receive_audio !=
        RTCOfferAnswerOptions::kUndefined) {
      recv_audio = (offer_answer_options.offer_to_receive_audio > 0);
    }
    if (offer_answer_options.offer_to_receive_video !=
        RTCOfferAnswerOptions::kUndefined) {
      recv_video = (offer_answer_options.offer_to_receive_video > 0);
    }
  }

  absl::optional<size_t> audio_index;
  absl::optional<size_t> video_index;
  absl::optional<size_t> data_index;

  GenerateMediaDescriptionOptions(
      remote_description(),
      RtpTransceiverDirectionFromSendRecv(send_audio, recv_audio),
      RtpTransceiverDirectionFromSendRecv(send_video, recv_video), &audio_index,
      &video_index, &data_index, session_options);

  cricket::MediaDescriptionOptions* audio_media_description_options =
      !audio_index ? nullptr
                   : &session_options->media_description_options[*audio_index];
  cricket::MediaDescriptionOptions* video_media_description_options =
      !video_index ? nullptr
                   : &session_options->media_description_options[*video_index];

  if (ConfiguredForMedia()) {
    AddPlanBRtpSenderOptions(rtp_manager()->GetSendersInternal(),
                             audio_media_description_options,
                             video_media_description_options,
                             offer_answer_options.num_simulcast_layers);
  }
}

void SdpOfferAnswerHandler::GetOptionsForUnifiedPlanAnswer(
    const PeerConnectionInterface::RTCOfferAnswerOptions& offer_answer_options,
    cricket::MediaSessionOptions* session_options) {

  RTC_DCHECK(remote_description());
  RTC_DCHECK(remote_description()->GetType() == SdpType::kOffer);
  for (const ContentInfo& content :
       remote_description()->description()->contents()) {
    cricket::MediaType media_type = content.media_description()->type();
    if (media_type == cricket::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MEDIA_TYPE_VIDEO) {
      auto transceiver = transceivers()->FindByMid(content.name);
      if (transceiver) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForTransceiver(
                transceiver->internal(), content.name,
                                    false));
      } else {

        RTC_DCHECK(content.rejected);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(media_type, content.name,
                                             RtpTransceiverDirection::kInactive,
                                                         true));
      }
    } else if (media_type == cricket::MEDIA_TYPE_UNSUPPORTED) {
      RTC_DCHECK(content.rejected);
      session_options->media_description_options.push_back(
          cricket::MediaDescriptionOptions(media_type, content.name,
                                           RtpTransceiverDirection::kInactive,
                                                       true));
    } else {
      RTC_CHECK_EQ(cricket::MEDIA_TYPE_DATA, media_type);

      if (content.rejected || content.name != *(pc_->sctp_mid())) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(content.name));
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForActiveData(content.name));
      }
    }
  }
}

const char* SdpOfferAnswerHandler::SessionErrorToString(
    SessionError error) const {
  switch (error) {
    case SessionError::kNone:
      return "ERROR_NONE";
    case SessionError::kContent:
      return "ERROR_CONTENT";
    case SessionError::kTransport:
      return "ERROR_TRANSPORT";
  }
  RTC_DCHECK_NOTREACHED();
  return "";
}

std::string SdpOfferAnswerHandler::GetSessionErrorMsg() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  rtc::StringBuilder desc;
  desc << kSessionError << SessionErrorToString(session_error()) << ". ";
  desc << kSessionErrorDesc << session_error_desc() << ".";
  return desc.Release();
}

void SdpOfferAnswerHandler::SetSessionError(SessionError error,
                                            const std::string& error_desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (error != session_error_) {
    session_error_ = error;
    session_error_desc_ = error_desc;
  }
}

RTCError SdpOfferAnswerHandler::HandleLegacyOfferOptions(
    const PeerConnectionInterface::RTCOfferAnswerOptions& options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(IsUnifiedPlan());

  if (options.offer_to_receive_audio == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_AUDIO);
  } else if (options.offer_to_receive_audio > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_audio > 1 is not supported.");
  }

  if (options.offer_to_receive_video == 0) {
    RemoveRecvDirectionFromReceivingTransceiversOfType(
        cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video == 1) {
    AddUpToOneReceivingTransceiverOfType(cricket::MEDIA_TYPE_VIDEO);
  } else if (options.offer_to_receive_video > 1) {
    LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_PARAMETER,
                         "offer_to_receive_video > 1 is not supported.");
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandler::RemoveRecvDirectionFromReceivingTransceiversOfType(
    cricket::MediaType media_type) {
  for (const auto& transceiver : GetReceivingTransceiversOfType(media_type)) {
    RtpTransceiverDirection new_direction =
        RtpTransceiverDirectionWithRecvSet(transceiver->direction(), false);
    if (new_direction != transceiver->direction()) {
      RTC_LOG(LS_INFO) << "Changing " << cricket::MediaTypeToString(media_type)
                       << " transceiver (MID="
                       << transceiver->mid().value_or("<not set>") << ") from "
                       << RtpTransceiverDirectionToString(
                              transceiver->direction())
                       << " to "
                       << RtpTransceiverDirectionToString(new_direction)
                       << " since CreateOffer specified offer_to_receive=0";
      transceiver->internal()->set_direction(new_direction);
    }
  }
}

void SdpOfferAnswerHandler::AddUpToOneReceivingTransceiverOfType(
    cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (GetReceivingTransceiversOfType(media_type).empty()) {
    RTC_LOG(LS_INFO)
        << "Adding one recvonly " << cricket::MediaTypeToString(media_type)
        << " transceiver since CreateOffer specified offer_to_receive=1";
    RtpTransceiverInit init;
    init.direction = RtpTransceiverDirection::kRecvOnly;
    pc_->AddTransceiver(media_type, nullptr, init,
                                                      false);
  }
}

std::vector<rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
SdpOfferAnswerHandler::GetReceivingTransceiversOfType(
    cricket::MediaType media_type) {
  std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
      receiving_transceivers;
  for (const auto& transceiver : transceivers()->List()) {
    if (!transceiver->stopped() && transceiver->media_type() == media_type &&
        RtpTransceiverDirectionHasRecv(transceiver->direction())) {
      receiving_transceivers.push_back(transceiver);
    }
  }
  return receiving_transceivers;
}

void SdpOfferAnswerHandler::ProcessRemovalOfRemoteTrack(
    rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>
        transceiver,
    std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>* remove_list,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK(transceiver->mid());
  RTC_LOG(LS_INFO) << "Processing the removal of a track for MID="
                   << *transceiver->mid();
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> previous_streams =
      transceiver->internal()->receiver_internal()->streams();

  transceiver->internal()->receiver_internal()->set_stream_ids({});
  remove_list->push_back(transceiver);
  RemoveRemoteStreamsIfEmpty(previous_streams, removed_streams);
}

void SdpOfferAnswerHandler::RemoveRemoteStreamsIfEmpty(
    const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& remote_streams,
    std::vector<rtc::scoped_refptr<MediaStreamInterface>>* removed_streams) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  for (const auto& remote_stream : remote_streams) {
    if (remote_stream->GetAudioTracks().empty() &&
        remote_stream->GetVideoTracks().empty()) {
      remote_streams_->RemoveStream(remote_stream.get());
      removed_streams->push_back(remote_stream);
    }
  }
}

void SdpOfferAnswerHandler::RemoveSenders(cricket::MediaType media_type) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  UpdateLocalSenders(std::vector<cricket::StreamParams>(), media_type);
  UpdateRemoteSendersList(std::vector<cricket::StreamParams>(), false,
                          media_type, nullptr);
}

void SdpOfferAnswerHandler::UpdateLocalSenders(
    const std::vector<cricket::StreamParams>& streams,
    cricket::MediaType media_type) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::UpdateLocalSenders");
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<RtpSenderInfo>* current_senders =
      rtp_manager()->GetLocalSenderInfos(media_type);

  for (auto sender_it = current_senders->begin();
       sender_it != current_senders->end();
                                 ) {
    const RtpSenderInfo& info = *sender_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.first_ssrc);
    if (!params || params->id != info.sender_id ||
        params->first_stream_id() != info.stream_id) {
      rtp_manager()->OnLocalSenderRemoved(info, media_type);
      sender_it = current_senders->erase(sender_it);
    } else {
      ++sender_it;
    }
  }

  for (const cricket::StreamParams& params : streams) {

    const std::string& stream_id = params.first_stream_id();
    const std::string& sender_id = params.id;
    uint32_t ssrc = params.first_ssrc();
    const RtpSenderInfo* sender_info =
        rtp_manager()->FindSenderInfo(*current_senders, stream_id, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_id, sender_id, ssrc));
      rtp_manager()->OnLocalSenderAdded(current_senders->back(), media_type);
    }
  }
}

void SdpOfferAnswerHandler::UpdateRemoteSendersList(
    const cricket::StreamParamsVec& streams,
    bool default_sender_needed,
    cricket::MediaType media_type,
    StreamCollection* new_streams) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::UpdateRemoteSendersList");
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(!IsUnifiedPlan());

  std::vector<RtpSenderInfo>* current_senders =
      rtp_manager()->GetRemoteSenderInfos(media_type);

  for (auto sender_it = current_senders->begin();
       sender_it != current_senders->end();
                                 ) {
    const RtpSenderInfo& info = *sender_it;
    const cricket::StreamParams* params =
        cricket::GetStreamBySsrc(streams, info.first_ssrc);
    std::string params_stream_id;
    if (params) {
      params_stream_id =
          (!params->first_stream_id().empty() ? params->first_stream_id()
                                              : kDefaultStreamId);
    }
    bool sender_exists = params && params->id == info.sender_id &&
                         params_stream_id == info.stream_id;

    if ((info.stream_id == kDefaultStreamId && default_sender_needed) ||
        sender_exists) {
      ++sender_it;
    } else {
      rtp_manager()->OnRemoteSenderRemoved(
          info, remote_streams_->find(info.stream_id), media_type);
      sender_it = current_senders->erase(sender_it);
    }
  }

  for (const cricket::StreamParams& params : streams) {
    if (!params.has_ssrcs()) {

      default_sender_needed = true;
      break;
    }

    const std::string& stream_id =
        (!params.first_stream_id().empty() ? params.first_stream_id()
                                           : kDefaultStreamId);
    const std::string& sender_id = params.id;
    uint32_t ssrc = params.first_ssrc();

    rtc::scoped_refptr<MediaStreamInterface> stream(
        remote_streams_->find(stream_id));
    if (!stream) {

      stream = MediaStreamProxy::Create(rtc::Thread::Current(),
                                        MediaStream::Create(stream_id));
      remote_streams_->AddStream(stream);
      new_streams->AddStream(stream);
    }

    const RtpSenderInfo* sender_info =
        rtp_manager()->FindSenderInfo(*current_senders, stream_id, sender_id);
    if (!sender_info) {
      current_senders->push_back(RtpSenderInfo(stream_id, sender_id, ssrc));
      rtp_manager()->OnRemoteSenderAdded(current_senders->back(), stream.get(),
                                         media_type);
    }
  }

  if (default_sender_needed) {
    rtc::scoped_refptr<MediaStreamInterface> default_stream(
        remote_streams_->find(kDefaultStreamId));
    if (!default_stream) {

      default_stream = MediaStreamProxy::Create(
          rtc::Thread::Current(), MediaStream::Create(kDefaultStreamId));
      remote_streams_->AddStream(default_stream);
      new_streams->AddStream(default_stream);
    }
    std::string default_sender_id = (media_type == cricket::MEDIA_TYPE_AUDIO)
                                        ? kDefaultAudioSenderId
                                        : kDefaultVideoSenderId;
    const RtpSenderInfo* default_sender_info = rtp_manager()->FindSenderInfo(
        *current_senders, kDefaultStreamId, default_sender_id);
    if (!default_sender_info) {
      current_senders->push_back(
          RtpSenderInfo(kDefaultStreamId, default_sender_id,          0));
      rtp_manager()->OnRemoteSenderAdded(current_senders->back(),
                                         default_stream.get(), media_type);
    }
  }
}

void SdpOfferAnswerHandler::EnableSending() {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::EnableSending");
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!ConfiguredForMedia()) {
    return;
  }
  for (const auto& transceiver : transceivers()->ListInternal()) {
    cricket::ChannelInterface* channel = transceiver->channel();
    if (channel) {
      channel->Enable(true);
    }
  }
}

RTCError SdpOfferAnswerHandler::PushdownMediaDescription(
    SdpType type,
    cricket::ContentSource source,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::PushdownMediaDescription");
  const SessionDescriptionInterface* sdesc =
      (source == cricket::CS_LOCAL ? local_description()
                                   : remote_description());
  RTC_DCHECK_RUN_ON(signaling_thread());
  RTC_DCHECK(sdesc);

  if (ConfiguredForMedia()) {

    if (!UpdatePayloadTypeDemuxingState(source, bundle_groups_by_mid)) {

      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to update payload type demuxing state.");
    }

    auto rtp_transceivers = transceivers()->ListInternal();
    std::vector<
        std::pair<cricket::ChannelInterface*, const MediaContentDescription*>>
        channels;
    for (const auto& transceiver : rtp_transceivers) {
      const ContentInfo* content_info =
          FindMediaSectionForTransceiver(transceiver, sdesc);
      cricket::ChannelInterface* channel = transceiver->channel();
      if (!channel || !content_info || content_info->rejected) {
        continue;
      }
      const MediaContentDescription* content_desc =
          content_info->media_description();
      if (!content_desc) {
        continue;
      }

      transceiver->OnNegotiationUpdate(type, content_desc);
      channels.push_back(std::make_pair(channel, content_desc));
    }

    for (const auto& entry : channels) {
      std::string error;
      bool success = context_->worker_thread()->BlockingCall([&]() {
        return (source == cricket::CS_LOCAL)
                   ? entry.first->SetLocalContent(entry.second, type, error)
                   : entry.first->SetRemoteContent(entry.second, type, error);
      });
      if (!success) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER, error);
      }
    }
  }

  if (pc_->sctp_mid() && local_description() && remote_description()) {
    auto local_sctp_description = cricket::GetFirstSctpDataContentDescription(
        local_description()->description());
    auto remote_sctp_description = cricket::GetFirstSctpDataContentDescription(
        remote_description()->description());
    if (local_sctp_description && remote_sctp_description) {
      int max_message_size;

      if (remote_sctp_description->max_message_size() == 0) {
        max_message_size = local_sctp_description->max_message_size();
      } else {
        max_message_size =
            std::min(local_sctp_description->max_message_size(),
                     remote_sctp_description->max_message_size());
      }
      pc_->StartSctpTransport(local_sctp_description->port(),
                              remote_sctp_description->port(),
                              max_message_size);
    }
  }

  return RTCError::OK();
}

RTCError SdpOfferAnswerHandler::PushdownTransportDescription(
    cricket::ContentSource source,
    SdpType type) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::PushdownTransportDescription");
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (source == cricket::CS_LOCAL) {
    const SessionDescriptionInterface* sdesc = local_description();
    RTC_DCHECK(sdesc);
    const auto* remote = remote_description();
    return transport_controller_s()->SetLocalDescription(
        type, sdesc->description(), remote ? remote->description() : nullptr);
  } else {
    const SessionDescriptionInterface* sdesc = remote_description();
    RTC_DCHECK(sdesc);
    const auto* local = local_description();
    return transport_controller_s()->SetRemoteDescription(
        type, local ? local->description() : nullptr, sdesc->description());
  }
}

void SdpOfferAnswerHandler::RemoveStoppedTransceivers() {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::RemoveStoppedTransceivers");
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (!IsUnifiedPlan())
    return;
  if (!ConfiguredForMedia()) {
    return;
  }

  auto transceiver_list = transceivers()->List();
  for (auto transceiver : transceiver_list) {

    if (!transceiver->stopped()) {
      continue;
    }
    const ContentInfo* local_content = FindMediaSectionForTransceiver(
        transceiver->internal(), local_description());
    const ContentInfo* remote_content = FindMediaSectionForTransceiver(
        transceiver->internal(), remote_description());
    if ((local_content && local_content->rejected) ||
        (remote_content && remote_content->rejected)) {
      RTC_LOG(LS_INFO) << "Dissociating transceiver"
                          " since the media section is being recycled.";
      transceiver->internal()->set_mid(absl::nullopt);
      transceiver->internal()->set_mline_index(absl::nullopt);
    } else if (!local_content && !remote_content) {

      RTC_LOG(LS_INFO)
          << "Dropping stopped transceiver that was never associated";
    }
    transceivers()->Remove(transceiver);
  }
}

void SdpOfferAnswerHandler::RemoveUnusedChannels(
    const SessionDescription* desc) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (ConfiguredForMedia()) {

    const cricket::ContentInfo* video_info =
        cricket::GetFirstVideoContent(desc);
    if (!video_info || video_info->rejected) {
      rtp_manager()->GetVideoTransceiver()->internal()->ClearChannel();
    }

    const cricket::ContentInfo* audio_info =
        cricket::GetFirstAudioContent(desc);
    if (!audio_info || audio_info->rejected) {
      rtp_manager()->GetAudioTransceiver()->internal()->ClearChannel();
    }
  }
  const cricket::ContentInfo* data_info = cricket::GetFirstDataContent(desc);
  if (!data_info) {
    RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA,
                   "No data channel section in the description.");
    error.set_error_detail(RTCErrorDetailType::DATA_CHANNEL_FAILURE);
    pc_->DestroyDataChannelTransport(error);
  } else if (data_info->rejected) {
    rtc::StringBuilder sb;
    sb << "Rejected data channel with mid=" << data_info->name << ".";

    RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA, sb.Release());
    error.set_error_detail(RTCErrorDetailType::DATA_CHANNEL_FAILURE);
    pc_->DestroyDataChannelTransport(error);
  }
}

void SdpOfferAnswerHandler::UpdateEndedRemoteMediaStreams() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams_to_remove;
  for (size_t i = 0; i < remote_streams_->count(); ++i) {
    MediaStreamInterface* stream = remote_streams_->at(i);
    if (stream->GetAudioTracks().empty() && stream->GetVideoTracks().empty()) {
      streams_to_remove.push_back(
          rtc::scoped_refptr<MediaStreamInterface>(stream));
    }
  }

  for (auto& stream : streams_to_remove) {
    remote_streams_->RemoveStream(stream.get());
    pc_->Observer()->OnRemoveStream(std::move(stream));
  }
}

bool SdpOfferAnswerHandler::UseCandidatesInRemoteDescription() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  auto* remote_desc = remote_description();
  if (!remote_desc) {
    return true;
  }
  bool ret = true;

  for (size_t m = 0; m < remote_desc->number_of_mediasections(); ++m) {
    const IceCandidateCollection* candidates = remote_desc->candidates(m);
    for (size_t n = 0; n < candidates->count(); ++n) {
      const IceCandidateInterface* candidate = candidates->at(n);
      bool valid = false;
      if (!ReadyToUseRemoteCandidate(candidate, remote_desc, &valid)) {
        if (valid) {
          RTC_LOG(LS_INFO)
              << "UseCandidatesInRemoteDescription: Not ready to use "
                 "candidate.";
        }
        continue;
      }
      ret = UseCandidate(candidate);
      if (!ret) {
        break;
      }
    }
  }
  return ret;
}

bool SdpOfferAnswerHandler::UseCandidate(
    const IceCandidateInterface* candidate) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  rtc::Thread::ScopedDisallowBlockingCalls no_blocking_calls;

  RTCErrorOr<const cricket::ContentInfo*> result =
      FindContentInfo(remote_description(), candidate);
  if (!result.ok())
    return false;

  const cricket::Candidate& c = candidate->candidate();
  RTCError error = cricket::VerifyCandidate(c);
  if (!error.ok()) {
    RTC_LOG(LS_WARNING) << "Invalid candidate: " << c.ToString();
    return true;
  }

  pc_->AddRemoteCandidate(result.value()->name, c);

  return true;
}

bool SdpOfferAnswerHandler::ReadyToUseRemoteCandidate(
    const IceCandidateInterface* candidate,
    const SessionDescriptionInterface* remote_desc,
    bool* valid) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  *valid = true;

  const SessionDescriptionInterface* current_remote_desc =
      remote_desc ? remote_desc : remote_description();

  if (!current_remote_desc) {
    return false;
  }

  RTCErrorOr<const cricket::ContentInfo*> result =
      FindContentInfo(current_remote_desc, candidate);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "ReadyToUseRemoteCandidate: Invalid candidate. "
                      << result.error().message();

    *valid = false;
    return false;
  }

  return true;
}

RTCErrorOr<const cricket::ContentInfo*> SdpOfferAnswerHandler::FindContentInfo(
    const SessionDescriptionInterface* description,
    const IceCandidateInterface* candidate) {
  if (!candidate->sdp_mid().empty()) {
    auto& contents = description->description()->contents();
    auto it = absl::c_find_if(
        contents, [candidate](const cricket::ContentInfo& content_info) {
          return content_info.mid() == candidate->sdp_mid();
        });
    if (it == contents.end()) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_PARAMETER,
          "Mid " + candidate->sdp_mid() +
              " specified but no media section with that mid found.");
    } else {
      return &*it;
    }
  } else if (candidate->sdp_mline_index() >= 0) {
    size_t mediacontent_index =
        static_cast<size_t>(candidate->sdp_mline_index());
    size_t content_size = description->description()->contents().size();
    if (mediacontent_index < content_size) {
      return &description->description()->contents()[mediacontent_index];
    } else {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                           "Media line index (" +
                               rtc::ToString(candidate->sdp_mline_index()) +
                               ") out of range (number of mlines: " +
                               rtc::ToString(content_size) + ").");
    }
  }

  LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                       "Neither sdp_mline_index nor sdp_mid specified.");
}

RTCError SdpOfferAnswerHandler::CreateChannels(const SessionDescription& desc) {
  TRACE_EVENT0("webrtc", "SdpOfferAnswerHandler::CreateChannels");

  RTC_DCHECK_RUN_ON(signaling_thread());
  const cricket::ContentInfo* voice = cricket::GetFirstAudioContent(&desc);
  if (voice && !voice->rejected &&
      !rtp_manager()->GetAudioTransceiver()->internal()->channel()) {
    auto error =
        rtp_manager()->GetAudioTransceiver()->internal()->CreateChannel(
            voice->name, pc_->call_ptr(), pc_->configuration()->media_config,
            pc_->SrtpRequired(), pc_->GetCryptoOptions(), audio_options(),
            video_options(), video_bitrate_allocator_factory_.get(),
            [&](absl::string_view mid) {
              RTC_DCHECK_RUN_ON(network_thread());
              return transport_controller_n()->GetRtpTransport(mid);
            });
    if (!error.ok()) {
      return error;
    }
  }

  const cricket::ContentInfo* video = cricket::GetFirstVideoContent(&desc);
  if (video && !video->rejected &&
      !rtp_manager()->GetVideoTransceiver()->internal()->channel()) {
    auto error =
        rtp_manager()->GetVideoTransceiver()->internal()->CreateChannel(
            video->name, pc_->call_ptr(), pc_->configuration()->media_config,
            pc_->SrtpRequired(), pc_->GetCryptoOptions(),

            audio_options(), video_options(),
            video_bitrate_allocator_factory_.get(), [&](absl::string_view mid) {
              RTC_DCHECK_RUN_ON(network_thread());
              return transport_controller_n()->GetRtpTransport(mid);
            });
    if (!error.ok()) {
      return error;
    }
  }

  const cricket::ContentInfo* data = cricket::GetFirstDataContent(&desc);
  if (data && !data->rejected && !pc_->CreateDataChannelTransport(data->name)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to create data channel.");
  }

  return RTCError::OK();
}

void SdpOfferAnswerHandler::DestroyMediaChannels() {
  RTC_DCHECK_RUN_ON(signaling_thread());
  if (!transceivers()) {
    return;
  }

  RTC_LOG_THREAD_BLOCK_COUNT();

  auto list = transceivers()->List();
  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(0);

  for (const auto& transceiver : list) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_VIDEO) {
      transceiver->internal()->ClearChannel();
    }
  }
  for (const auto& transceiver : list) {
    if (transceiver->media_type() == cricket::MEDIA_TYPE_AUDIO) {
      transceiver->internal()->ClearChannel();
    }
  }
}

void SdpOfferAnswerHandler::GenerateMediaDescriptionOptions(
    const SessionDescriptionInterface* session_desc,
    RtpTransceiverDirection audio_direction,
    RtpTransceiverDirection video_direction,
    absl::optional<size_t>* audio_index,
    absl::optional<size_t>* video_index,
    absl::optional<size_t>* data_index,
    cricket::MediaSessionOptions* session_options) {
  RTC_DCHECK_RUN_ON(signaling_thread());
  for (const cricket::ContentInfo& content :
       session_desc->description()->contents()) {
    if (IsAudioContent(&content)) {

      if (*audio_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_AUDIO, content.name,
                RtpTransceiverDirection::kInactive,             true));
      } else {
        bool stopped = (audio_direction == RtpTransceiverDirection::kInactive);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(cricket::MEDIA_TYPE_AUDIO,
                                             content.name, audio_direction,
                                             stopped));
        *audio_index = session_options->media_description_options.size() - 1;
      }
      session_options->media_description_options.back().header_extensions =
          media_engine()->voice().GetRtpHeaderExtensions();
    } else if (IsVideoContent(&content)) {

      if (*video_index) {
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(
                cricket::MEDIA_TYPE_VIDEO, content.name,
                RtpTransceiverDirection::kInactive,             true));
      } else {
        bool stopped = (video_direction == RtpTransceiverDirection::kInactive);
        session_options->media_description_options.push_back(
            cricket::MediaDescriptionOptions(cricket::MEDIA_TYPE_VIDEO,
                                             content.name, video_direction,
                                             stopped));
        *video_index = session_options->media_description_options.size() - 1;
      }
      session_options->media_description_options.back().header_extensions =
          media_engine()->video().GetRtpHeaderExtensions();
    } else if (IsUnsupportedContent(&content)) {
      session_options->media_description_options.push_back(
          cricket::MediaDescriptionOptions(cricket::MEDIA_TYPE_UNSUPPORTED,
                                           content.name,
                                           RtpTransceiverDirection::kInactive,
                                                       true));
    } else {
      RTC_DCHECK(IsDataContent(&content));

      if (*data_index) {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForRejectedData(content.name));
      } else {
        session_options->media_description_options.push_back(
            GetMediaDescriptionOptionsForActiveData(content.name));
        *data_index = session_options->media_description_options.size() - 1;
      }
    }
  }
}

cricket::MediaDescriptionOptions
SdpOfferAnswerHandler::GetMediaDescriptionOptionsForActiveData(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());

  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kSendRecv,
                                                       false);
  return options;
}

cricket::MediaDescriptionOptions
SdpOfferAnswerHandler::GetMediaDescriptionOptionsForRejectedData(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(signaling_thread());
  cricket::MediaDescriptionOptions options(cricket::MEDIA_TYPE_DATA, mid,
                                           RtpTransceiverDirection::kInactive,
                                                       true);
  return options;
}

bool SdpOfferAnswerHandler::UpdatePayloadTypeDemuxingState(
    cricket::ContentSource source,
    const std::map<std::string, const cricket::ContentGroup*>&
        bundle_groups_by_mid) {
  TRACE_EVENT0("webrtc",
               "SdpOfferAnswerHandler::UpdatePayloadTypeDemuxingState");
  RTC_DCHECK_RUN_ON(signaling_thread());

  const SessionDescriptionInterface* sdesc =
      (source == cricket::CS_LOCAL ? local_description()
                                   : remote_description());
  struct PayloadTypes {
    std::set<int> audio_payload_types;
    std::set<int> video_payload_types;
    bool pt_demuxing_possible_audio = true;
    bool pt_demuxing_possible_video = true;
  };
  std::map<const cricket::ContentGroup*, PayloadTypes> payload_types_by_bundle;

  bool mid_header_extension_missing_audio = false;
  bool mid_header_extension_missing_video = false;
  for (auto& content_info : sdesc->description()->contents()) {
    auto it = bundle_groups_by_mid.find(content_info.name);
    const cricket::ContentGroup* bundle_group =
        it != bundle_groups_by_mid.end() ? it->second : nullptr;

    if (!bundle_group) {
      continue;
    }
    PayloadTypes* payload_types = &payload_types_by_bundle[bundle_group];
    if (content_info.rejected ||
        (source == cricket::ContentSource::CS_LOCAL &&
         !RtpTransceiverDirectionHasRecv(
             content_info.media_description()->direction())) ||
        (source == cricket::ContentSource::CS_REMOTE &&
         !RtpTransceiverDirectionHasSend(
             content_info.media_description()->direction()))) {

      continue;
    }
    const cricket::MediaType media_type =
        content_info.media_description()->type();
    if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO ||
        media_type == cricket::MediaType::MEDIA_TYPE_VIDEO) {
      if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO &&
          !mid_header_extension_missing_audio) {
        mid_header_extension_missing_audio =
            !ContentHasHeaderExtension(content_info, RtpExtension::kMidUri);
      } else if (media_type == cricket::MEDIA_TYPE_VIDEO &&
                 !mid_header_extension_missing_video) {
        mid_header_extension_missing_video =
            !ContentHasHeaderExtension(content_info, RtpExtension::kMidUri);
      }
      const cricket::MediaContentDescription* media_desc =
          content_info.media_description();
      for (const cricket::Codec& codec : media_desc->codecs()) {
        if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
          if (payload_types->audio_payload_types.count(codec.id)) {

            if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
              payload_types->pt_demuxing_possible_audio = false;
            }
          }
          payload_types->audio_payload_types.insert(codec.id);
        } else if (media_type == cricket::MEDIA_TYPE_VIDEO) {
          if (payload_types->video_payload_types.count(codec.id)) {

            payload_types->pt_demuxing_possible_video = false;
          }
          payload_types->video_payload_types.insert(codec.id);
        }
      }
    }
  }

  bool bundled_pt_demux_allowed_audio = !IsUnifiedPlan() ||
                                        mid_header_extension_missing_audio ||
                                        pt_demuxing_has_been_used_audio_;
  bool bundled_pt_demux_allowed_video = !IsUnifiedPlan() ||
                                        mid_header_extension_missing_video ||
                                        pt_demuxing_has_been_used_video_;

  std::vector<std::pair<bool, cricket::ChannelInterface*>> channels_to_update;
  for (const auto& transceiver : transceivers()->ListInternal()) {
    cricket::ChannelInterface* channel = transceiver->channel();
    const ContentInfo* content =
        FindMediaSectionForTransceiver(transceiver, sdesc);
    if (!channel || !content) {
      continue;
    }

    const cricket::MediaType media_type = channel->media_type();
    if (media_type != cricket::MediaType::MEDIA_TYPE_AUDIO &&
        media_type != cricket::MediaType::MEDIA_TYPE_VIDEO) {
      continue;
    }

    RtpTransceiverDirection local_direction =
        content->media_description()->direction();
    if (source == cricket::CS_REMOTE) {
      local_direction = RtpTransceiverDirectionReversed(local_direction);
    }

    auto bundle_it = bundle_groups_by_mid.find(channel->mid());
    const cricket::ContentGroup* bundle_group =
        bundle_it != bundle_groups_by_mid.end() ? bundle_it->second : nullptr;
    bool pt_demux_enabled = RtpTransceiverDirectionHasRecv(local_direction);
    if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
      pt_demux_enabled &=
          !bundle_group ||
          (bundled_pt_demux_allowed_audio &&
           payload_types_by_bundle[bundle_group].pt_demuxing_possible_audio);
      if (pt_demux_enabled) {
        pt_demuxing_has_been_used_audio_ = true;
      }
    } else {
      RTC_DCHECK_EQ(media_type, cricket::MediaType::MEDIA_TYPE_VIDEO);
      pt_demux_enabled &=
          !bundle_group ||
          (bundled_pt_demux_allowed_video &&
           payload_types_by_bundle[bundle_group].pt_demuxing_possible_video);
      if (pt_demux_enabled) {
        pt_demuxing_has_been_used_video_ = true;
      }
    }

    channels_to_update.emplace_back(pt_demux_enabled, transceiver->channel());
  }

  if (channels_to_update.empty()) {
    return true;
  }

  return context_->worker_thread()->BlockingCall([&channels_to_update]() {
    for (const auto& it : channels_to_update) {
      if (!it.second->SetPayloadTypeDemuxingEnabled(it.first)) {

        return false;
      }
    }
    return true;
  });
}

bool SdpOfferAnswerHandler::ConfiguredForMedia() const {
  return context_->media_engine();
}

}
