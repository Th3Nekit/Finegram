/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_LIBVPX_VP9_ENCODER_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_LIBVPX_VP9_ENCODER_H_

#ifdef RTC_ENABLE_VP9

#include <array>
#include <memory>
#include <vector>

#include "api/fec_controller_override.h"
#include "api/field_trials_view.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp9_profile.h"
#include "common_video/include/video_frame_buffer_pool.h"
#include "modules/video_coding/codecs/interface/libvpx_interface.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/codecs/vp9/vp9_frame_buffer_pool.h"
#include "modules/video_coding/svc/scalable_video_controller.h"
#include "modules/video_coding/utility/framerate_controller_deprecated.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/experiments/encoder_info_settings.h"
#include <vpx/vp8cx.h>

namespace webrtc {

class LibvpxVp9Encoder : public VP9Encoder {
 public:
  LibvpxVp9Encoder(const cricket::VideoCodec& codec,
                   std::unique_ptr<LibvpxInterface> interface,
                   const FieldTrialsView& trials);

  ~LibvpxVp9Encoder() override;

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  int Release() override;

  int InitEncode(const VideoCodec* codec_settings,
                 const Settings& settings) override;

  int Encode(const VideoFrame& input_image,
             const std::vector<VideoFrameType>* frame_types) override;

  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

  void SetRates(const RateControlParameters& parameters) override;

  EncoderInfo GetEncoderInfo() const override;

 private:

  int NumberOfThreads(int width, int height, int number_of_cores);

  int InitAndSetControlSettings(const VideoCodec* inst);

  bool PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             absl::optional<int>* spatial_idx,
                             absl::optional<int>* temporal_idx,
                             const vpx_codec_cx_pkt& pkt);
  void FillReferenceIndices(const vpx_codec_cx_pkt& pkt,
                            size_t pic_num,
                            bool inter_layer_predicted,
                            CodecSpecificInfoVP9* vp9_info);
  void UpdateReferenceBuffers(const vpx_codec_cx_pkt& pkt, size_t pic_num);
  vpx_svc_ref_frame_config_t SetReferences(bool is_key_pic,
                                           int first_active_spatial_layer_id);

  bool ExplicitlyConfiguredSpatialLayers() const;
  bool SetSvcRates(const VideoBitrateAllocation& bitrate_allocation);

  void EnableSpatialLayer(int sid);
  void DisableSpatialLayer(int sid);
  void SetActiveSpatialLayers();

  void GetEncodedLayerFrame(const vpx_codec_cx_pkt* pkt);

  static void EncoderOutputCodedPacketCallback(vpx_codec_cx_pkt* pkt,
                                               void* user_data);

  void DeliverBufferedFrame(bool end_of_picture);

  bool DropFrame(uint8_t spatial_idx, uint32_t rtp_timestamp);

  uint32_t MaxIntraTarget(uint32_t optimal_buffer_size);

  size_t SteadyStateSize(int sid, int tid);

  void MaybeRewrapRawWithFormat(vpx_img_fmt fmt);

  rtc::scoped_refptr<VideoFrameBuffer> PrepareBufferForProfile0(
      rtc::scoped_refptr<VideoFrameBuffer> buffer);

  const std::unique_ptr<LibvpxInterface> libvpx_;
  EncodedImage encoded_image_;
  CodecSpecificInfo codec_specific_;
  EncodedImageCallback* encoded_complete_callback_;
  VideoCodec codec_;
  const VP9Profile profile_;
  bool inited_;
  int64_t timestamp_;
  uint32_t rc_max_intra_target_;
  vpx_codec_ctx_t* encoder_;
  vpx_codec_enc_cfg_t* config_;
  vpx_image_t* raw_;
  vpx_svc_extra_cfg_t svc_params_;
  const VideoFrame* input_image_;
  GofInfoVP9 gof_;

  bool force_key_frame_;
  size_t pics_since_key_;
  uint8_t num_temporal_layers_;
  uint8_t num_spatial_layers_;
  uint8_t num_active_spatial_layers_;
  uint8_t first_active_layer_;
  bool layer_deactivation_requires_key_frame_;
  bool is_svc_;
  InterLayerPredMode inter_layer_pred_;
  bool external_ref_control_;
  const bool trusted_rate_controller_;
  vpx_svc_frame_drop_t svc_drop_frame_;
  bool first_frame_in_picture_;
  VideoBitrateAllocation current_bitrate_allocation_;
  bool ss_info_needed_;
  bool force_all_active_layers_;

  std::unique_ptr<ScalableVideoController> svc_controller_;
  absl::optional<ScalabilityMode> scalability_mode_;
  std::vector<FramerateControllerDeprecated> framerate_controller_;

  bool is_flexible_mode_;
  struct RefFrameBuffer {
    bool operator==(const RefFrameBuffer& o) {
      return pic_num == o.pic_num && spatial_layer_id == o.spatial_layer_id &&
             temporal_layer_id == o.temporal_layer_id;
    }

    size_t pic_num = 0;
    int spatial_layer_id = 0;
    int temporal_layer_id = 0;
  };
  std::array<RefFrameBuffer, kNumVp9Buffers> ref_buf_;
  std::vector<ScalableVideoController::LayerFrameConfig> layer_frames_;

  const struct VariableFramerateExperiment {
    bool enabled;

    float framerate_limit;

    int steady_state_qp;

    int steady_state_undershoot_percentage;

    int frames_before_steady_state;
  } variable_framerate_experiment_;
  static VariableFramerateExperiment ParseVariableFramerateConfig(
      const FieldTrialsView& trials);
  FramerateControllerDeprecated variable_framerate_controller_;

  const struct QualityScalerExperiment {
    int low_qp;
    int high_qp;
    bool enabled;
  } quality_scaler_experiment_;
  static QualityScalerExperiment ParseQualityScalerConfig(
      const FieldTrialsView& trials);
  const bool external_ref_ctrl_;

  struct PerformanceFlags {

    bool use_per_layer_speed = false;

    struct ParameterSet {
      int base_layer_speed = -1;
      int high_layer_speed = -1;

      int deblock_mode = 0;
      bool allow_denoising = true;
    };

    flat_map<int, ParameterSet> settings_by_resolution;
  };

  const PerformanceFlags performance_flags_;

  std::vector<PerformanceFlags::ParameterSet>
      performance_flags_by_spatial_index_;
  void UpdatePerformanceFlags();
  static PerformanceFlags ParsePerformanceFlagsFromTrials(
      const FieldTrialsView& trials);
  static PerformanceFlags GetDefaultPerformanceFlags();

  int num_steady_state_frames_;

  bool config_changed_;

  const LibvpxVp9EncoderInfoSettings encoder_info_override_;

  const struct SvcFrameDropConfig {
    bool enabled;
    int layer_drop_mode;
    int max_consec_drop;
  } svc_frame_drop_config_;
  static SvcFrameDropConfig ParseSvcFrameDropConfig(
      const FieldTrialsView& trials);
};

}

#endif

#endif
