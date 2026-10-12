/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_config.h"
#include "modules/video_capture/video_capture_defines.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class VideoCaptureOptions;

namespace videocapturemodule {

class RTC_EXPORT VideoCaptureImpl : public VideoCaptureModule {
 public:

  static rtc::scoped_refptr<VideoCaptureModule> Create(
      const char* deviceUniqueIdUTF8);
  static rtc::scoped_refptr<VideoCaptureModule> Create(
      VideoCaptureOptions* options,
      const char* deviceUniqueIdUTF8);

  static DeviceInfo* CreateDeviceInfo();
  static DeviceInfo* CreateDeviceInfo(VideoCaptureOptions* options);

  static int32_t RotationFromDegrees(int degrees, VideoRotation* rotation);
  static int32_t RotationInDegrees(VideoRotation rotation, int* degrees);

  void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* dataCallback) override;
  virtual void RegisterCaptureDataCallback(
      RawVideoSinkInterface* dataCallback) override;
  void DeRegisterCaptureDataCallback() override;

  int32_t SetCaptureRotation(VideoRotation rotation) override;
  bool SetApplyRotation(bool enable) override;
  bool GetApplyRotation() override;

  const char* CurrentDeviceName() const override;

  int32_t IncomingFrame(uint8_t* videoFrame,
                        size_t videoFrameLength,
                        const VideoCaptureCapability& frameInfo,
                        int64_t captureTime = 0);

  int32_t StartCapture(const VideoCaptureCapability& capability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability&             ) override;

 protected:
  VideoCaptureImpl();
  ~VideoCaptureImpl() override;

  SequenceChecker api_checker_;

  rtc::RaceChecker capture_checker_;

  char* _deviceUniqueId RTC_GUARDED_BY(api_checker_);
  Mutex api_lock_;

  VideoCaptureCapability _requestedCapability RTC_GUARDED_BY(api_checker_);

 private:
  void UpdateFrameCount();
  uint32_t CalculateFrameRate(int64_t now_ns);
  int32_t DeliverCapturedFrame(VideoFrame& captureFrame)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(api_lock_);
  void DeliverRawFrame(uint8_t* videoFrame,
                       size_t videoFrameLength,
                       const VideoCaptureCapability& frameInfo,
                       int64_t captureTime)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(api_lock_);

  int64_t _lastProcessTimeNanos RTC_GUARDED_BY(capture_checker_);

  int64_t _lastFrameRateCallbackTimeNanos RTC_GUARDED_BY(capture_checker_);

  rtc::VideoSinkInterface<VideoFrame>* _dataCallBack RTC_GUARDED_BY(api_lock_);
  RawVideoSinkInterface* _rawDataCallBack RTC_GUARDED_BY(api_lock_);

  int64_t _lastProcessFrameTimeNanos RTC_GUARDED_BY(capture_checker_);

  int64_t _incomingFrameTimesNanos[kFrameRateCountHistorySize] RTC_GUARDED_BY(
      capture_checker_);

  VideoRotation _rotateFrame RTC_GUARDED_BY(api_lock_);

  bool apply_rotation_ RTC_GUARDED_BY(api_lock_);
};
}
}
#endif
