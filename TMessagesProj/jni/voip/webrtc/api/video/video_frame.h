/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_FRAME_H_
#define API_VIDEO_VIDEO_FRAME_H_

#include <stdint.h>

#include <utility>

#include "absl/types/optional.h"
#include "api/rtp_packet_infos.h"
#include "api/scoped_refptr.h"
#include "api/video/color_space.h"
#include "api/video/hdr_metadata.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT VideoFrame {
 public:

  static constexpr uint16_t kNotSetId = 0;

  struct RTC_EXPORT UpdateRect {
    int offset_x = 0;
    int offset_y = 0;
    int width = 0;
    int height = 0;

    void Union(const UpdateRect& other);

    void Intersect(const UpdateRect& other);

    void MakeEmptyUpdate();

    bool IsEmpty() const;

    bool operator==(const UpdateRect& other) const {
      return other.offset_x == offset_x && other.offset_y == offset_y &&
             other.width == width && other.height == height;
    }

    bool operator!=(const UpdateRect& other) const { return !(*this == other); }

    UpdateRect ScaleWithFrame(int frame_width,
                              int frame_height,
                              int crop_x,
                              int crop_y,
                              int crop_width,
                              int crop_height,
                              int scaled_width,
                              int scaled_height) const;
  };

  struct RTC_EXPORT ProcessingTime {
    TimeDelta Elapsed() const { return finish - start; }
    Timestamp start;
    Timestamp finish;
  };

  struct RTC_EXPORT RenderParameters {
    bool use_low_latency_rendering = false;
    absl::optional<int32_t> max_composition_delay_in_frames;

    bool operator==(const RenderParameters& other) const {
      return other.use_low_latency_rendering == use_low_latency_rendering &&
             other.max_composition_delay_in_frames ==
                 max_composition_delay_in_frames;
    }

    bool operator!=(const RenderParameters& other) const {
      return !(*this == other);
    }
  };

  class RTC_EXPORT Builder {
   public:
    Builder();
    ~Builder();

    VideoFrame build();
    Builder& set_video_frame_buffer(
        const rtc::scoped_refptr<VideoFrameBuffer>& buffer);
    Builder& set_timestamp_ms(int64_t timestamp_ms);
    Builder& set_timestamp_us(int64_t timestamp_us);
    Builder& set_capture_time_identifier(
        const absl::optional<Timestamp>& capture_time_identifier);
    Builder& set_reference_time(
        const absl::optional<Timestamp>& reference_time);
    Builder& set_timestamp_rtp(uint32_t timestamp_rtp);
    Builder& set_ntp_time_ms(int64_t ntp_time_ms);
    Builder& set_rotation(VideoRotation rotation);
    Builder& set_color_space(const absl::optional<ColorSpace>& color_space);
    Builder& set_color_space(const ColorSpace* color_space);
    Builder& set_id(uint16_t id);
    Builder& set_update_rect(const absl::optional<UpdateRect>& update_rect);
    Builder& set_packet_infos(RtpPacketInfos packet_infos);

   private:
    uint16_t id_ = kNotSetId;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
    int64_t timestamp_us_ = 0;
    absl::optional<Timestamp> capture_time_identifier_;
    absl::optional<Timestamp> reference_time_;
    uint32_t timestamp_rtp_ = 0;
    int64_t ntp_time_ms_ = 0;
    VideoRotation rotation_ = kVideoRotation_0;
    absl::optional<ColorSpace> color_space_;
    RenderParameters render_parameters_;
    absl::optional<UpdateRect> update_rect_;
    RtpPacketInfos packet_infos_;
  };

  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             webrtc::VideoRotation rotation,
             int64_t timestamp_us);
  VideoFrame(const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             uint32_t timestamp_rtp,
             int64_t render_time_ms,
             VideoRotation rotation);

  ~VideoFrame();

  VideoFrame(const VideoFrame&);
  VideoFrame(VideoFrame&&);
  VideoFrame& operator=(const VideoFrame&);
  VideoFrame& operator=(VideoFrame&&);

  int width() const;

  int height() const;

  uint32_t size() const;

  uint16_t id() const { return id_; }
  void set_id(uint16_t id) { id_ = id; }

  int64_t timestamp_us() const { return timestamp_us_; }
  void set_timestamp_us(int64_t timestamp_us) { timestamp_us_ = timestamp_us; }

  const absl::optional<Timestamp>& capture_time_identifier() const {
    return capture_time_identifier_;
  }
  void set_capture_time_identifier(
      const absl::optional<Timestamp>& capture_time_identifier) {
    capture_time_identifier_ = capture_time_identifier;
  }

  const absl::optional<Timestamp>& reference_time() const {
    return reference_time_;
  }
  void set_reference_time(const absl::optional<Timestamp>& reference_time) {
    reference_time_ = reference_time;
  }

  void set_timestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

  uint32_t timestamp() const { return timestamp_rtp_; }

  void set_ntp_time_ms(int64_t ntp_time_ms) { ntp_time_ms_ = ntp_time_ms; }

  int64_t ntp_time_ms() const { return ntp_time_ms_; }

  VideoRotation rotation() const { return rotation_; }
  void set_rotation(VideoRotation rotation) { rotation_ = rotation; }

  const absl::optional<ColorSpace>& color_space() const { return color_space_; }
  void set_color_space(const absl::optional<ColorSpace>& color_space) {
    color_space_ = color_space;
  }

  RenderParameters render_parameters() const { return render_parameters_; }
  void set_render_parameters(const RenderParameters& render_parameters) {
    render_parameters_ = render_parameters;
  }

  [[deprecated("Use render_parameters() instead.")]] absl::optional<int32_t>
  max_composition_delay_in_frames() const {
    return render_parameters_.max_composition_delay_in_frames;
  }

  int64_t render_time_ms() const;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer() const;

  void set_video_frame_buffer(
      const rtc::scoped_refptr<VideoFrameBuffer>& buffer);

  bool is_texture() const {
    return video_frame_buffer()->type() == VideoFrameBuffer::Type::kNative;
  }

  bool has_update_rect() const { return update_rect_.has_value(); }

  UpdateRect update_rect() const {
    return update_rect_.value_or(UpdateRect{0, 0, width(), height()});
  }

  void set_update_rect(const VideoFrame::UpdateRect& update_rect) {
    RTC_DCHECK_GE(update_rect.offset_x, 0);
    RTC_DCHECK_GE(update_rect.offset_y, 0);
    RTC_DCHECK_LE(update_rect.offset_x + update_rect.width, width());
    RTC_DCHECK_LE(update_rect.offset_y + update_rect.height, height());
    update_rect_ = update_rect;
  }

  void clear_update_rect() { update_rect_ = absl::nullopt; }

  const RtpPacketInfos& packet_infos() const { return packet_infos_; }
  void set_packet_infos(RtpPacketInfos value) {
    packet_infos_ = std::move(value);
  }

  const absl::optional<ProcessingTime> processing_time() const {
    return processing_time_;
  }
  void set_processing_time(const ProcessingTime& processing_time) {
    processing_time_ = processing_time;
  }

 private:
  VideoFrame(uint16_t id,
             const rtc::scoped_refptr<VideoFrameBuffer>& buffer,
             int64_t timestamp_us,
             const absl::optional<Timestamp>& capture_time_identifier,
             const absl::optional<Timestamp>& reference_time,
             uint32_t timestamp_rtp,
             int64_t ntp_time_ms,
             VideoRotation rotation,
             const absl::optional<ColorSpace>& color_space,
             const RenderParameters& render_parameters,
             const absl::optional<UpdateRect>& update_rect,
             RtpPacketInfos packet_infos);

  uint16_t id_;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer_;
  uint32_t timestamp_rtp_;
  int64_t ntp_time_ms_;
  int64_t timestamp_us_;
  absl::optional<Timestamp> capture_time_identifier_;

  absl::optional<Timestamp> reference_time_;
  VideoRotation rotation_;
  absl::optional<ColorSpace> color_space_;

  RenderParameters render_parameters_;

  absl::optional<UpdateRect> update_rect_;

  RtpPacketInfos packet_infos_;

  absl::optional<ProcessingTime> processing_time_;
};

}

#endif
