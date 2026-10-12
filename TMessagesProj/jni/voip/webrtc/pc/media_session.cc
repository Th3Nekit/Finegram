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
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/base/media_engine.h"
#include "media/base/sdp_video_format_utils.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "pc/media_protocol_names.h"
#include "pc/rtp_media_utils.h"
#include "pc/used_ids.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/third_party/base64/base64.h"
#include "rtc_base/unique_id_generator.h"

namespace {

using rtc::UniqueRandomIdGenerator;
using webrtc::RTCError;
using webrtc::RTCErrorType;
using webrtc::RtpTransceiverDirection;

webrtc::RtpExtension RtpExtensionFromCapability(
    const webrtc::RtpHeaderExtensionCapability& capability) {
  return webrtc::RtpExtension(capability.uri,
                              capability.preferred_id.value_or(1));
}

cricket::RtpHeaderExtensions RtpHeaderExtensionsFromCapabilities(
    const std::vector<webrtc::RtpHeaderExtensionCapability>& capabilities) {
  cricket::RtpHeaderExtensions exts;
  for (const auto& capability : capabilities) {
    exts.push_back(RtpExtensionFromCapability(capability));
  }
  return exts;
}

std::vector<webrtc::RtpHeaderExtensionCapability>
UnstoppedRtpHeaderExtensionCapabilities(
    std::vector<webrtc::RtpHeaderExtensionCapability> capabilities) {
  capabilities.erase(
      std::remove_if(
          capabilities.begin(), capabilities.end(),
          [](const webrtc::RtpHeaderExtensionCapability& capability) {
            return capability.direction == RtpTransceiverDirection::kStopped;
          }),
      capabilities.end());
  return capabilities;
}

bool IsCapabilityPresent(const webrtc::RtpHeaderExtensionCapability& capability,
                         const cricket::RtpHeaderExtensions& extensions) {
  return std::find_if(extensions.begin(), extensions.end(),
                      [&capability](const webrtc::RtpExtension& extension) {
                        return capability.uri == extension.uri;
                      }) != extensions.end();
}

cricket::RtpHeaderExtensions UnstoppedOrPresentRtpHeaderExtensions(
    const std::vector<webrtc::RtpHeaderExtensionCapability>& capabilities,
    const cricket::RtpHeaderExtensions& unencrypted,
    const cricket::RtpHeaderExtensions& encrypted) {
  cricket::RtpHeaderExtensions extensions;
  for (const auto& capability : capabilities) {
    if (capability.direction != RtpTransceiverDirection::kStopped ||
        IsCapabilityPresent(capability, unencrypted) ||
        IsCapabilityPresent(capability, encrypted)) {
      extensions.push_back(RtpExtensionFromCapability(capability));
    }
  }
  return extensions;
}

}

namespace cricket {

namespace {

bool IsRtxCodec(const webrtc::RtpCodecCapability& capability) {
  return absl::EqualsIgnoreCase(capability.name, kRtxCodecName);
}

bool ContainsRtxCodec(const std::vector<Codec>& codecs) {
  return absl::c_find_if(codecs, [](const Codec& c) {
           return c.GetResiliencyType() == Codec::ResiliencyType::kRtx;
         }) != codecs.end();
}

bool IsRedCodec(const webrtc::RtpCodecCapability& capability) {
  return absl::EqualsIgnoreCase(capability.name, kRedCodecName);
}

bool ContainsFlexfecCodec(const std::vector<Codec>& codecs) {
  return absl::c_find_if(codecs, [](const Codec& c) {
           return c.GetResiliencyType() == Codec::ResiliencyType::kFlexfec;
         }) != codecs.end();
}

bool IsComfortNoiseCodec(const Codec& codec) {
  return absl::EqualsIgnoreCase(codec.name, kComfortNoiseCodecName);
}

void StripCNCodecs(AudioCodecs* audio_codecs) {
  audio_codecs->erase(std::remove_if(audio_codecs->begin(), audio_codecs->end(),
                                     [](const AudioCodec& codec) {
                                       return IsComfortNoiseCodec(codec);
                                     }),
                      audio_codecs->end());
}

RtpTransceiverDirection NegotiateRtpTransceiverDirection(
    RtpTransceiverDirection offer,
    RtpTransceiverDirection wants) {
  bool offer_send = webrtc::RtpTransceiverDirectionHasSend(offer);
  bool offer_recv = webrtc::RtpTransceiverDirectionHasRecv(offer);
  bool wants_send = webrtc::RtpTransceiverDirectionHasSend(wants);
  bool wants_recv = webrtc::RtpTransceiverDirectionHasRecv(wants);
  return webrtc::RtpTransceiverDirectionFromSendRecv(offer_recv && wants_send,
                                                     offer_send && wants_recv);
}

bool IsMediaContentOfType(const ContentInfo* content, MediaType media_type) {
  if (!content || !content->media_description()) {
    return false;
  }
  return content->media_description()->type() == media_type;
}

StreamParamsVec GetCurrentStreamParams(
    const std::vector<const ContentInfo*>& active_local_contents) {
  StreamParamsVec stream_params;
  for (const ContentInfo* content : active_local_contents) {
    for (const StreamParams& params : content->media_description()->streams()) {
      stream_params.push_back(params);
    }
  }
  return stream_params;
}

StreamParams CreateStreamParamsForNewSenderWithSsrcs(
    const SenderOptions& sender,
    const std::string& rtcp_cname,
    bool include_rtx_streams,
    bool include_flexfec_stream,
    UniqueRandomIdGenerator* ssrc_generator,
    const webrtc::FieldTrialsView& field_trials) {
  StreamParams result;
  result.id = sender.track_id;

  if (include_flexfec_stream && sender.num_sim_layers > 1) {
    include_flexfec_stream = false;
    RTC_LOG(LS_WARNING)
        << "Our FlexFEC implementation only supports protecting "
           "a single media streams. This session has multiple "
           "media streams however, so no FlexFEC SSRC will be generated.";
  }
  if (include_flexfec_stream && !field_trials.IsEnabled("WebRTC-FlexFEC-03")) {
    include_flexfec_stream = false;
    RTC_LOG(LS_WARNING)
        << "WebRTC-FlexFEC trial is not enabled, not sending FlexFEC";
  }

  result.GenerateSsrcs(sender.num_sim_layers, include_rtx_streams,
                       include_flexfec_stream, ssrc_generator);

  result.cname = rtcp_cname;
  result.set_stream_ids(sender.stream_ids);

  return result;
}

bool ValidateSimulcastLayers(const std::vector<RidDescription>& rids,
                             const SimulcastLayerList& simulcast_layers) {
  return absl::c_all_of(
      simulcast_layers.GetAllLayers(), [&rids](const SimulcastLayer& layer) {
        return absl::c_any_of(rids, [&layer](const RidDescription& rid) {
          return rid.rid == layer.rid;
        });
      });
}

StreamParams CreateStreamParamsForNewSenderWithRids(
    const SenderOptions& sender,
    const std::string& rtcp_cname) {
  RTC_DCHECK(!sender.rids.empty());
  RTC_DCHECK_EQ(sender.num_sim_layers, 0)
      << "RIDs are the compliant way to indicate simulcast.";
  RTC_DCHECK(ValidateSimulcastLayers(sender.rids, sender.simulcast_layers));
  StreamParams result;
  result.id = sender.track_id;
  result.cname = rtcp_cname;
  result.set_stream_ids(sender.stream_ids);

  if (sender.rids.size() > 1) {
    result.set_rids(sender.rids);
  }

  return result;
}

void AddSimulcastToMediaDescription(
    const MediaDescriptionOptions& media_description_options,
    MediaContentDescription* description) {
  RTC_DCHECK(description);

  if (absl::c_all_of(description->streams(), [](const StreamParams& params) {
        return !params.has_rids();
      })) {
    return;
  }

  RTC_DCHECK_EQ(1, description->streams().size())
      << "RIDs are only supported in Unified Plan semantics.";
  RTC_DCHECK_EQ(1, media_description_options.sender_options.size());
  RTC_DCHECK(description->type() == MediaType::MEDIA_TYPE_AUDIO ||
             description->type() == MediaType::MEDIA_TYPE_VIDEO);

  if (description->streams()[0].rids().size() <= 1) {
    return;
  }

  SimulcastDescription simulcast;
  simulcast.send_layers() =
      media_description_options.sender_options[0].simulcast_layers;
  description->set_simulcast_description(simulcast);
}

bool AddStreamParams(const std::vector<SenderOptions>& sender_options,
                     const std::string& rtcp_cname,
                     UniqueRandomIdGenerator* ssrc_generator,
                     StreamParamsVec* current_streams,
                     MediaContentDescription* content_description,
                     const webrtc::FieldTrialsView& field_trials) {

  if (IsSctpProtocol(content_description->protocol())) {
    return true;
  }

  const bool include_rtx_streams =
      ContainsRtxCodec(content_description->codecs());

  const bool include_flexfec_stream =
      ContainsFlexfecCodec(content_description->codecs());

  for (const SenderOptions& sender : sender_options) {
    StreamParams* param = GetStreamByIds(*current_streams, sender.track_id);
    if (!param) {

      StreamParams stream_param =
          sender.rids.empty()
              ?

              CreateStreamParamsForNewSenderWithSsrcs(
                  sender, rtcp_cname, include_rtx_streams,
                  include_flexfec_stream, ssrc_generator, field_trials)
              :

              CreateStreamParamsForNewSenderWithRids(sender, rtcp_cname);

      content_description->AddStream(stream_param);

      current_streams->push_back(stream_param);
    } else {

      param->set_stream_ids(sender.stream_ids);
      content_description->AddStream(*param);
    }
  }
  return true;
}

bool UpdateTransportInfoForBundle(const ContentGroup& bundle_group,
                                  SessionDescription* sdesc) {

  if (!sdesc || !bundle_group.FirstContentName()) {
    return false;
  }

  const std::string& selected_content_name = *bundle_group.FirstContentName();
  const TransportInfo* selected_transport_info =
      sdesc->GetTransportInfoByName(selected_content_name);
  if (!selected_transport_info) {
    return false;
  }

  const std::string& selected_ufrag =
      selected_transport_info->description.ice_ufrag;
  const std::string& selected_pwd =
      selected_transport_info->description.ice_pwd;
  ConnectionRole selected_connection_role =
      selected_transport_info->description.connection_role;
  for (TransportInfo& transport_info : sdesc->transport_infos()) {
    if (bundle_group.HasContentName(transport_info.content_name) &&
        transport_info.content_name != selected_content_name) {
      transport_info.description.ice_ufrag = selected_ufrag;
      transport_info.description.ice_pwd = selected_pwd;
      transport_info.description.connection_role = selected_connection_role;
    }
  }
  return true;
}

std::vector<const ContentInfo*> GetActiveContents(
    const SessionDescription& description,
    const MediaSessionOptions& session_options) {
  std::vector<const ContentInfo*> active_contents;
  for (size_t i = 0; i < description.contents().size(); ++i) {
    RTC_DCHECK_LT(i, session_options.media_description_options.size());
    const ContentInfo& content = description.contents()[i];
    const MediaDescriptionOptions& media_options =
        session_options.media_description_options[i];
    if (!content.rejected && !media_options.stopped &&
        content.name == media_options.mid) {
      active_contents.push_back(&content);
    }
  }
  return active_contents;
}

RTCError CreateContentOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const RtpHeaderExtensions& rtp_extensions,
    UniqueRandomIdGenerator* ssrc_generator,
    StreamParamsVec* current_streams,
    MediaContentDescription* offer) {
  offer->set_rtcp_mux(session_options.rtcp_mux_enabled);
  if (offer->type() == cricket::MEDIA_TYPE_VIDEO) {
    offer->set_rtcp_reduced_size(true);
  }

  RtpHeaderExtensions extensions;
  for (auto extension_with_id : rtp_extensions) {
    for (const auto& extension : media_description_options.header_extensions) {
      if (extension_with_id.uri == extension.uri) {

        if (extension.direction != RtpTransceiverDirection::kStopped) {
          extensions.push_back(extension_with_id);
        }
      }
    }
  }
  offer->set_rtp_header_extensions(extensions);

  AddSimulcastToMediaDescription(media_description_options, offer);

  return RTCError::OK();
}

RTCError CreateMediaContentOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const std::vector<Codec>& codecs,
    const RtpHeaderExtensions& rtp_extensions,
    UniqueRandomIdGenerator* ssrc_generator,
    StreamParamsVec* current_streams,
    MediaContentDescription* offer,
    const webrtc::FieldTrialsView& field_trials) {
  offer->AddCodecs(codecs);
  if (!AddStreamParams(media_description_options.sender_options,
                       session_options.rtcp_cname, ssrc_generator,
                       current_streams, offer, field_trials)) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to add stream parameters");
  }

  return CreateContentOffer(media_description_options, session_options,
                            rtp_extensions, ssrc_generator, current_streams,
                            offer);
}

bool ReferencedCodecsMatch(const std::vector<Codec>& codecs1,
                           const int codec1_id,
                           const std::vector<Codec>& codecs2,
                           const int codec2_id) {
  const Codec* codec1 = FindCodecById(codecs1, codec1_id);
  const Codec* codec2 = FindCodecById(codecs2, codec2_id);
  return codec1 != nullptr && codec2 != nullptr && codec1->Matches(*codec2);
}

void NegotiatePacketization(const Codec& local_codec,
                            const Codec& remote_codec,
                            Codec* negotiated_codec) {
  negotiated_codec->packetization =
      (local_codec.packetization == remote_codec.packetization)
          ? local_codec.packetization
          : absl::nullopt;
}

#ifdef RTC_ENABLE_H265
void NegotiateTxMode(const Codec& local_codec,
                     const Codec& remote_codec,
                     Codec* negotiated_codec) {
  negotiated_codec->tx_mode = (local_codec.tx_mode == remote_codec.tx_mode)
                                  ? local_codec.tx_mode
                                  : absl::nullopt;
}
#endif

absl::optional<Codec> FindMatchingCodec(const std::vector<Codec>& codecs1,
                                        const std::vector<Codec>& codecs2,
                                        const Codec& codec_to_match) {

  RTC_DCHECK(absl::c_any_of(codecs1, [&codec_to_match](const Codec& codec) {
    return &codec == &codec_to_match;
  }));
  for (const Codec& potential_match : codecs2) {
    if (potential_match.Matches(codec_to_match)) {
      if (codec_to_match.GetResiliencyType() == Codec::ResiliencyType::kRtx) {
        int apt_value_1 = 0;
        int apt_value_2 = 0;
        if (!codec_to_match.GetParam(kCodecParamAssociatedPayloadType,
                                     &apt_value_1) ||
            !potential_match.GetParam(kCodecParamAssociatedPayloadType,
                                      &apt_value_2)) {
          RTC_LOG(LS_WARNING) << "RTX missing associated payload type.";
          continue;
        }
        if (!ReferencedCodecsMatch(codecs1, apt_value_1, codecs2,
                                   apt_value_2)) {
          continue;
        }
      } else if (codec_to_match.GetResiliencyType() ==
                 Codec::ResiliencyType::kRed) {
        auto red_parameters_1 =
            codec_to_match.params.find(kCodecParamNotInNameValueFormat);
        auto red_parameters_2 =
            potential_match.params.find(kCodecParamNotInNameValueFormat);
        bool has_parameters_1 = red_parameters_1 != codec_to_match.params.end();
        bool has_parameters_2 =
            red_parameters_2 != potential_match.params.end();
        if (has_parameters_1 && has_parameters_2) {

          std::vector<absl::string_view> redundant_payloads_1 =
              rtc::split(red_parameters_1->second, '/');
          std::vector<absl::string_view> redundant_payloads_2 =
              rtc::split(red_parameters_2->second, '/');
          if (redundant_payloads_1.size() > 0 &&
              redundant_payloads_2.size() > 0) {
            bool consistent = true;
            for (size_t i = 1; i < redundant_payloads_1.size(); i++) {
              if (redundant_payloads_1[i] != redundant_payloads_1[0]) {
                consistent = false;
                break;
              }
            }
            for (size_t i = 1; i < redundant_payloads_2.size(); i++) {
              if (redundant_payloads_2[i] != redundant_payloads_2[0]) {
                consistent = false;
                break;
              }
            }
            if (!consistent) {
              continue;
            }

            int red_value_1;
            int red_value_2;
            if (rtc::FromString(redundant_payloads_1[0], &red_value_1) &&
                rtc::FromString(redundant_payloads_2[0], &red_value_2)) {
              if (!ReferencedCodecsMatch(codecs1, red_value_1, codecs2,
                                         red_value_2)) {
                continue;
              }
            }
          }
        } else if (has_parameters_1 != has_parameters_2) {
          continue;
        }
      }
      return potential_match;
    }
  }
  return absl::nullopt;
}

void NegotiateCodecs(const std::vector<Codec>& local_codecs,
                     const std::vector<Codec>& offered_codecs,
                     std::vector<Codec>* negotiated_codecs,
                     bool keep_offer_order) {
  for (const Codec& ours : local_codecs) {
    absl::optional<Codec> theirs =
        FindMatchingCodec(local_codecs, offered_codecs, ours);

    if (theirs) {
      Codec negotiated = ours;
      NegotiatePacketization(ours, *theirs, &negotiated);
      negotiated.IntersectFeedbackParams(*theirs);
      if (negotiated.GetResiliencyType() == Codec::ResiliencyType::kRtx) {
        const auto apt_it =
            theirs->params.find(kCodecParamAssociatedPayloadType);

        RTC_DCHECK(apt_it != theirs->params.end());
        negotiated.SetParam(kCodecParamAssociatedPayloadType, apt_it->second);

        const auto rtx_time_it = theirs->params.find(kCodecParamRtxTime);
        if (rtx_time_it != theirs->params.end()) {
          negotiated.SetParam(kCodecParamRtxTime, rtx_time_it->second);
        }
      } else if (negotiated.GetResiliencyType() ==
                 Codec::ResiliencyType::kRed) {
        const auto red_it =
            theirs->params.find(kCodecParamNotInNameValueFormat);
        if (red_it != theirs->params.end()) {
          negotiated.SetParam(kCodecParamNotInNameValueFormat, red_it->second);
        }
      }
      if (absl::EqualsIgnoreCase(ours.name, kH264CodecName)) {
        webrtc::H264GenerateProfileLevelIdForAnswer(ours.params, theirs->params,
                                                    &negotiated.params);
      }
#ifdef RTC_ENABLE_H265
      if (absl::EqualsIgnoreCase(ours.name, kH265CodecName)) {
        webrtc::H265GenerateProfileTierLevelForAnswer(
            ours.params, theirs->params, &negotiated.params);
        NegotiateTxMode(ours, *theirs, &negotiated);
      }
#endif
      negotiated.id = theirs->id;
      negotiated.name = theirs->name;
      negotiated_codecs->push_back(std::move(negotiated));
    }
  }
  if (keep_offer_order) {

    std::unordered_map<int, int> payload_type_preferences;
    int preference = static_cast<int>(offered_codecs.size() + 1);
    for (const Codec& codec : offered_codecs) {
      payload_type_preferences[codec.id] = preference--;
    }
    absl::c_sort(*negotiated_codecs, [&payload_type_preferences](
                                         const Codec& a, const Codec& b) {
      return payload_type_preferences[a.id] > payload_type_preferences[b.id];
    });
  }
}

const Codec* GetAssociatedCodecForRtx(const std::vector<Codec>& codec_list,
                                      const Codec& rtx_codec) {
  std::string associated_pt_str;
  if (!rtx_codec.GetParam(kCodecParamAssociatedPayloadType,
                          &associated_pt_str)) {
    RTC_LOG(LS_WARNING) << "RTX codec " << rtx_codec.id
                        << " is missing an associated payload type.";
    return nullptr;
  }

  int associated_pt;
  if (!rtc::FromString(associated_pt_str, &associated_pt)) {
    RTC_LOG(LS_WARNING) << "Couldn't convert payload type " << associated_pt_str
                        << " of RTX codec " << rtx_codec.id
                        << " to an integer.";
    return nullptr;
  }

  const Codec* associated_codec = FindCodecById(codec_list, associated_pt);
  if (!associated_codec) {
    RTC_LOG(LS_WARNING) << "Couldn't find associated codec with payload type "
                        << associated_pt << " for RTX codec " << rtx_codec.id
                        << ".";
  }
  return associated_codec;
}

const Codec* GetAssociatedCodecForRed(const std::vector<Codec>& codec_list,
                                      const Codec& red_codec) {
  std::string fmtp;
  if (!red_codec.GetParam(kCodecParamNotInNameValueFormat, &fmtp)) {

    if (red_codec.type == Codec::Type::kAudio) {
      RTC_LOG(LS_WARNING) << "RED codec " << red_codec.id
                          << " is missing an associated payload type.";
    }
    return nullptr;
  }

  std::vector<absl::string_view> redundant_payloads = rtc::split(fmtp, '/');
  if (redundant_payloads.size() < 2) {
    return nullptr;
  }

  absl::string_view associated_pt_str = redundant_payloads[0];
  int associated_pt;
  if (!rtc::FromString(associated_pt_str, &associated_pt)) {
    RTC_LOG(LS_WARNING) << "Couldn't convert first payload type "
                        << associated_pt_str << " of RED codec " << red_codec.id
                        << " to an integer.";
    return nullptr;
  }

  const Codec* associated_codec = FindCodecById(codec_list, associated_pt);
  if (!associated_codec) {
    RTC_LOG(LS_WARNING) << "Couldn't find associated codec with payload type "
                        << associated_pt << " for RED codec " << red_codec.id
                        << ".";
  }
  return associated_codec;
}

void MergeCodecs(const std::vector<Codec>& reference_codecs,
                 std::vector<Codec>* offered_codecs,
                 UsedPayloadTypes* used_pltypes) {

  for (const Codec& reference_codec : reference_codecs) {
    if (reference_codec.GetResiliencyType() != Codec::ResiliencyType::kRtx &&
        reference_codec.GetResiliencyType() != Codec::ResiliencyType::kRed &&
        !FindMatchingCodec(reference_codecs, *offered_codecs,
                           reference_codec)) {
      Codec codec = reference_codec;
      used_pltypes->FindAndSetIdUsed(&codec);
      offered_codecs->push_back(codec);
    }
  }

  for (const Codec& reference_codec : reference_codecs) {
    if (reference_codec.GetResiliencyType() == Codec::ResiliencyType::kRtx &&
        !FindMatchingCodec(reference_codecs, *offered_codecs,
                           reference_codec)) {
      Codec rtx_codec = reference_codec;
      const Codec* associated_codec =
          GetAssociatedCodecForRtx(reference_codecs, rtx_codec);
      if (!associated_codec) {
        continue;
      }

      absl::optional<Codec> matching_codec = FindMatchingCodec(
          reference_codecs, *offered_codecs, *associated_codec);
      if (!matching_codec) {
        RTC_LOG(LS_WARNING)
            << "Couldn't find matching " << associated_codec->name << " codec.";
        continue;
      }

      rtx_codec.params[kCodecParamAssociatedPayloadType] =
          rtc::ToString(matching_codec->id);
      used_pltypes->FindAndSetIdUsed(&rtx_codec);
      offered_codecs->push_back(rtx_codec);
    } else if (reference_codec.GetResiliencyType() ==
                   Codec::ResiliencyType::kRed &&
               !FindMatchingCodec(reference_codecs, *offered_codecs,
                                  reference_codec)) {
      Codec red_codec = reference_codec;
      const Codec* associated_codec =
          GetAssociatedCodecForRed(reference_codecs, red_codec);
      if (associated_codec) {
        absl::optional<Codec> matching_codec = FindMatchingCodec(
            reference_codecs, *offered_codecs, *associated_codec);
        if (!matching_codec) {
          RTC_LOG(LS_WARNING) << "Couldn't find matching "
                              << associated_codec->name << " codec.";
          continue;
        }

        red_codec.params[kCodecParamNotInNameValueFormat] =
            rtc::ToString(matching_codec->id) + "/" +
            rtc::ToString(matching_codec->id);
      }
      used_pltypes->FindAndSetIdUsed(&red_codec);
      offered_codecs->push_back(red_codec);
    }
  }
}

std::vector<Codec> MatchCodecPreference(
    const std::vector<webrtc::RtpCodecCapability>& codec_preferences,
    const std::vector<Codec>& codecs,
    const std::vector<Codec>& supported_codecs) {
  std::vector<Codec> filtered_codecs;
  bool want_rtx = false;
  bool want_red = false;

  for (const auto& codec_preference : codec_preferences) {
    if (IsRtxCodec(codec_preference)) {
      want_rtx = true;
    } else if (IsRedCodec(codec_preference)) {
      want_red = true;
    }
  }
  bool red_was_added = false;
  for (const auto& codec_preference : codec_preferences) {
    auto found_codec = absl::c_find_if(
        supported_codecs, [&codec_preference](const Codec& codec) {
          webrtc::RtpCodecParameters codec_parameters =
              codec.ToCodecParameters();
          return codec_parameters.name == codec_preference.name &&
                 codec_parameters.kind == codec_preference.kind &&
                 codec_parameters.num_channels ==
                     codec_preference.num_channels &&
                 codec_parameters.clock_rate == codec_preference.clock_rate &&
                 codec_parameters.parameters == codec_preference.parameters;
        });

    if (found_codec != supported_codecs.end()) {
      absl::optional<Codec> found_codec_with_correct_pt =
          FindMatchingCodec(supported_codecs, codecs, *found_codec);
      if (found_codec_with_correct_pt) {

        bool is_red_codec = found_codec_with_correct_pt->GetResiliencyType() ==
                            Codec::ResiliencyType::kRed;
        if (!is_red_codec || !red_was_added) {
          filtered_codecs.push_back(*found_codec_with_correct_pt);
          red_was_added = is_red_codec ? true : red_was_added;
        }
        std::string id = rtc::ToString(found_codec_with_correct_pt->id);

        if (want_red || want_rtx) {
          for (const auto& codec : codecs) {
            if (codec.GetResiliencyType() == Codec::ResiliencyType::kRtx) {
              const auto apt =
                  codec.params.find(cricket::kCodecParamAssociatedPayloadType);
              if (apt != codec.params.end() && apt->second == id) {
                filtered_codecs.push_back(codec);
                break;
              }
            } else if (codec.GetResiliencyType() ==
                       Codec::ResiliencyType::kRed) {

              const auto fmtp =
                  codec.params.find(cricket::kCodecParamNotInNameValueFormat);
              if (fmtp != codec.params.end()) {
                std::vector<absl::string_view> redundant_payloads =
                    rtc::split(fmtp->second, '/');
                if (!redundant_payloads.empty() &&
                    redundant_payloads[0] == id) {
                  if (!red_was_added) {
                    filtered_codecs.push_back(codec);
                    red_was_added = true;
                  }
                  break;
                }
              }
            }
          }
        }
      }
    }
  }

  return filtered_codecs;
}

std::vector<Codec> ComputeCodecsUnion(const std::vector<Codec>& codecs1,
                                      const std::vector<Codec>& codecs2) {
  std::vector<Codec> all_codecs;
  UsedPayloadTypes used_payload_types;
  for (const Codec& codec : codecs1) {
    Codec codec_mutable = codec;
    used_payload_types.FindAndSetIdUsed(&codec_mutable);
    all_codecs.push_back(codec_mutable);
  }

  MergeCodecs(codecs2, &all_codecs, &used_payload_types);

  return all_codecs;
}

void MergeRtpHdrExts(const RtpHeaderExtensions& reference_extensions,
                     RtpHeaderExtensions* offered_extensions,
                     RtpHeaderExtensions* regular_extensions,
                     RtpHeaderExtensions* encrypted_extensions,
                     UsedRtpHeaderExtensionIds* used_ids) {
  for (auto reference_extension : reference_extensions) {
    if (!webrtc::RtpExtension::FindHeaderExtensionByUriAndEncryption(
            *offered_extensions, reference_extension.uri,
            reference_extension.encrypt)) {
      if (reference_extension.encrypt) {
        const webrtc::RtpExtension* existing =
            webrtc::RtpExtension::FindHeaderExtensionByUriAndEncryption(
                *encrypted_extensions, reference_extension.uri,
                reference_extension.encrypt);
        if (existing) {
          offered_extensions->push_back(*existing);
        } else {
          used_ids->FindAndSetIdUsed(&reference_extension);
          encrypted_extensions->push_back(reference_extension);
          offered_extensions->push_back(reference_extension);
        }
      } else {
        const webrtc::RtpExtension* existing =
            webrtc::RtpExtension::FindHeaderExtensionByUriAndEncryption(
                *regular_extensions, reference_extension.uri,
                reference_extension.encrypt);
        if (existing) {
          offered_extensions->push_back(*existing);
        } else {
          used_ids->FindAndSetIdUsed(&reference_extension);
          regular_extensions->push_back(reference_extension);
          offered_extensions->push_back(reference_extension);
        }
      }
    }
  }
}

void AddEncryptedVersionsOfHdrExts(RtpHeaderExtensions* offered_extensions,
                                   RtpHeaderExtensions* encrypted_extensions,
                                   UsedRtpHeaderExtensionIds* used_ids) {
  RtpHeaderExtensions encrypted_extensions_to_add;
  for (const auto& extension : *offered_extensions) {

    if (extension.encrypt) {
      continue;
    }

    if (!webrtc::RtpExtension::IsEncryptionSupported(extension.uri)) {
      continue;
    }

    const bool have_encrypted_extension =
        webrtc::RtpExtension::FindHeaderExtensionByUriAndEncryption(
            *offered_extensions, extension.uri, true);
    if (have_encrypted_extension) {
      continue;
    }

    const webrtc::RtpExtension* shared_encrypted_extension =
        webrtc::RtpExtension::FindHeaderExtensionByUriAndEncryption(
            *encrypted_extensions, extension.uri, true);
    if (shared_encrypted_extension) {

      encrypted_extensions_to_add.push_back(*shared_encrypted_extension);
      continue;
    }

    webrtc::RtpExtension new_encrypted_extension(extension);
    new_encrypted_extension.encrypt = true;
    used_ids->FindAndSetIdUsed(&new_encrypted_extension);
    encrypted_extensions->push_back(new_encrypted_extension);
    encrypted_extensions_to_add.push_back(new_encrypted_extension);
  }

  offered_extensions->insert(offered_extensions->end(),
                             encrypted_extensions_to_add.begin(),
                             encrypted_extensions_to_add.end());
}

const webrtc::RtpExtension* FindHeaderExtensionByUriDiscardUnsupported(
    const std::vector<webrtc::RtpExtension>& extensions,
    absl::string_view uri,
    webrtc::RtpExtension::Filter filter) {

  if (!webrtc::RtpExtension::IsEncryptionSupported(uri)) {

    if (filter == webrtc::RtpExtension::kRequireEncryptedExtension) {
      return nullptr;
    }

    filter = webrtc::RtpExtension::Filter::kDiscardEncryptedExtension;
  }

  return webrtc::RtpExtension::FindHeaderExtensionByUri(extensions, uri,
                                                        filter);
}

void NegotiateRtpHeaderExtensions(const RtpHeaderExtensions& local_extensions,
                                  const RtpHeaderExtensions& offered_extensions,
                                  webrtc::RtpExtension::Filter filter,
                                  RtpHeaderExtensions* negotiated_extensions) {
  bool frame_descriptor_in_local = false;
  bool dependency_descriptor_in_local = false;
  bool abs_capture_time_in_local = false;

  for (const webrtc::RtpExtension& ours : local_extensions) {
    if (ours.uri == webrtc::RtpExtension::kGenericFrameDescriptorUri00)
      frame_descriptor_in_local = true;
    else if (ours.uri == webrtc::RtpExtension::kDependencyDescriptorUri)
      dependency_descriptor_in_local = true;
    else if (ours.uri == webrtc::RtpExtension::kAbsoluteCaptureTimeUri)
      abs_capture_time_in_local = true;

    const webrtc::RtpExtension* theirs =
        FindHeaderExtensionByUriDiscardUnsupported(offered_extensions, ours.uri,
                                                   filter);
    if (theirs) {

      negotiated_extensions->push_back(*theirs);
    }
  }

  if (!dependency_descriptor_in_local) {
    const webrtc::RtpExtension* theirs =
        FindHeaderExtensionByUriDiscardUnsupported(
            offered_extensions, webrtc::RtpExtension::kDependencyDescriptorUri,
            filter);
    if (theirs) {
      negotiated_extensions->push_back(*theirs);
    }
  }
  if (!frame_descriptor_in_local) {
    const webrtc::RtpExtension* theirs =
        FindHeaderExtensionByUriDiscardUnsupported(
            offered_extensions,
            webrtc::RtpExtension::kGenericFrameDescriptorUri00, filter);
    if (theirs) {
      negotiated_extensions->push_back(*theirs);
    }
  }

  if (!abs_capture_time_in_local) {
    const webrtc::RtpExtension* theirs =
        FindHeaderExtensionByUriDiscardUnsupported(
            offered_extensions, webrtc::RtpExtension::kAbsoluteCaptureTimeUri,
            filter);
    if (theirs) {
      negotiated_extensions->push_back(*theirs);
    }
  }
}

bool SetCodecsInAnswer(const MediaContentDescription* offer,
                       const std::vector<Codec>& local_codecs,
                       const MediaDescriptionOptions& media_description_options,
                       const MediaSessionOptions& session_options,
                       UniqueRandomIdGenerator* ssrc_generator,
                       StreamParamsVec* current_streams,
                       MediaContentDescription* answer,
                       const webrtc::FieldTrialsView& field_trials) {
  RTC_DCHECK(offer->type() == MEDIA_TYPE_AUDIO ||
             offer->type() == MEDIA_TYPE_VIDEO);
  std::vector<Codec> negotiated_codecs;
  NegotiateCodecs(local_codecs, offer->codecs(), &negotiated_codecs,
                  media_description_options.codec_preferences.empty());
  answer->AddCodecs(negotiated_codecs);
  answer->set_protocol(offer->protocol());
  if (!AddStreamParams(media_description_options.sender_options,
                       session_options.rtcp_cname, ssrc_generator,
                       current_streams, answer, field_trials)) {
    return false;
  }
  return true;
}

bool CreateMediaContentAnswer(
    const MediaContentDescription* offer,
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const RtpHeaderExtensions& local_rtp_extensions,
    UniqueRandomIdGenerator* ssrc_generator,
    bool enable_encrypted_rtp_header_extensions,
    StreamParamsVec* current_streams,
    bool bundle_enabled,
    MediaContentDescription* answer) {
  answer->set_extmap_allow_mixed_enum(offer->extmap_allow_mixed_enum());
  const webrtc::RtpExtension::Filter extensions_filter =
      enable_encrypted_rtp_header_extensions
          ? webrtc::RtpExtension::Filter::kPreferEncryptedExtension
          : webrtc::RtpExtension::Filter::kDiscardEncryptedExtension;

  RtpHeaderExtensions local_rtp_extensions_to_reply_with;
  for (auto extension_with_id : local_rtp_extensions) {
    for (const auto& extension : media_description_options.header_extensions) {
      if (extension_with_id.uri == extension.uri) {

        if (extension.direction != RtpTransceiverDirection::kStopped) {
          local_rtp_extensions_to_reply_with.push_back(extension_with_id);
        }
      }
    }
  }
  RtpHeaderExtensions negotiated_rtp_extensions;
  NegotiateRtpHeaderExtensions(local_rtp_extensions_to_reply_with,
                               offer->rtp_header_extensions(),
                               extensions_filter, &negotiated_rtp_extensions);
  answer->set_rtp_header_extensions(negotiated_rtp_extensions);

  answer->set_rtcp_mux(session_options.rtcp_mux_enabled && offer->rtcp_mux());
  if (answer->type() == cricket::MEDIA_TYPE_VIDEO) {
    answer->set_rtcp_reduced_size(offer->rtcp_reduced_size());
  }

  answer->set_remote_estimate(offer->remote_estimate());

  AddSimulcastToMediaDescription(media_description_options, answer);

  answer->set_direction(NegotiateRtpTransceiverDirection(
      offer->direction(), media_description_options.direction));

  return true;
}

bool IsMediaProtocolSupported(MediaType type,
                              const std::string& protocol,
                              bool secure_transport) {

  if (protocol.empty()) {
    return true;
  }

  if (type == MEDIA_TYPE_DATA) {

    if (secure_transport) {

      return IsDtlsSctp(protocol);
    } else {
      return IsPlainSctp(protocol);
    }
  }

  if (secure_transport) {

    return IsDtlsRtp(protocol) || IsPlainRtp(protocol);
  } else {
    return IsPlainRtp(protocol);
  }
}

void SetMediaProtocol(bool secure_transport, MediaContentDescription* desc) {
  if (secure_transport)
    desc->set_protocol(kMediaProtocolDtlsSavpf);
  else
    desc->set_protocol(kMediaProtocolAvpf);
}

const TransportDescription* GetTransportDescription(
    const std::string& content_name,
    const SessionDescription* current_description) {
  const TransportDescription* desc = NULL;
  if (current_description) {
    const TransportInfo* info =
        current_description->GetTransportInfoByName(content_name);
    if (info) {
      desc = &info->description;
    }
  }
  return desc;
}

webrtc::RTCErrorOr<AudioCodecs> GetNegotiatedCodecsForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const std::vector<Codec>& codecs,
    const std::vector<Codec>& supported_codecs) {
  std::vector<Codec> filtered_codecs;
  if (!media_description_options.codec_preferences.empty()) {

    filtered_codecs = MatchCodecPreference(
        media_description_options.codec_preferences, codecs, supported_codecs);
  } else {

    if (current_content && !current_content->rejected &&
        current_content->name == media_description_options.mid) {
      if (!IsMediaContentOfType(current_content,
                                media_description_options.type)) {

        LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                             "Media type for content with mid='" +
                                 current_content->name +
                                 "' does not match previous type.");
      }
      const MediaContentDescription* mcd = current_content->media_description();
      for (const Codec& codec : mcd->codecs()) {
        if (FindMatchingCodec(mcd->codecs(), codecs, codec)) {
          filtered_codecs.push_back(codec);
        }
      }
    }

    for (const Codec& codec : supported_codecs) {
      absl::optional<Codec> found_codec =
          FindMatchingCodec(supported_codecs, codecs, codec);
      if (found_codec &&
          !FindMatchingCodec(supported_codecs, filtered_codecs, codec)) {

        if (media_description_options.type == MEDIA_TYPE_VIDEO &&
            found_codec->GetResiliencyType() == Codec::ResiliencyType::kRtx) {

          auto referenced_codec =
              GetAssociatedCodecForRtx(supported_codecs, codec);
          RTC_DCHECK(referenced_codec);

          absl::optional<Codec> changed_referenced_codec = FindMatchingCodec(
              supported_codecs, filtered_codecs, *referenced_codec);
          if (changed_referenced_codec) {
            found_codec->SetParam(kCodecParamAssociatedPayloadType,
                                  changed_referenced_codec->id);
          }
        }
        filtered_codecs.push_back(*found_codec);
      }
    }
  }

  if (media_description_options.type == MEDIA_TYPE_AUDIO &&
      !session_options.vad_enabled) {

    StripCNCodecs(&filtered_codecs);
  } else if (media_description_options.type == MEDIA_TYPE_VIDEO &&
             session_options.raw_packetization_for_video) {
    for (Codec& codec : filtered_codecs) {
      if (codec.IsMediaCodec()) {
        codec.packetization = kPacketizationParamRaw;
      }
    }
  }
  return filtered_codecs;
}

webrtc::RTCErrorOr<AudioCodecs> GetNegotiatedCodecsForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const std::vector<Codec>& codecs,
    const std::vector<Codec>& supported_codecs) {
  std::vector<Codec> filtered_codecs;

  if (!media_description_options.codec_preferences.empty()) {
    filtered_codecs = MatchCodecPreference(
        media_description_options.codec_preferences, codecs, supported_codecs);
  } else {

    if (current_content && !current_content->rejected &&
        current_content->name == media_description_options.mid) {
      if (!IsMediaContentOfType(current_content,
                                media_description_options.type)) {

        LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                             "Media type for content with mid='" +
                                 current_content->name +
                                 "' does not match previous type.");
      }
      const MediaContentDescription* mcd = current_content->media_description();
      for (const Codec& codec : mcd->codecs()) {
        if (FindMatchingCodec(mcd->codecs(), codecs, codec)) {
          filtered_codecs.push_back(codec);
        }
      }
    }

    std::vector<Codec> other_codecs;
    for (const Codec& codec : supported_codecs) {
      if (FindMatchingCodec(supported_codecs, codecs, codec) &&
          !FindMatchingCodec(supported_codecs, filtered_codecs, codec)) {

        other_codecs.push_back(codec);
      }
    }

    filtered_codecs = ComputeCodecsUnion(filtered_codecs, other_codecs);
  }

  if (media_description_options.type == MEDIA_TYPE_AUDIO &&
      !session_options.vad_enabled) {

    StripCNCodecs(&filtered_codecs);
  } else if (media_description_options.type == MEDIA_TYPE_VIDEO &&
             session_options.raw_packetization_for_video) {
    for (Codec& codec : filtered_codecs) {
      if (codec.IsMediaCodec()) {
        codec.packetization = kPacketizationParamRaw;
      }
    }
  }
  return filtered_codecs;
}

}

void MediaDescriptionOptions::AddAudioSender(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids) {
  RTC_DCHECK(type == MEDIA_TYPE_AUDIO);
  AddSenderInternal(track_id, stream_ids, {}, SimulcastLayerList(), 1);
}

void MediaDescriptionOptions::AddVideoSender(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    const std::vector<RidDescription>& rids,
    const SimulcastLayerList& simulcast_layers,
    int num_sim_layers) {
  RTC_DCHECK(type == MEDIA_TYPE_VIDEO);
  RTC_DCHECK(rids.empty() || num_sim_layers == 0)
      << "RIDs are the compliant way to indicate simulcast.";
  RTC_DCHECK(ValidateSimulcastLayers(rids, simulcast_layers));
  AddSenderInternal(track_id, stream_ids, rids, simulcast_layers,
                    num_sim_layers);
}

void MediaDescriptionOptions::AddSenderInternal(
    const std::string& track_id,
    const std::vector<std::string>& stream_ids,
    const std::vector<RidDescription>& rids,
    const SimulcastLayerList& simulcast_layers,
    int num_sim_layers) {

  RTC_CHECK(stream_ids.size() == 1U);
  SenderOptions options;
  options.track_id = track_id;
  options.stream_ids = stream_ids;
  options.simulcast_layers = simulcast_layers;
  options.rids = rids;
  options.num_sim_layers = num_sim_layers;
  sender_options.push_back(options);
}

bool MediaSessionOptions::HasMediaDescription(MediaType type) const {
  return absl::c_any_of(
      media_description_options,
      [type](const MediaDescriptionOptions& t) { return t.type == type; });
}

MediaSessionDescriptionFactory::MediaSessionDescriptionFactory(
    cricket::MediaEngineInterface* media_engine,
    bool rtx_enabled,
    rtc::UniqueRandomIdGenerator* ssrc_generator,
    const TransportDescriptionFactory* transport_desc_factory)
    : ssrc_generator_(ssrc_generator),
      transport_desc_factory_(transport_desc_factory) {
  RTC_CHECK(transport_desc_factory_);
  if (media_engine) {
    audio_send_codecs_ = media_engine->voice().send_codecs();
    audio_recv_codecs_ = media_engine->voice().recv_codecs();
    video_send_codecs_ = media_engine->video().send_codecs(rtx_enabled);
    video_recv_codecs_ = media_engine->video().recv_codecs(rtx_enabled);
  }
  ComputeAudioCodecsIntersectionAndUnion();
  ComputeVideoCodecsIntersectionAndUnion();
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_sendrecv_codecs()
    const {
  return audio_sendrecv_codecs_;
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_send_codecs() const {
  return audio_send_codecs_;
}

const AudioCodecs& MediaSessionDescriptionFactory::audio_recv_codecs() const {
  return audio_recv_codecs_;
}

void MediaSessionDescriptionFactory::set_audio_codecs(
    const AudioCodecs& send_codecs,
    const AudioCodecs& recv_codecs) {
  audio_send_codecs_ = send_codecs;
  audio_recv_codecs_ = recv_codecs;
  ComputeAudioCodecsIntersectionAndUnion();
}

const VideoCodecs& MediaSessionDescriptionFactory::video_sendrecv_codecs()
    const {
  return video_sendrecv_codecs_;
}

const VideoCodecs& MediaSessionDescriptionFactory::video_send_codecs() const {
  return video_send_codecs_;
}

const VideoCodecs& MediaSessionDescriptionFactory::video_recv_codecs() const {
  return video_recv_codecs_;
}

void MediaSessionDescriptionFactory::set_video_codecs(
    const VideoCodecs& send_codecs,
    const VideoCodecs& recv_codecs) {
  video_send_codecs_ = send_codecs;
  video_recv_codecs_ = recv_codecs;
  ComputeVideoCodecsIntersectionAndUnion();
}

RtpHeaderExtensions
MediaSessionDescriptionFactory::filtered_rtp_header_extensions(
    RtpHeaderExtensions extensions) const {
  if (!is_unified_plan_) {

    extensions.erase(
        std::remove_if(
            extensions.begin(), extensions.end(),
            [](const webrtc::RtpExtension& extension) {
              return extension.uri == webrtc::RtpExtension::kMidUri ||
                     extension.uri == webrtc::RtpExtension::kRidUri ||
                     extension.uri == webrtc::RtpExtension::kRepairedRidUri;
            }),
        extensions.end());
  }
  return extensions;
}

webrtc::RTCErrorOr<std::unique_ptr<SessionDescription>>
MediaSessionDescriptionFactory::CreateOfferOrError(
    const MediaSessionOptions& session_options,
    const SessionDescription* current_description) const {

  if (current_description) {
    RTC_DCHECK_LE(current_description->contents().size(),
                  session_options.media_description_options.size());
  }

  IceCredentialsIterator ice_credentials(
      session_options.pooled_ice_credentials);

  std::vector<const ContentInfo*> current_active_contents;
  if (current_description) {
    current_active_contents =
        GetActiveContents(*current_description, session_options);
  }

  StreamParamsVec current_streams =
      GetCurrentStreamParams(current_active_contents);

  AudioCodecs offer_audio_codecs;
  VideoCodecs offer_video_codecs;
  GetCodecsForOffer(current_active_contents, &offer_audio_codecs,
                    &offer_video_codecs);
  AudioVideoRtpHeaderExtensions extensions_with_ids =
      GetOfferedRtpHeaderExtensionsWithIds(
          current_active_contents, session_options.offer_extmap_allow_mixed,
          session_options.media_description_options);

  auto offer = std::make_unique<SessionDescription>();

  size_t msection_index = 0;
  for (const MediaDescriptionOptions& media_description_options :
       session_options.media_description_options) {
    const ContentInfo* current_content = nullptr;
    if (current_description &&
        msection_index < current_description->contents().size()) {
      current_content = &current_description->contents()[msection_index];

    }
    RTCError error;
    switch (media_description_options.type) {
      case MEDIA_TYPE_AUDIO:
      case MEDIA_TYPE_VIDEO:
        error = AddRtpContentForOffer(
            media_description_options, session_options, current_content,
            current_description,
            media_description_options.type == MEDIA_TYPE_AUDIO
                ? extensions_with_ids.audio
                : extensions_with_ids.video,
            media_description_options.type == MEDIA_TYPE_AUDIO
                ? offer_audio_codecs
                : offer_video_codecs,
            &current_streams, offer.get(), &ice_credentials);
        break;
      case MEDIA_TYPE_DATA:
        error = AddDataContentForOffer(media_description_options,
                                       session_options, current_content,
                                       current_description, &current_streams,
                                       offer.get(), &ice_credentials);
        break;
      case MEDIA_TYPE_UNSUPPORTED:
        error = AddUnsupportedContentForOffer(
            media_description_options, session_options, current_content,
            current_description, offer.get(), &ice_credentials);
        break;
      default:
        RTC_DCHECK_NOTREACHED();
    }
    if (!error.ok()) {
      return error;
    }
    ++msection_index;
  }

  if (session_options.bundle_enabled) {
    ContentGroup offer_bundle(GROUP_TYPE_BUNDLE);
    for (const ContentInfo& content : offer->contents()) {
      if (content.rejected) {
        continue;
      }

      offer_bundle.AddContentName(content.name);
    }
    if (!offer_bundle.content_names().empty()) {
      offer->AddGroup(offer_bundle);
      if (!UpdateTransportInfoForBundle(offer_bundle, offer.get())) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INTERNAL_ERROR,
            "CreateOffer failed to UpdateTransportInfoForBundle");
      }
    }
  }

  if (is_unified_plan_) {

    offer->set_msid_signaling(cricket::kMsidSignalingSemantic |
                              cricket::kMsidSignalingMediaSection |
                              cricket::kMsidSignalingSsrcAttribute);
  } else {

    offer->set_msid_signaling(cricket::kMsidSignalingSemantic |
                              cricket::kMsidSignalingSsrcAttribute);
  }

  offer->set_extmap_allow_mixed(session_options.offer_extmap_allow_mixed);

  return offer;
}

webrtc::RTCErrorOr<std::unique_ptr<SessionDescription>>
MediaSessionDescriptionFactory::CreateAnswerOrError(
    const SessionDescription* offer,
    const MediaSessionOptions& session_options,
    const SessionDescription* current_description) const {
  if (!offer) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR, "Called without offer.");
  }

  RTC_DCHECK_EQ(offer->contents().size(),
                session_options.media_description_options.size());

  IceCredentialsIterator ice_credentials(
      session_options.pooled_ice_credentials);

  std::vector<const ContentInfo*> current_active_contents;
  if (current_description) {
    current_active_contents =
        GetActiveContents(*current_description, session_options);
  }

  StreamParamsVec current_streams =
      GetCurrentStreamParams(current_active_contents);

  AudioCodecs answer_audio_codecs;
  VideoCodecs answer_video_codecs;
  GetCodecsForAnswer(current_active_contents, *offer, &answer_audio_codecs,
                     &answer_video_codecs);

  auto answer = std::make_unique<SessionDescription>();

  std::vector<const ContentGroup*> offer_bundles =
      offer->GetGroupsByName(GROUP_TYPE_BUNDLE);

  std::vector<ContentGroup> answer_bundles;
  std::vector<std::unique_ptr<TransportInfo>> bundle_transports;
  answer_bundles.reserve(offer_bundles.size());
  bundle_transports.reserve(offer_bundles.size());
  for (size_t i = 0; i < offer_bundles.size(); ++i) {
    answer_bundles.emplace_back(GROUP_TYPE_BUNDLE);
    bundle_transports.emplace_back(nullptr);
  }

  answer->set_extmap_allow_mixed(offer->extmap_allow_mixed());

  size_t msection_index = 0;
  for (const MediaDescriptionOptions& media_description_options :
       session_options.media_description_options) {
    const ContentInfo* offer_content = &offer->contents()[msection_index];

    RTC_DCHECK(
        IsMediaContentOfType(offer_content, media_description_options.type));
    RTC_DCHECK(media_description_options.mid == offer_content->name);

    absl::optional<size_t> bundle_index;
    for (size_t i = 0; i < offer_bundles.size(); ++i) {
      if (offer_bundles[i]->HasContentName(media_description_options.mid)) {
        bundle_index = i;
        break;
      }
    }
    TransportInfo* bundle_transport =
        bundle_index.has_value() ? bundle_transports[bundle_index.value()].get()
                                 : nullptr;

    const ContentInfo* current_content = nullptr;
    if (current_description &&
        msection_index < current_description->contents().size()) {
      current_content = &current_description->contents()[msection_index];
    }
    RtpHeaderExtensions header_extensions = RtpHeaderExtensionsFromCapabilities(
        UnstoppedRtpHeaderExtensionCapabilities(
            media_description_options.header_extensions));
    RTCError error;
    switch (media_description_options.type) {
      case MEDIA_TYPE_AUDIO:
      case MEDIA_TYPE_VIDEO:
        error = AddRtpContentForAnswer(
            media_description_options, session_options, offer_content, offer,
            current_content, current_description, bundle_transport,
            media_description_options.type == MEDIA_TYPE_AUDIO
                ? answer_audio_codecs
                : answer_video_codecs,
            header_extensions, &current_streams, answer.get(),
            &ice_credentials);
        break;
      case MEDIA_TYPE_DATA:
        error = AddDataContentForAnswer(
            media_description_options, session_options, offer_content, offer,
            current_content, current_description, bundle_transport,
            &current_streams, answer.get(), &ice_credentials);
        break;
      case MEDIA_TYPE_UNSUPPORTED:
        error = AddUnsupportedContentForAnswer(
            media_description_options, session_options, offer_content, offer,
            current_content, current_description, bundle_transport,
            answer.get(), &ice_credentials);
        break;
      default:
        RTC_DCHECK_NOTREACHED();
    }
    if (!error.ok()) {
      return error;
    }
    ++msection_index;

    ContentInfo& added = answer->contents().back();
    if (!added.rejected && session_options.bundle_enabled &&
        bundle_index.has_value()) {

      RTC_DCHECK_EQ(media_description_options.mid, added.name);
      answer_bundles[bundle_index.value()].AddContentName(added.name);
      bundle_transports[bundle_index.value()].reset(
          new TransportInfo(*answer->GetTransportInfoByName(added.name)));
    }
  }

  if (!offer_bundles.empty()) {
    for (const ContentGroup& answer_bundle : answer_bundles) {
      answer->AddGroup(answer_bundle);

      if (answer_bundle.FirstContentName()) {

        if (!UpdateTransportInfoForBundle(answer_bundle, answer.get())) {
          LOG_AND_RETURN_ERROR(
              RTCErrorType::INTERNAL_ERROR,
              "CreateAnswer failed to UpdateTransportInfoForBundle.");
        }
      }
    }
  }

  if (is_unified_plan_) {

    int msid_signaling = offer->msid_signaling();
    if (msid_signaling ==
        (cricket::kMsidSignalingSemantic | cricket::kMsidSignalingMediaSection |
         cricket::kMsidSignalingSsrcAttribute)) {

      answer->set_msid_signaling(cricket::kMsidSignalingSemantic |
                                 cricket::kMsidSignalingMediaSection);
    } else if (msid_signaling == cricket::kMsidSignalingSemantic) {

      answer->set_msid_signaling(cricket::kMsidSignalingSemantic |
                                 cricket::kMsidSignalingMediaSection |
                                 cricket::kMsidSignalingSsrcAttribute);
    } else {

      answer->set_msid_signaling(msid_signaling);
    }
  } else {

    answer->set_msid_signaling(cricket::kMsidSignalingSemantic |
                               cricket::kMsidSignalingSsrcAttribute);
  }

  return answer;
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForOffer(
    const RtpTransceiverDirection& direction) const {
  switch (direction) {

    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kStopped:
    case RtpTransceiverDirection::kInactive:
      return audio_sendrecv_codecs_;
    case RtpTransceiverDirection::kSendOnly:
      return audio_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return audio_recv_codecs_;
  }
  RTC_CHECK_NOTREACHED();
}

const AudioCodecs& MediaSessionDescriptionFactory::GetAudioCodecsForAnswer(
    const RtpTransceiverDirection& offer,
    const RtpTransceiverDirection& answer) const {
  switch (answer) {

    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kStopped:
    case RtpTransceiverDirection::kInactive:
      return GetAudioCodecsForOffer(
          webrtc::RtpTransceiverDirectionReversed(offer));
    case RtpTransceiverDirection::kSendOnly:
      return audio_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return audio_recv_codecs_;
  }
  RTC_CHECK_NOTREACHED();
}

const VideoCodecs& MediaSessionDescriptionFactory::GetVideoCodecsForOffer(
    const RtpTransceiverDirection& direction) const {
  switch (direction) {

    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kStopped:
    case RtpTransceiverDirection::kInactive:
      return video_sendrecv_codecs_;
    case RtpTransceiverDirection::kSendOnly:
      return video_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return video_recv_codecs_;
  }
  RTC_CHECK_NOTREACHED();
}

const VideoCodecs& MediaSessionDescriptionFactory::GetVideoCodecsForAnswer(
    const RtpTransceiverDirection& offer,
    const RtpTransceiverDirection& answer) const {
  switch (answer) {

    case RtpTransceiverDirection::kSendRecv:
    case RtpTransceiverDirection::kStopped:
    case RtpTransceiverDirection::kInactive:
      return GetVideoCodecsForOffer(
          webrtc::RtpTransceiverDirectionReversed(offer));
    case RtpTransceiverDirection::kSendOnly:
      return video_send_codecs_;
    case RtpTransceiverDirection::kRecvOnly:
      return video_recv_codecs_;
  }
  RTC_CHECK_NOTREACHED();
}

void MergeCodecsFromDescription(
    const std::vector<const ContentInfo*>& current_active_contents,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs,
    UsedPayloadTypes* used_pltypes) {
  for (const ContentInfo* content : current_active_contents) {
    if (IsMediaContentOfType(content, MEDIA_TYPE_AUDIO)) {
      MergeCodecs(content->media_description()->codecs(), audio_codecs,
                  used_pltypes);
    } else if (IsMediaContentOfType(content, MEDIA_TYPE_VIDEO)) {
      MergeCodecs(content->media_description()->codecs(), video_codecs,
                  used_pltypes);
    }
  }
}

void MediaSessionDescriptionFactory::GetCodecsForOffer(
    const std::vector<const ContentInfo*>& current_active_contents,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs) const {

  UsedPayloadTypes used_pltypes;
  MergeCodecsFromDescription(current_active_contents, audio_codecs,
                             video_codecs, &used_pltypes);

  MergeCodecs(all_audio_codecs_, audio_codecs, &used_pltypes);
  MergeCodecs(all_video_codecs_, video_codecs, &used_pltypes);
}

void MediaSessionDescriptionFactory::GetCodecsForAnswer(
    const std::vector<const ContentInfo*>& current_active_contents,
    const SessionDescription& remote_offer,
    AudioCodecs* audio_codecs,
    VideoCodecs* video_codecs) const {

  UsedPayloadTypes used_pltypes;
  MergeCodecsFromDescription(current_active_contents, audio_codecs,
                             video_codecs, &used_pltypes);

  AudioCodecs filtered_offered_audio_codecs;
  VideoCodecs filtered_offered_video_codecs;
  for (const ContentInfo& content : remote_offer.contents()) {
    if (IsMediaContentOfType(&content, MEDIA_TYPE_AUDIO)) {
      std::vector<Codec> offered_codecs = content.media_description()->codecs();
      for (const Codec& offered_audio_codec : offered_codecs) {
        if (!FindMatchingCodec(offered_codecs, filtered_offered_audio_codecs,
                               offered_audio_codec) &&
            FindMatchingCodec(offered_codecs, all_audio_codecs_,
                              offered_audio_codec)) {
          filtered_offered_audio_codecs.push_back(offered_audio_codec);
        }
      }
    } else if (IsMediaContentOfType(&content, MEDIA_TYPE_VIDEO)) {
      std::vector<Codec> offered_codecs = content.media_description()->codecs();
      for (const Codec& offered_video_codec : offered_codecs) {
        if (!FindMatchingCodec(offered_codecs, filtered_offered_video_codecs,
                               offered_video_codec) &&
            FindMatchingCodec(offered_codecs, all_video_codecs_,
                              offered_video_codec)) {
          filtered_offered_video_codecs.push_back(offered_video_codec);
        }
      }
    }
  }

  MergeCodecs(filtered_offered_audio_codecs, audio_codecs, &used_pltypes);
  MergeCodecs(filtered_offered_video_codecs, video_codecs, &used_pltypes);
}

MediaSessionDescriptionFactory::AudioVideoRtpHeaderExtensions
MediaSessionDescriptionFactory::GetOfferedRtpHeaderExtensionsWithIds(
    const std::vector<const ContentInfo*>& current_active_contents,
    bool extmap_allow_mixed,
    const std::vector<MediaDescriptionOptions>& media_description_options)
    const {

  UsedRtpHeaderExtensionIds used_ids(
      extmap_allow_mixed ? UsedRtpHeaderExtensionIds::IdDomain::kTwoByteAllowed
                         : UsedRtpHeaderExtensionIds::IdDomain::kOneByteOnly);
  RtpHeaderExtensions all_regular_extensions;
  RtpHeaderExtensions all_encrypted_extensions;

  AudioVideoRtpHeaderExtensions offered_extensions;

  for (const ContentInfo* content : current_active_contents) {
    if (IsMediaContentOfType(content, MEDIA_TYPE_AUDIO)) {
      MergeRtpHdrExts(content->media_description()->rtp_header_extensions(),
                      &offered_extensions.audio, &all_regular_extensions,
                      &all_encrypted_extensions, &used_ids);
    } else if (IsMediaContentOfType(content, MEDIA_TYPE_VIDEO)) {
      MergeRtpHdrExts(content->media_description()->rtp_header_extensions(),
                      &offered_extensions.video, &all_regular_extensions,
                      &all_encrypted_extensions, &used_ids);
    }
  }

  for (const auto& entry : media_description_options) {
    RtpHeaderExtensions filtered_extensions =
        filtered_rtp_header_extensions(UnstoppedOrPresentRtpHeaderExtensions(
            entry.header_extensions, all_regular_extensions,
            all_encrypted_extensions));
    if (entry.type == MEDIA_TYPE_AUDIO)
      MergeRtpHdrExts(filtered_extensions, &offered_extensions.audio,
                      &all_regular_extensions, &all_encrypted_extensions,
                      &used_ids);
    else if (entry.type == MEDIA_TYPE_VIDEO)
      MergeRtpHdrExts(filtered_extensions, &offered_extensions.video,
                      &all_regular_extensions, &all_encrypted_extensions,
                      &used_ids);
  }

  if (enable_encrypted_rtp_header_extensions_ &&
      current_active_contents.empty()) {
    AddEncryptedVersionsOfHdrExts(&offered_extensions.audio,
                                  &all_encrypted_extensions, &used_ids);
    AddEncryptedVersionsOfHdrExts(&offered_extensions.video,
                                  &all_encrypted_extensions, &used_ids);
  }
  return offered_extensions;
}

RTCError MediaSessionDescriptionFactory::AddTransportOffer(
    const std::string& content_name,
    const TransportOptions& transport_options,
    const SessionDescription* current_desc,
    SessionDescription* offer_desc,
    IceCredentialsIterator* ice_credentials) const {
  const TransportDescription* current_tdesc =
      GetTransportDescription(content_name, current_desc);
  std::unique_ptr<TransportDescription> new_tdesc(
      transport_desc_factory_->CreateOffer(transport_options, current_tdesc,
                                           ice_credentials));
  if (!new_tdesc) {
    RTC_LOG(LS_ERROR) << "Failed to AddTransportOffer, content name="
                      << content_name;
  }
  offer_desc->AddTransportInfo(TransportInfo(content_name, *new_tdesc));
  return RTCError::OK();
}

std::unique_ptr<TransportDescription>
MediaSessionDescriptionFactory::CreateTransportAnswer(
    const std::string& content_name,
    const SessionDescription* offer_desc,
    const TransportOptions& transport_options,
    const SessionDescription* current_desc,
    bool require_transport_attributes,
    IceCredentialsIterator* ice_credentials) const {
  const TransportDescription* offer_tdesc =
      GetTransportDescription(content_name, offer_desc);
  const TransportDescription* current_tdesc =
      GetTransportDescription(content_name, current_desc);
  return transport_desc_factory_->CreateAnswer(offer_tdesc, transport_options,
                                               require_transport_attributes,
                                               current_tdesc, ice_credentials);
}

RTCError MediaSessionDescriptionFactory::AddTransportAnswer(
    const std::string& content_name,
    const TransportDescription& transport_desc,
    SessionDescription* answer_desc) const {
  answer_desc->AddTransportInfo(TransportInfo(content_name, transport_desc));
  return RTCError::OK();
}

RTCError MediaSessionDescriptionFactory::AddRtpContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const RtpHeaderExtensions& header_extensions,
    const std::vector<Codec>& codecs,
    StreamParamsVec* current_streams,
    SessionDescription* session_description,
    IceCredentialsIterator* ice_credentials) const {
  RTC_DCHECK(media_description_options.type == MEDIA_TYPE_AUDIO ||
             media_description_options.type == MEDIA_TYPE_VIDEO);

  const std::vector<Codec>& supported_codecs =
      media_description_options.type == MEDIA_TYPE_AUDIO
          ? GetAudioCodecsForOffer(media_description_options.direction)
          : GetVideoCodecsForOffer(media_description_options.direction);
  webrtc::RTCErrorOr<std::vector<Codec>> error_or_filtered_codecs =
      GetNegotiatedCodecsForOffer(media_description_options, session_options,
                                  current_content, codecs, supported_codecs);
  if (!error_or_filtered_codecs.ok()) {
    return error_or_filtered_codecs.MoveError();
  }

  std::unique_ptr<MediaContentDescription> content_description;
  if (media_description_options.type == MEDIA_TYPE_AUDIO) {
    content_description = std::make_unique<AudioContentDescription>();
  } else {
    content_description = std::make_unique<VideoContentDescription>();
  }

  auto error = CreateMediaContentOffer(
      media_description_options, session_options,
      error_or_filtered_codecs.MoveValue(), header_extensions, ssrc_generator(),
      current_streams, content_description.get(),
      transport_desc_factory_->trials());
  if (!error.ok()) {
    return error;
  }

  bool secure_transport = !(transport_desc_factory_->insecure());
  SetMediaProtocol(secure_transport, content_description.get());

  content_description->set_direction(media_description_options.direction);

  session_description->AddContent(
      media_description_options.mid, MediaProtocolType::kRtp,
      media_description_options.stopped, std::move(content_description));
  return AddTransportOffer(media_description_options.mid,
                           media_description_options.transport_options,
                           current_description, session_description,
                           ice_credentials);
}

RTCError MediaSessionDescriptionFactory::AddDataContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    StreamParamsVec* current_streams,
    SessionDescription* desc,
    IceCredentialsIterator* ice_credentials) const {
  auto data = std::make_unique<SctpDataContentDescription>();

  bool secure_transport = true;

  std::vector<std::string> crypto_suites;

  data->set_protocol(secure_transport ? kMediaProtocolUdpDtlsSctp
                                      : kMediaProtocolSctp);
  data->set_use_sctpmap(session_options.use_obsolete_sctp_sdp);
  data->set_max_message_size(kSctpSendBufferSize);

  auto error = CreateContentOffer(media_description_options, session_options,
                                  RtpHeaderExtensions(), ssrc_generator(),
                                  current_streams, data.get());
  if (!error.ok()) {
    return error;
  }

  desc->AddContent(media_description_options.mid, MediaProtocolType::kSctp,
                   media_description_options.stopped, std::move(data));
  return AddTransportOffer(media_description_options.mid,
                           media_description_options.transport_options,
                           current_description, desc, ice_credentials);
}

RTCError MediaSessionDescriptionFactory::AddUnsupportedContentForOffer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    SessionDescription* desc,
    IceCredentialsIterator* ice_credentials) const {
  RTC_CHECK(IsMediaContentOfType(current_content, MEDIA_TYPE_UNSUPPORTED));

  const UnsupportedContentDescription* current_unsupported_description =
      current_content->media_description()->as_unsupported();
  auto unsupported = std::make_unique<UnsupportedContentDescription>(
      current_unsupported_description->media_type());
  unsupported->set_protocol(current_content->media_description()->protocol());
  desc->AddContent(media_description_options.mid, MediaProtocolType::kOther,
                                true, std::move(unsupported));

  return AddTransportOffer(media_description_options.mid,
                           media_description_options.transport_options,
                           current_description, desc, ice_credentials);
}

RTCError MediaSessionDescriptionFactory::AddRtpContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    const std::vector<Codec>& codecs,
    const RtpHeaderExtensions& header_extensions,
    StreamParamsVec* current_streams,
    SessionDescription* answer,
    IceCredentialsIterator* ice_credentials) const {
  RTC_DCHECK(media_description_options.type == MEDIA_TYPE_AUDIO ||
             media_description_options.type == MEDIA_TYPE_VIDEO);
  RTC_CHECK(
      IsMediaContentOfType(offer_content, media_description_options.type));
  const RtpMediaContentDescription* offer_content_description;
  if (media_description_options.type == MEDIA_TYPE_AUDIO) {
    offer_content_description = offer_content->media_description()->as_audio();
  } else {
    offer_content_description = offer_content->media_description()->as_video();
  }

  std::unique_ptr<TransportDescription> transport = CreateTransportAnswer(
      media_description_options.mid, offer_description,
      media_description_options.transport_options, current_description,
      !offer_content->rejected && bundle_transport == nullptr, ice_credentials);
  if (!transport) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INTERNAL_ERROR,
        "Failed to create transport answer, transport is missing");
  }

  auto wants_rtd = media_description_options.direction;
  auto offer_rtd = offer_content_description->direction();
  auto answer_rtd = NegotiateRtpTransceiverDirection(offer_rtd, wants_rtd);

  const std::vector<Codec>& supported_codecs =
      media_description_options.type == MEDIA_TYPE_AUDIO
          ? GetAudioCodecsForAnswer(offer_rtd, answer_rtd)
          : GetVideoCodecsForAnswer(offer_rtd, answer_rtd);
  webrtc::RTCErrorOr<std::vector<Codec>> error_or_filtered_codecs =
      GetNegotiatedCodecsForAnswer(media_description_options, session_options,
                                   current_content, codecs, supported_codecs);
  if (!error_or_filtered_codecs.ok()) {
    return error_or_filtered_codecs.MoveError();
  }
  auto filtered_codecs = error_or_filtered_codecs.MoveValue();

  bool has_common_media_codecs =
      std::find_if(filtered_codecs.begin(), filtered_codecs.end(),
                   [](const Codec& c) {
                     return c.IsMediaCodec() && !IsComfortNoiseCodec(c);
                   }) != filtered_codecs.end();

  bool bundle_enabled = offer_description->HasGroup(GROUP_TYPE_BUNDLE) &&
                        session_options.bundle_enabled;
  std::unique_ptr<MediaContentDescription> answer_content;
  if (media_description_options.type == MEDIA_TYPE_AUDIO) {
    answer_content = std::make_unique<AudioContentDescription>();
  } else {
    answer_content = std::make_unique<VideoContentDescription>();
  }
  if (!SetCodecsInAnswer(
          offer_content_description, filtered_codecs, media_description_options,
          session_options, ssrc_generator(), current_streams,
          answer_content.get(), transport_desc_factory_->trials())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to set codecs in answer");
  }
  if (!CreateMediaContentAnswer(
          offer_content_description, media_description_options, session_options,
          filtered_rtp_header_extensions(header_extensions), ssrc_generator(),
          enable_encrypted_rtp_header_extensions_, current_streams,
          bundle_enabled, answer_content.get())) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to create answer");
  }

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : transport->secure();
  bool rejected = media_description_options.stopped ||
                  offer_content->rejected || !has_common_media_codecs ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_AUDIO,
                                            answer_content->protocol(), secure);
  if (rejected) {
    RTC_LOG(LS_INFO) << "m= section '" << media_description_options.mid
                     << "' being rejected in answer.";
  }

  auto error = AddTransportAnswer(media_description_options.mid,
                                  *(transport.get()), answer);
  if (!error.ok()) {
    return error;
  }

  answer->AddContent(media_description_options.mid, offer_content->type,
                     rejected, std::move(answer_content));
  return RTCError::OK();
}

RTCError MediaSessionDescriptionFactory::AddDataContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    StreamParamsVec* current_streams,
    SessionDescription* answer,
    IceCredentialsIterator* ice_credentials) const {
  std::unique_ptr<TransportDescription> data_transport = CreateTransportAnswer(
      media_description_options.mid, offer_description,
      media_description_options.transport_options, current_description,
      !offer_content->rejected && bundle_transport == nullptr, ice_credentials);
  if (!data_transport) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INTERNAL_ERROR,
        "Failed to create transport answer, data transport is missing");
  }

  bool bundle_enabled = offer_description->HasGroup(GROUP_TYPE_BUNDLE) &&
                        session_options.bundle_enabled;
  RTC_CHECK(IsMediaContentOfType(offer_content, MEDIA_TYPE_DATA));
  std::unique_ptr<MediaContentDescription> data_answer;
  if (offer_content->media_description()->as_sctp()) {

    data_answer = std::make_unique<SctpDataContentDescription>();
    const SctpDataContentDescription* offer_data_description =
        offer_content->media_description()->as_sctp();

    data_answer->as_sctp()->set_protocol(offer_data_description->protocol());

    if (offer_data_description->max_message_size() == 0) {
      data_answer->as_sctp()->set_max_message_size(kSctpSendBufferSize);
    } else {
      data_answer->as_sctp()->set_max_message_size(std::min(
          offer_data_description->max_message_size(), kSctpSendBufferSize));
    }
    if (!CreateMediaContentAnswer(
            offer_data_description, media_description_options, session_options,
            RtpHeaderExtensions(), ssrc_generator(),
            enable_encrypted_rtp_header_extensions_, current_streams,
            bundle_enabled, data_answer.get())) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                           "Failed to create answer");
    }

    bool offer_uses_sctpmap = offer_data_description->use_sctpmap();
    data_answer->as_sctp()->set_use_sctpmap(offer_uses_sctpmap);
  } else {
    RTC_DCHECK_NOTREACHED() << "Non-SCTP data content found";
  }

  bool secure = bundle_transport ? bundle_transport->description.secure()
                                 : data_transport->secure();

  bool rejected = media_description_options.stopped ||
                  offer_content->rejected ||
                  !IsMediaProtocolSupported(MEDIA_TYPE_DATA,
                                            data_answer->protocol(), secure);
  auto error = AddTransportAnswer(media_description_options.mid,
                                  *(data_transport.get()), answer);
  if (!error.ok()) {
    return error;
  }
  answer->AddContent(media_description_options.mid, offer_content->type,
                     rejected, std::move(data_answer));
  return RTCError::OK();
}

RTCError MediaSessionDescriptionFactory::AddUnsupportedContentForAnswer(
    const MediaDescriptionOptions& media_description_options,
    const MediaSessionOptions& session_options,
    const ContentInfo* offer_content,
    const SessionDescription* offer_description,
    const ContentInfo* current_content,
    const SessionDescription* current_description,
    const TransportInfo* bundle_transport,
    SessionDescription* answer,
    IceCredentialsIterator* ice_credentials) const {
  std::unique_ptr<TransportDescription> unsupported_transport =
      CreateTransportAnswer(
          media_description_options.mid, offer_description,
          media_description_options.transport_options, current_description,
          !offer_content->rejected && bundle_transport == nullptr,
          ice_credentials);
  if (!unsupported_transport) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INTERNAL_ERROR,
        "Failed to create transport answer, unsupported transport is missing");
  }
  RTC_CHECK(IsMediaContentOfType(offer_content, MEDIA_TYPE_UNSUPPORTED));

  const UnsupportedContentDescription* offer_unsupported_description =
      offer_content->media_description()->as_unsupported();
  std::unique_ptr<MediaContentDescription> unsupported_answer =
      std::make_unique<UnsupportedContentDescription>(
          offer_unsupported_description->media_type());
  unsupported_answer->set_protocol(offer_unsupported_description->protocol());

  auto error = AddTransportAnswer(media_description_options.mid,
                                  *(unsupported_transport.get()), answer);
  if (!error.ok()) {
    return error;
  }

  answer->AddContent(media_description_options.mid, offer_content->type,
                                  true, std::move(unsupported_answer));
  return RTCError::OK();
}

void MediaSessionDescriptionFactory::ComputeAudioCodecsIntersectionAndUnion() {
  audio_sendrecv_codecs_.clear();
  all_audio_codecs_.clear();

  for (const Codec& send : audio_send_codecs_) {
    all_audio_codecs_.push_back(send);
    if (!FindMatchingCodec(audio_send_codecs_, audio_recv_codecs_, send)) {

      RTC_DCHECK(send.GetResiliencyType() != Codec::ResiliencyType::kRtx);
    }
  }
  for (const Codec& recv : audio_recv_codecs_) {
    if (!FindMatchingCodec(audio_recv_codecs_, audio_send_codecs_, recv)) {
      all_audio_codecs_.push_back(recv);
    }
  }

  NegotiateCodecs(audio_recv_codecs_, audio_send_codecs_,
                  &audio_sendrecv_codecs_, true);
}

void MediaSessionDescriptionFactory::ComputeVideoCodecsIntersectionAndUnion() {
  video_sendrecv_codecs_.clear();

  all_video_codecs_ =
      ComputeCodecsUnion(video_recv_codecs_, video_send_codecs_);

  NegotiateCodecs(video_recv_codecs_, video_send_codecs_,
                  &video_sendrecv_codecs_, true);
}

bool IsMediaContent(const ContentInfo* content) {
  return (content && (content->type == MediaProtocolType::kRtp ||
                      content->type == MediaProtocolType::kSctp));
}

bool IsAudioContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_AUDIO);
}

bool IsVideoContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_VIDEO);
}

bool IsDataContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_DATA);
}

bool IsUnsupportedContent(const ContentInfo* content) {
  return IsMediaContentOfType(content, MEDIA_TYPE_UNSUPPORTED);
}

const ContentInfo* GetFirstMediaContent(const ContentInfos& contents,
                                        MediaType media_type) {
  for (const ContentInfo& content : contents) {
    if (IsMediaContentOfType(&content, media_type)) {
      return &content;
    }
  }
  return nullptr;
}

const ContentInfo* GetFirstAudioContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_AUDIO);
}

const ContentInfo* GetFirstVideoContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_VIDEO);
}

const ContentInfo* GetFirstDataContent(const ContentInfos& contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_DATA);
}

const ContentInfo* GetFirstMediaContent(const SessionDescription* sdesc,
                                        MediaType media_type) {
  if (sdesc == nullptr) {
    return nullptr;
  }

  return GetFirstMediaContent(sdesc->contents(), media_type);
}

const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_AUDIO);
}

const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_VIDEO);
}

const ContentInfo* GetFirstDataContent(const SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_DATA);
}

const MediaContentDescription* GetFirstMediaContentDescription(
    const SessionDescription* sdesc,
    MediaType media_type) {
  const ContentInfo* content = GetFirstMediaContent(sdesc, media_type);
  return (content ? content->media_description() : nullptr);
}

const AudioContentDescription* GetFirstAudioContentDescription(
    const SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_AUDIO);
  return desc ? desc->as_audio() : nullptr;
}

const VideoContentDescription* GetFirstVideoContentDescription(
    const SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_VIDEO);
  return desc ? desc->as_video() : nullptr;
}

const SctpDataContentDescription* GetFirstSctpDataContentDescription(
    const SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_DATA);
  return desc ? desc->as_sctp() : nullptr;
}

ContentInfo* GetFirstMediaContent(ContentInfos* contents,
                                  MediaType media_type) {
  for (ContentInfo& content : *contents) {
    if (IsMediaContentOfType(&content, media_type)) {
      return &content;
    }
  }
  return nullptr;
}

ContentInfo* GetFirstAudioContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_AUDIO);
}

ContentInfo* GetFirstVideoContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_VIDEO);
}

ContentInfo* GetFirstDataContent(ContentInfos* contents) {
  return GetFirstMediaContent(contents, MEDIA_TYPE_DATA);
}

ContentInfo* GetFirstMediaContent(SessionDescription* sdesc,
                                  MediaType media_type) {
  if (sdesc == nullptr) {
    return nullptr;
  }

  return GetFirstMediaContent(&sdesc->contents(), media_type);
}

ContentInfo* GetFirstAudioContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_AUDIO);
}

ContentInfo* GetFirstVideoContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_VIDEO);
}

ContentInfo* GetFirstDataContent(SessionDescription* sdesc) {
  return GetFirstMediaContent(sdesc, MEDIA_TYPE_DATA);
}

MediaContentDescription* GetFirstMediaContentDescription(
    SessionDescription* sdesc,
    MediaType media_type) {
  ContentInfo* content = GetFirstMediaContent(sdesc, media_type);
  return (content ? content->media_description() : nullptr);
}

AudioContentDescription* GetFirstAudioContentDescription(
    SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_AUDIO);
  return desc ? desc->as_audio() : nullptr;
}

VideoContentDescription* GetFirstVideoContentDescription(
    SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_VIDEO);
  return desc ? desc->as_video() : nullptr;
}

SctpDataContentDescription* GetFirstSctpDataContentDescription(
    SessionDescription* sdesc) {
  auto desc = GetFirstMediaContentDescription(sdesc, MEDIA_TYPE_DATA);
  return desc ? desc->as_sctp() : nullptr;
}

}
