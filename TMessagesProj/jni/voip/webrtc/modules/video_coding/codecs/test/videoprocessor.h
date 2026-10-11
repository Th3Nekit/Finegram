/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/videocodec_test_fixture.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/resolution.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/codecs/test/videocodec_test_stats_impl.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {
namespace test {

class VideoProcessor {
 public:
  using VideoDecoderList = std::vector<std::unique_ptr<VideoDecoder>>;
  using LayerKey = std::pair<int                  , int                   >;
  using IvfFileWriterMap = std::map<LayerKey, std::unique_ptr<IvfFileWriter>>;

  using FrameWriterList = std::vector<std::unique_ptr<FrameWriter>>;
  using FrameStatistics = VideoCodecTestStats::FrameStatistics;

  VideoProcessor(webrtc::VideoEncoder* encoder,
                 VideoDecoderList* decoders,
                 FrameReader* input_frame_reader,
                 const VideoCodecTestFixture::Config& config,
                 VideoCodecTestStatsImpl* stats,
                 IvfFileWriterMap* encoded_frame_writers,
                 FrameWriterList* decoded_frame_writers);
  ~VideoProcessor();

  VideoProcessor(const VideoProcessor&) = delete;
  VideoProcessor& operator=(const VideoProcessor&) = delete;

  void ProcessFrame();

  void SetRates(size_t bitrate_kbps, double framerate_fps);

  void Finalize();

 private:
  class VideoProcessorEncodeCompleteCallback
      : public webrtc::EncodedImageCallback {
   public:
    explicit VideoProcessorEncodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(TaskQueueBase::Current()) {
      RTC_DCHECK(video_processor_);
      RTC_DCHECK(task_queue_);
    }

    Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info) override {
      RTC_CHECK(codec_specific_info);

      if (!task_queue_->IsCurrent()) {
        VideoProcessor* video_processor = video_processor_;
        task_queue_->PostTask([video_processor, encoded_image,
                               codec_specific_info = *codec_specific_info] {
          video_processor->FrameEncoded(encoded_image, codec_specific_info);
        });
        return Result(Result::OK, 0);
      }

      video_processor_->FrameEncoded(encoded_image, *codec_specific_info);
      return Result(Result::OK, 0);
    }

   private:
    VideoProcessor* const video_processor_;
    TaskQueueBase* const task_queue_;
  };

  class VideoProcessorDecodeCompleteCallback
      : public webrtc::DecodedImageCallback {
   public:
    explicit VideoProcessorDecodeCompleteCallback(
        VideoProcessor* video_processor,
        size_t simulcast_svc_idx)
        : video_processor_(video_processor),
          simulcast_svc_idx_(simulcast_svc_idx),
          task_queue_(TaskQueueBase::Current()) {
      RTC_DCHECK(video_processor_);
      RTC_DCHECK(task_queue_);
    }

    int32_t Decoded(webrtc::VideoFrame& image) override;

    int32_t Decoded(webrtc::VideoFrame& image,
                    int64_t decode_time_ms) override {
      return Decoded(image);
    }

    void Decoded(webrtc::VideoFrame& image,
                 absl::optional<int32_t> decode_time_ms,
                 absl::optional<uint8_t> qp) override {
      Decoded(image);
    }

   private:
    VideoProcessor* const video_processor_;
    const size_t simulcast_svc_idx_;
    TaskQueueBase* const task_queue_;
  };

  void FrameEncoded(const webrtc::EncodedImage& encoded_image,
                    const webrtc::CodecSpecificInfo& codec_specific);

  void FrameDecoded(const webrtc::VideoFrame& image, size_t simulcast_svc_idx);

  void DecodeFrame(const EncodedImage& encoded_image, size_t simulcast_svc_idx);

  const webrtc::EncodedImage* BuildAndStoreSuperframe(
      const EncodedImage& encoded_image,
      VideoCodecType codec,
      size_t frame_number,
      size_t simulcast_svc_idx,
      bool inter_layer_predicted) RTC_RUN_ON(sequence_checker_);

  void CalcFrameQuality(const I420BufferInterface& decoded_frame,
                        FrameStatistics* frame_stat);

  void WriteDecodedFrame(const I420BufferInterface& decoded_frame,
                         FrameWriter& frame_writer);

  void HandleTailDrops();

  const VideoCodecTestFixture::Config config_;
  const size_t num_simulcast_or_spatial_layers_;
  const bool analyze_frame_quality_;

  VideoCodecTestStatsImpl* const stats_;

  webrtc::VideoEncoder* const encoder_;
  VideoDecoderList* const decoders_;
  const std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;

  std::map<size_t, RateProfile> target_rates_ RTC_GUARDED_BY(sequence_checker_);

  VideoProcessorEncodeCompleteCallback encode_callback_;

  std::vector<std::unique_ptr<VideoProcessorDecodeCompleteCallback>>
      decode_callback_;

  FrameReader* const input_frame_reader_;

  std::map<size_t, VideoFrame> input_frames_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<EncodedImage> merged_encoded_frames_
      RTC_GUARDED_BY(sequence_checker_);

  IvfFileWriterMap* const encoded_frame_writers_;
  FrameWriterList* const decoded_frame_writers_;

  size_t last_inputed_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  size_t last_inputed_timestamp_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<bool> first_encoded_frame_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<size_t> last_encoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<bool> first_decoded_frame_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<size_t> last_decoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);

  std::vector<rtc::scoped_refptr<I420Buffer>> last_decoded_frame_buffer_
      RTC_GUARDED_BY(sequence_checker_);

  int64_t post_encode_time_ns_ RTC_GUARDED_BY(sequence_checker_);

  bool is_finalized_ RTC_GUARDED_BY(sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}
}

#endif
