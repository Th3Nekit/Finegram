/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEO_RTP_TRACK_SOURCE_H_
#define PC_VIDEO_RTP_TRACK_SOURCE_H_

#include <vector>

#include "api/sequence_checker.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_broadcaster.h"
#include "pc/video_track_source.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class VideoRtpTrackSource : public VideoTrackSource {
 public:
  class Callback {
   public:
    virtual ~Callback() = default;

    virtual void OnGenerateKeyFrame() = 0;

    virtual void OnEncodedSinkEnabled(bool enable) = 0;
  };

  explicit VideoRtpTrackSource(Callback* callback);

  VideoRtpTrackSource(const VideoRtpTrackSource&) = delete;
  VideoRtpTrackSource& operator=(const VideoRtpTrackSource&) = delete;

  void ClearCallback();

  void BroadcastRecordableEncodedFrame(
      const RecordableEncodedFrame& frame) const;

  rtc::VideoSourceInterface<VideoFrame>* source() override;
  rtc::VideoSinkInterface<VideoFrame>* sink();

  bool SupportsEncodedOutput() const override;

  void GenerateKeyFrame() override;

  void AddEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) override;

  void RemoveEncodedSink(
      rtc::VideoSinkInterface<RecordableEncodedFrame>* sink) override;

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_{
      SequenceChecker::kDetached};

  rtc::VideoBroadcaster broadcaster_;
  mutable Mutex mu_;
  std::vector<rtc::VideoSinkInterface<RecordableEncodedFrame>*> encoded_sinks_
      RTC_GUARDED_BY(mu_);
  Callback* callback_ RTC_GUARDED_BY(worker_sequence_checker_);
};

}

#endif
