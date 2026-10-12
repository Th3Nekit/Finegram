/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PCLF_MEDIA_CONFIGURATION_H_
#define API_TEST_PCLF_MEDIA_CONFIGURATION_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_options.h"
#include "api/fec_controller.h"
#include "api/function_view.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/peer_network_dependencies.h"
#include "api/test/simulated_network.h"
#include "api/test/stats_observer_interface.h"
#include "api/test/track_id_stream_info_map.h"
#include "api/test/video/video_frame_writer.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

constexpr size_t kDefaultSlidesWidth = 1850;
constexpr size_t kDefaultSlidesHeight = 1110;

enum class CapturingDeviceIndex : size_t {};

struct ScrollingParams {

  TimeDelta duration;

  size_t source_width = kDefaultSlidesWidth;

  size_t source_height = kDefaultSlidesHeight;
};

struct ScreenShareConfig {
  explicit ScreenShareConfig(TimeDelta slide_change_interval);

  TimeDelta slide_change_interval;

  bool generate_slides = false;

  absl::optional<ScrollingParams> scrolling_params;

  std::vector<std::string> slides_yuv_file_names;
};

struct VideoSimulcastConfig {
  explicit VideoSimulcastConfig(int simulcast_streams_count);

  int simulcast_streams_count;
};

struct EmulatedSFUConfig {
  EmulatedSFUConfig() = default;
  explicit EmulatedSFUConfig(int target_layer_index);
  EmulatedSFUConfig(absl::optional<int> target_layer_index,
                    absl::optional<int> target_temporal_index);

  absl::optional<int> target_layer_index;

  absl::optional<int> target_temporal_index;
};

class VideoResolution {
 public:

  enum class Spec {

    kNone,

    kMaxFromSender
  };

  VideoResolution(size_t width, size_t height, int32_t fps);
  explicit VideoResolution(Spec spec = Spec::kNone);

  bool operator==(const VideoResolution& other) const;
  bool operator!=(const VideoResolution& other) const;

  size_t width() const { return width_; }
  void set_width(size_t width) { width_ = width; }
  size_t height() const { return height_; }
  void set_height(size_t height) { height_ = height; }
  int32_t fps() const { return fps_; }
  void set_fps(int32_t fps) { fps_ = fps; }

  bool IsRegular() const;

  std::string ToString() const;

 private:
  size_t width_ = 0;
  size_t height_ = 0;
  int32_t fps_ = 0;
  Spec spec_ = Spec::kNone;
};

class VideoDumpOptions {
 public:
  static constexpr int kDefaultSamplingModulo = 1;

  explicit VideoDumpOptions(
      absl::string_view output_directory,
      int sampling_modulo = kDefaultSamplingModulo,
      bool export_frame_ids = false,
      std::function<std::unique_ptr<test::VideoFrameWriter>(
          absl::string_view file_name_prefix,
          const VideoResolution& resolution)> video_frame_writer_factory =
          Y4mVideoFrameWriterFactory);
  VideoDumpOptions(absl::string_view output_directory, bool export_frame_ids);

  VideoDumpOptions(const VideoDumpOptions&) = default;
  VideoDumpOptions& operator=(const VideoDumpOptions&) = default;
  VideoDumpOptions(VideoDumpOptions&&) = default;
  VideoDumpOptions& operator=(VideoDumpOptions&&) = default;

  std::string output_directory() const { return output_directory_; }
  int sampling_modulo() const { return sampling_modulo_; }
  bool export_frame_ids() const { return export_frame_ids_; }

  std::unique_ptr<test::VideoFrameWriter> CreateInputDumpVideoFrameWriter(
      absl::string_view stream_label,
      const VideoResolution& resolution) const;

  std::unique_ptr<test::VideoFrameWriter> CreateOutputDumpVideoFrameWriter(
      absl::string_view stream_label,
      absl::string_view receiver,
      const VideoResolution& resolution) const;

  std::string ToString() const;

 private:
  static std::unique_ptr<test::VideoFrameWriter> Y4mVideoFrameWriterFactory(
      absl::string_view file_name_prefix,
      const VideoResolution& resolution);
  std::string GetInputDumpFileName(absl::string_view stream_label,
                                   const VideoResolution& resolution) const;

  absl::optional<std::string> GetInputFrameIdsDumpFileName(
      absl::string_view stream_label,
      const VideoResolution& resolution) const;
  std::string GetOutputDumpFileName(absl::string_view stream_label,
                                    absl::string_view receiver,
                                    const VideoResolution& resolution) const;

  absl::optional<std::string> GetOutputFrameIdsDumpFileName(
      absl::string_view stream_label,
      absl::string_view receiver,
      const VideoResolution& resolution) const;

  std::string output_directory_;
  int sampling_modulo_ = 1;
  bool export_frame_ids_ = false;
  std::function<std::unique_ptr<test::VideoFrameWriter>(
      absl::string_view file_name_prefix,
      const VideoResolution& resolution)>
      video_frame_writer_factory_;
};

struct VideoConfig {
  explicit VideoConfig(const VideoResolution& resolution);
  VideoConfig(size_t width, size_t height, int32_t fps);
  VideoConfig(absl::string_view stream_label,
              size_t width,
              size_t height,
              int32_t fps);

  size_t width;

  size_t height;
  int32_t fps;
  VideoResolution GetResolution() const {
    return VideoResolution(width, height, fps);
  }

  absl::optional<std::string> stream_label;

  absl::optional<VideoTrackInterface::ContentHint> content_hint;

  absl::optional<VideoSimulcastConfig> simulcast_config;

  absl::optional<EmulatedSFUConfig> emulated_sfu_config;

  std::vector<RtpEncodingParameters> encoding_params;

  absl::optional<int> temporal_layers_count;

  absl::optional<VideoDumpOptions> input_dump_options;

  absl::optional<VideoDumpOptions> output_dump_options;

  bool output_dump_use_fixed_framerate = false;

  bool show_on_screen = false;

  absl::optional<std::string> sync_group;

  absl::optional<DegradationPreference> degradation_preference;
};

struct AudioConfig {
  AudioConfig() = default;
  explicit AudioConfig(absl::string_view stream_label);

  absl::optional<std::string> stream_label;

  absl::optional<std::string> input_file_name;

  absl::optional<std::string> input_dump_file_name;

  absl::optional<std::string> output_dump_file_name;

  cricket::AudioOptions audio_options;

  int sampling_frequency_in_hz = 48000;

  absl::optional<std::string> sync_group;
};

struct VideoCodecConfig {
  explicit VideoCodecConfig(absl::string_view name);
  VideoCodecConfig(absl::string_view name,
                   std::map<std::string, std::string> required_params);

  std::string name;

  std::map<std::string, std::string> required_params;
};

class VideoSubscription {
 public:

  static absl::optional<VideoResolution> GetMaxResolution(
      rtc::ArrayView<const VideoConfig> video_configs);
  static absl::optional<VideoResolution> GetMaxResolution(
      rtc::ArrayView<const VideoResolution> resolutions);

  bool operator==(const VideoSubscription& other) const;
  bool operator!=(const VideoSubscription& other) const;

  VideoSubscription& SubscribeToPeer(
      absl::string_view peer_name,
      VideoResolution resolution =
          VideoResolution(VideoResolution::Spec::kMaxFromSender));

  VideoSubscription& SubscribeToAllPeers(
      VideoResolution resolution =
          VideoResolution(VideoResolution::Spec::kMaxFromSender));

  absl::optional<VideoResolution> GetResolutionForPeer(
      absl::string_view peer_name) const;

  std::vector<std::string> GetSubscribedPeers() const;

  std::string ToString() const;

 private:
  absl::optional<VideoResolution> default_resolution_ = absl::nullopt;
  std::map<std::string, VideoResolution> peers_resolution_;
};

struct EchoEmulationConfig {

  TimeDelta echo_delay = TimeDelta::Millis(50);
};

}
}

#endif
