/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_ADAPTED_VIDEO_TRACK_SOURCE_H_
#define MEDIA_BASE_ADAPTED_VIDEO_TRACK_SOURCE_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {

class RTC_EXPORT AdaptedVideoTrackSource
    : public webrtc::Notifier<webrtc::VideoTrackSourceInterface> {
 public:
  AdaptedVideoTrackSource();
  ~AdaptedVideoTrackSource() override;

 protected:

  explicit AdaptedVideoTrackSource(int required_alignment);

  void OnFrame(const webrtc::VideoFrame& frame);

  void OnFrameDropped();

  bool AdaptFrame(int width,
                  int height,
                  int64_t time_us,
                  int* out_width,
                  int* out_height,
                  int* crop_width,
                  int* crop_height,
                  int* crop_x,
                  int* crop_y);

  bool apply_rotation();

  cricket::VideoAdapter* video_adapter() { return &video_adapter_; }

 private:

  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  bool GetStats(Stats* stats) override;

  void OnSinkWantsChanged(const rtc::VideoSinkWants& wants);

  bool SupportsEncodedOutput() const override { return false; }
  void GenerateKeyFrame() override {}
  void AddEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {}
  void RemoveEncodedSink(
      rtc::VideoSinkInterface<webrtc::RecordableEncodedFrame>* sink) override {}
  void ProcessConstraints(
      const webrtc::VideoTrackSourceConstraints& constraints) override;

  cricket::VideoAdapter video_adapter_;

  webrtc::Mutex stats_mutex_;
  absl::optional<Stats> stats_ RTC_GUARDED_BY(stats_mutex_);

  VideoBroadcaster broadcaster_;
};

}

#endif
