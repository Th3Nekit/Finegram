/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
#define MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_H_

#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "modules/video_capture/raw_video_sink_interface.h"
#include "modules/video_capture/video_capture_defines.h"

namespace webrtc {

class VideoCaptureModule : public RefCountInterface {
 public:

  class DeviceInfo {
   public:
    virtual uint32_t NumberOfDevices() = 0;

    virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                  char* deviceNameUTF8,
                                  uint32_t deviceNameLength,
                                  char* deviceUniqueIdUTF8,
                                  uint32_t deviceUniqueIdUTF8Length,
                                  char* productUniqueIdUTF8 = 0,
                                  uint32_t productUniqueIdUTF8Length = 0) = 0;

    virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8) = 0;

    virtual int32_t GetCapability(const char* deviceUniqueIdUTF8,
                                  uint32_t deviceCapabilityNumber,
                                  VideoCaptureCapability& capability) = 0;

    virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                   VideoRotation& orientation) = 0;

    virtual int32_t GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting) = 0;

    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8,
        void* parentWindow,
        uint32_t positionX,
        uint32_t positionY) = 0;

    virtual ~DeviceInfo() {}
  };

  virtual void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* dataCallback) = 0;
  virtual void RegisterCaptureDataCallback(
      RawVideoSinkInterface* dataCallback) = 0;

  virtual void DeRegisterCaptureDataCallback() = 0;

  virtual int32_t StartCapture(const VideoCaptureCapability& capability) = 0;

  virtual int32_t StopCapture() = 0;

  virtual const char* CurrentDeviceName() const = 0;

  virtual bool CaptureStarted() = 0;

  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) = 0;

  virtual int32_t SetCaptureRotation(VideoRotation rotation) = 0;

  virtual bool SetApplyRotation(bool enable) = 0;

  virtual bool GetApplyRotation() = 0;

 protected:
  ~VideoCaptureModule() override {}
};

}
#endif
