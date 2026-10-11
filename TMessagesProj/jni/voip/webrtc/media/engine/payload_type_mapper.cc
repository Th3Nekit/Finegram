/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/payload_type_mapper.h"

#include <utility>

#include "absl/strings/ascii.h"
#include "api/audio_codecs/audio_format.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"

namespace cricket {

webrtc::SdpAudioFormat AudioCodecToSdpAudioFormat(const AudioCodec& ac) {
  return webrtc::SdpAudioFormat(ac.name, ac.clockrate, ac.channels, ac.params);
}

PayloadTypeMapper::PayloadTypeMapper()

    : next_unused_payload_type_(96),
      max_payload_type_(127),
      mappings_(
          {
           {{kPcmuCodecName, 8000, 1}, 0},
           {{"GSM", 8000, 1}, 3},
           {{"G723", 8000, 1}, 4},
           {{"DVI4", 8000, 1}, 5},
           {{"DVI4", 16000, 1}, 6},
           {{"LPC", 8000, 1}, 7},
           {{kPcmaCodecName, 8000, 1}, 8},
           {{kG722CodecName, 8000, 1}, 9},
           {{kL16CodecName, 44100, 2}, 10},
           {{kL16CodecName, 44100, 1}, 11},
           {{"QCELP", 8000, 1}, 12},
           {{kCnCodecName, 8000, 1}, 13},

           {{"MPA", 90000, 0}, 14},
           {{"MPA", 90000, 1}, 14},
           {{"G728", 8000, 1}, 15},
           {{"DVI4", 11025, 1}, 16},
           {{"DVI4", 22050, 1}, 17},
           {{"G729", 8000, 1}, 18},

           {{kIlbcCodecName, 8000, 1}, 102},
           {{kCnCodecName, 16000, 1}, 105},
           {{kCnCodecName, 32000, 1}, 106},
           {{kOpusCodecName,
             48000,
             2,
             {{kCodecParamMinPTime, "10"},
              {kCodecParamUseInbandFec, kParamValueTrue}}},
            111},

           {{kRedCodecName,
             48000,
             2,
             {{kCodecParamNotInNameValueFormat, "111/111"}}},
            63},

           {{kDtmfCodecName, 48000, 1}, 110},
           {{kDtmfCodecName, 32000, 1}, 112},
           {{kDtmfCodecName, 16000, 1}, 113},
           {{kDtmfCodecName, 8000, 1}, 126}}) {

  for (const auto& mapping : mappings_) {
    used_payload_types_.insert(mapping.second);
  }
}

PayloadTypeMapper::~PayloadTypeMapper() = default;

absl::optional<int> PayloadTypeMapper::GetMappingFor(
    const webrtc::SdpAudioFormat& format) {
  auto iter = mappings_.find(format);
  if (iter != mappings_.end())
    return iter->second;

  for (; next_unused_payload_type_ <= max_payload_type_;
       ++next_unused_payload_type_) {
    int payload_type = next_unused_payload_type_;
    if (used_payload_types_.find(payload_type) == used_payload_types_.end()) {
      used_payload_types_.insert(payload_type);
      mappings_[format] = payload_type;
      ++next_unused_payload_type_;
      return payload_type;
    }
  }

  return absl::nullopt;
}

absl::optional<int> PayloadTypeMapper::FindMappingFor(
    const webrtc::SdpAudioFormat& format) const {
  auto iter = mappings_.find(format);
  if (iter != mappings_.end())
    return iter->second;

  return absl::nullopt;
}

absl::optional<AudioCodec> PayloadTypeMapper::ToAudioCodec(
    const webrtc::SdpAudioFormat& format) {

  auto opt_payload_type = GetMappingFor(format);
  if (opt_payload_type) {
    AudioCodec codec =
        cricket::CreateAudioCodec(*opt_payload_type, format.name,
                                  format.clockrate_hz, format.num_channels);
    codec.params = format.parameters;
    return std::move(codec);
  }

  return absl::nullopt;
}

bool PayloadTypeMapper::SdpAudioFormatOrdering::operator()(
    const webrtc::SdpAudioFormat& a,
    const webrtc::SdpAudioFormat& b) const {
  if (a.clockrate_hz == b.clockrate_hz) {
    if (a.num_channels == b.num_channels) {
      int name_cmp =
          absl::AsciiStrToLower(a.name).compare(absl::AsciiStrToLower(b.name));
      if (name_cmp == 0)
        return a.parameters < b.parameters;
      return name_cmp < 0;
    }
    return a.num_channels < b.num_channels;
  }
  return a.clockrate_hz < b.clockrate_hz;
}

}
