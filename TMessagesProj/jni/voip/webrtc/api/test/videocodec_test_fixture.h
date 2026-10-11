/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_VIDEOCODEC_TEST_FIXTURE_H_
#define API_TEST_VIDEOCODEC_TEST_FIXTURE_H_

#include <string>
#include <vector>

#include "api/test/videocodec_test_stats.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {
namespace test {

struct RateProfile {
  size_t target_kbps;
  double input_fps;
  size_t frame_num;
};

struct RateControlThresholds {
  double max_avg_bitrate_mismatch_percent;
  double max_time_to_reach_target_bitrate_sec;

  double max_avg_framerate_mismatch_percent;
  double max_avg_buffer_level_sec;
  double max_max_key_frame_delay_sec;
  double max_max_delta_frame_delay_sec;
  size_t max_num_spatial_resizes;
  size_t max_num_key_frames;
};

struct QualityThresholds {
  double min_avg_psnr;
  double min_min_psnr;
  double min_avg_ssim;
  double min_min_ssim;
};

struct BitstreamThresholds {
  size_t max_max_nalu_size_bytes;
};

class VideoCodecTestFixture {
 public:
  class EncodedFrameChecker {
   public:
    virtual ~EncodedFrameChecker() = default;
    virtual void CheckEncodedFrame(VideoCodecType codec,
                                   const EncodedImage& encoded_frame) const = 0;
  };

  struct Config {
    Config();
    void SetCodecSettings(std::string codec_name,
                          size_t num_simulcast_streams,
                          size_t num_spatial_layers,
                          size_t num_temporal_layers,
                          bool denoising_on,
                          bool frame_dropper_on,
                          bool spatial_resize_on,
                          size_t width,
                          size_t height);

    size_t NumberOfCores() const;
    size_t NumberOfTemporalLayers() const;
    size_t NumberOfSpatialLayers() const;
    size_t NumberOfSimulcastStreams() const;

    std::string ToString() const;
    std::string CodecName() const;

    std::string test_name;

    std::string filename;

    absl::optional<int> clip_width;
    absl::optional<int> clip_height;

    absl::optional<int> clip_fps;

    absl::optional<int> reference_width;
    absl::optional<int> reference_height;

    std::string filepath;

    size_t num_frames = 0;

    size_t max_payload_size_bytes = 1440;

    bool decode = true;

    bool use_single_core = false;

    bool measure_cpu = false;

    bool encode_in_real_time = false;

    VideoCodec codec_settings;

    std::string codec_name;

    absl::optional<SdpVideoFormat> encoder_format;
    absl::optional<SdpVideoFormat> decoder_format;

    struct H264CodecSettings {
      H264Profile profile = H264Profile::kProfileConstrainedBaseline;
      H264PacketizationMode packetization_mode =
          H264PacketizationMode::NonInterleaved;
    } h264_codec_settings;

    const EncodedFrameChecker* encoded_frame_checker = nullptr;

    bool print_frame_level_stats = false;

    std::string output_path;

    struct VisualizationParams {
      bool save_encoded_ivf = false;
      bool save_decoded_y4m = false;
    } visualization_params;

    bool analyze_quality_of_dropped_frames = false;
  };

  virtual ~VideoCodecTestFixture() = default;

  virtual void RunTest(const std::vector<RateProfile>& rate_profiles,
                       const std::vector<RateControlThresholds>* rc_thresholds,
                       const std::vector<QualityThresholds>* quality_thresholds,
                       const BitstreamThresholds* bs_thresholds) = 0;
  virtual VideoCodecTestStats& GetStats() = 0;
};

}
}

#endif
