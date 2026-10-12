/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/scoped_refptr.h"
#include "modules/video_capture/windows/video_capture_ds.h"

namespace webrtc {
namespace videocapturemodule {

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo() {

  return DeviceInfoDS::Create();
}

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const char* device_id) {
  if (device_id == nullptr)
    return nullptr;

  auto capture = rtc::make_ref_counted<VideoCaptureDS>();
  if (capture->Init(device_id) != 0) {
    return nullptr;
  }

  return capture;
}

}
}
