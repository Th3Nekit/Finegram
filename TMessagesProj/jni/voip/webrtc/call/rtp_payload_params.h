/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_PAYLOAD_PARAMS_H_
#define CALL_RTP_PAYLOAD_PARAMS_H_

#include <array>
#include <vector>

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/video_codecs/video_encoder.h"
#include "call/rtp_config.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/chain_diff_calculator.h"
#include "modules/video_coding/frame_dependencies_calculator.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

class RtpPayloadParams final {
 public:
  RtpPayloadParams(uint32_t ssrc,
                   const RtpPayloadState* state,
                   const FieldTrialsView& trials);
  RtpPayloadParams(const RtpPayloadParams& other);
  ~RtpPayloadParams();

  RTPVideoHeader GetRtpVideoHeader(const EncodedImage& image,
                                   const CodecSpecificInfo* codec_specific_info,
                                   int64_t shared_frame_id);

  absl::optional<FrameDependencyStructure> GenericStructure(
      const CodecSpecificInfo* codec_specific_info);

  uint32_t ssrc() const;

  RtpPayloadState state() const;

 private:
  void SetCodecSpecific(RTPVideoHeader* rtp_video_header,
                        bool first_frame_in_picture);
  RTPVideoHeader::GenericDescriptorInfo GenericDescriptorFromFrameInfo(
      const GenericFrameInfo& frame_info,
      int64_t frame_id);
  void SetGeneric(const CodecSpecificInfo* codec_specific_info,
                  int64_t frame_id,
                  bool is_keyframe,
                  RTPVideoHeader* rtp_video_header);

  void Vp8ToGeneric(const CodecSpecificInfoVP8& vp8_info,
                    int64_t shared_frame_id,
                    bool is_keyframe,
                    RTPVideoHeader* rtp_video_header);

  void Vp9ToGeneric(const CodecSpecificInfoVP9& vp9_info,
                    int64_t shared_frame_id,
                    RTPVideoHeader& rtp_video_header);

  void H264ToGeneric(const CodecSpecificInfoH264& h264_info,
                     int64_t shared_frame_id,
                     bool is_keyframe,
                     RTPVideoHeader* rtp_video_header);

  void GenericToGeneric(int64_t shared_frame_id,
                        bool is_keyframe,
                        RTPVideoHeader* rtp_video_header);

  void SetDependenciesVp8Deprecated(
      const CodecSpecificInfoVP8& vp8_info,
      int64_t shared_frame_id,
      bool is_keyframe,
      int spatial_index,
      int temporal_index,
      bool layer_sync,
      RTPVideoHeader::GenericDescriptorInfo* generic);
  void SetDependenciesVp8New(const CodecSpecificInfoVP8& vp8_info,
                             int64_t shared_frame_id,
                             bool is_keyframe,
                             bool layer_sync,
                             RTPVideoHeader::GenericDescriptorInfo* generic);

  FrameDependenciesCalculator dependencies_calculator_;
  ChainDiffCalculator chains_calculator_;

  std::array<std::array<int64_t, RtpGenericFrameDescriptor::kMaxTemporalLayers>,
             RtpGenericFrameDescriptor::kMaxSpatialLayers>
      last_shared_frame_id_;

  std::vector<std::array<int64_t, RtpGenericFrameDescriptor::kMaxSpatialLayers>>
      last_vp9_frame_id_;

  std::array<int64_t, RtpGenericFrameDescriptor::kMaxSpatialLayers>
      chain_last_frame_id_;

  static constexpr size_t kMaxCodecBuffersCount =
      CodecSpecificInfoVP8::kBuffersCount;

  std::array<int64_t, kMaxCodecBuffersCount> buffer_id_to_frame_id_;

  absl::optional<bool> new_version_used_;

  const uint32_t ssrc_;
  RtpPayloadState state_;

  const bool generic_picture_id_experiment_;
  const bool simulate_generic_structure_;
};
}
#endif
