/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_VIDEO_BROADCASTER_H_
#define MEDIA_BASE_VIDEO_BROADCASTER_H_

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_source_base.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {

class VideoBroadcaster : public VideoSourceBase,
                         public VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoBroadcaster();
  ~VideoBroadcaster() override;

  void AddOrUpdateSink(VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const VideoSinkWants& wants) override;
  void RemoveSink(VideoSinkInterface<webrtc::VideoFrame>* sink) override;

  bool frame_wanted() const;

  VideoSinkWants wants() const;

  void OnFrame(const webrtc::VideoFrame& frame) override;

  void OnDiscardedFrame() override;

  void ProcessConstraints(
      const webrtc::VideoTrackSourceConstraints& constraints);

 protected:
  void UpdateWants() RTC_EXCLUSIVE_LOCKS_REQUIRED(sinks_and_wants_lock_);
  const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& GetBlackFrameBuffer(
      int width,
      int height) RTC_EXCLUSIVE_LOCKS_REQUIRED(sinks_and_wants_lock_);

  mutable webrtc::Mutex sinks_and_wants_lock_;

  VideoSinkWants current_wants_ RTC_GUARDED_BY(sinks_and_wants_lock_);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> black_frame_buffer_;
  bool previous_frame_sent_to_all_sinks_ RTC_GUARDED_BY(sinks_and_wants_lock_) =
      true;
  absl::optional<webrtc::VideoTrackSourceConstraints> last_constraints_
      RTC_GUARDED_BY(sinks_and_wants_lock_);
};

}

#endif
