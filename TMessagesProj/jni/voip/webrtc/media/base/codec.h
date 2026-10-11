/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_CODEC_H_
#define MEDIA_BASE_CODEC_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/audio_codecs/audio_format.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {

class FeedbackParam {
 public:
  FeedbackParam() = default;
  FeedbackParam(absl::string_view id, const std::string& param)
      : id_(id), param_(param) {}
  explicit FeedbackParam(absl::string_view id)
      : id_(id), param_(kParamValueEmpty) {}

  bool operator==(const FeedbackParam& other) const;
  bool operator!=(const FeedbackParam& c) const { return !(*this == c); }

  const std::string& id() const { return id_; }
  const std::string& param() const { return param_; }

 private:
  std::string id_;
  std::string param_;
};

class FeedbackParams {
 public:
  FeedbackParams();
  ~FeedbackParams();
  bool operator==(const FeedbackParams& other) const;
  bool operator!=(const FeedbackParams& c) const { return !(*this == c); }

  bool Has(const FeedbackParam& param) const;
  void Add(const FeedbackParam& param);

  void Intersect(const FeedbackParams& from);

  const std::vector<FeedbackParam>& params() const { return params_; }

 private:
  bool HasDuplicateEntries() const;

  std::vector<FeedbackParam> params_;
};

struct RTC_EXPORT Codec {
  enum class Type {
    kAudio,
    kVideo,
  };

  enum class ResiliencyType {
    kNone,
    kRed,
    kUlpfec,
    kFlexfec,
    kRtx,
  };

  Type type;
  int id;
  std::string name;
  int clockrate;

  int bitrate;
  size_t channels;

  absl::optional<std::string> packetization;
  absl::InlinedVector<webrtc::ScalabilityMode, webrtc::kScalabilityModeCount>
      scalability_modes;

  absl::optional<std::string> tx_mode;

  webrtc::CodecParameterMap params;
  FeedbackParams feedback_params;

  Codec(const Codec& c);
  Codec(Codec&& c);

  virtual ~Codec();

  bool Matches(const Codec& codec) const;
  bool MatchesRtpCodec(const webrtc::RtpCodec& capability) const;

  bool GetParam(const std::string& name, std::string* out) const;
  bool GetParam(const std::string& name, int* out) const;

  void SetParam(const std::string& name, const std::string& value);
  void SetParam(const std::string& name, int value);

  bool RemoveParam(const std::string& name);

  bool HasFeedbackParam(const FeedbackParam& param) const;
  void AddFeedbackParam(const FeedbackParam& param);

  void IntersectFeedbackParams(const Codec& other);

  virtual webrtc::RtpCodecParameters ToCodecParameters() const;

  bool IsMediaCodec() const;

  bool IsResiliencyCodec() const;
  ResiliencyType GetResiliencyType() const;

  bool ValidateCodecFormat() const;

  std::string ToString() const;

  Codec& operator=(const Codec& c);
  Codec& operator=(Codec&& c);

  bool operator==(const Codec& c) const;

  bool operator!=(const Codec& c) const { return !(*this == c); }

 protected:

  explicit Codec(Type type);

  Codec(Type type, int id, const std::string& name, int clockrate);
  Codec(Type type,
        int id,
        const std::string& name,
        int clockrate,
        size_t channels);

  explicit Codec(const webrtc::SdpAudioFormat& c);
  explicit Codec(const webrtc::SdpVideoFormat& c);

  friend Codec CreateAudioCodec(int id,
                                const std::string& name,
                                int clockrate,
                                size_t channels);
  friend Codec CreateAudioCodec(const webrtc::SdpAudioFormat& c);
  friend Codec CreateAudioRtxCodec(int rtx_payload_type,
                                   int associated_payload_type);
  friend Codec CreateVideoCodec(int id, const std::string& name);
  friend Codec CreateVideoCodec(const webrtc::SdpVideoFormat& c);
  friend Codec CreateVideoRtxCodec(int rtx_payload_type,
                                   int associated_payload_type);
};

using VideoCodec = Codec;
using AudioCodec = Codec;

using VideoCodecs = std::vector<Codec>;
using AudioCodecs = std::vector<Codec>;

Codec CreateAudioCodec(int id,
                       const std::string& name,
                       int clockrate,
                       size_t channels);
Codec CreateAudioCodec(const webrtc::SdpAudioFormat& c);
Codec CreateAudioRtxCodec(int rtx_payload_type, int associated_payload_type);
Codec CreateVideoCodec(const std::string& name);
Codec CreateVideoCodec(int id, const std::string& name);
Codec CreateVideoCodec(const webrtc::SdpVideoFormat& c);
Codec CreateVideoRtxCodec(int rtx_payload_type, int associated_payload_type);

const Codec* FindCodecById(const std::vector<Codec>& codecs, int payload_type);

bool HasLntf(const Codec& codec);
bool HasNack(const Codec& codec);
bool HasRemb(const Codec& codec);
bool HasRrtr(const Codec& codec);
bool HasTransportCc(const Codec& codec);

const Codec* FindMatchingVideoCodec(const std::vector<Codec>& supported_codecs,
                                    const Codec& codec);

std::vector<const Codec*> FindAllMatchingCodecs(
    const std::vector<Codec>& supported_codecs,
    const Codec& codec);

RTC_EXPORT void AddH264ConstrainedBaselineProfileToSupportedFormats(
    std::vector<webrtc::SdpVideoFormat>* supported_formats);

}

#endif
