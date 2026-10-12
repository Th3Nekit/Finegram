/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SHARED_DESKTOP_FRAME_H_
#define MODULES_DESKTOP_CAPTURE_SHARED_DESKTOP_FRAME_H_

#include <memory>

#include "api/scoped_refptr.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT SharedDesktopFrame final : public DesktopFrame {
 public:
  ~SharedDesktopFrame() override;

  SharedDesktopFrame(const SharedDesktopFrame&) = delete;
  SharedDesktopFrame& operator=(const SharedDesktopFrame&) = delete;

  static std::unique_ptr<SharedDesktopFrame> Wrap(
      std::unique_ptr<DesktopFrame> desktop_frame);

  static SharedDesktopFrame* Wrap(DesktopFrame* desktop_frame);

  DesktopFrame* GetUnderlyingFrame();

  bool ShareFrameWith(const SharedDesktopFrame& other) const;

  std::unique_ptr<SharedDesktopFrame> Share();

  bool IsShared();

 private:
  typedef rtc::FinalRefCountedObject<std::unique_ptr<DesktopFrame>> Core;

  SharedDesktopFrame(rtc::scoped_refptr<Core> core);

  const rtc::scoped_refptr<Core> core_;
};

}

#endif
