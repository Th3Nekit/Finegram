/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/desktop_capture_metrics_helper.h"

#include "modules/desktop_capture/desktop_capture_types.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

enum class SequentialDesktopCapturerId {
  kUnknown = 0,
  kWgcCapturerWin = 1,

  kWindowCapturerWinGdi = 3,
  kScreenCapturerWinGdi = 4,
  kScreenCapturerWinDirectx = 5,
  kMaxValue = kScreenCapturerWinDirectx
};
}

void RecordCapturerImpl(uint32_t capturer_id) {
  SequentialDesktopCapturerId sequential_id;
  switch (capturer_id) {
    case DesktopCapturerId::kWgcCapturerWin:
      sequential_id = SequentialDesktopCapturerId::kWgcCapturerWin;
      break;
    case DesktopCapturerId::kWindowCapturerWinGdi:
      sequential_id = SequentialDesktopCapturerId::kWindowCapturerWinGdi;
      break;
    case DesktopCapturerId::kScreenCapturerWinGdi:
      sequential_id = SequentialDesktopCapturerId::kScreenCapturerWinGdi;
      break;
    case DesktopCapturerId::kScreenCapturerWinDirectx:
      sequential_id = SequentialDesktopCapturerId::kScreenCapturerWinDirectx;
      break;
    case DesktopCapturerId::kUnknown:
    default:
      sequential_id = SequentialDesktopCapturerId::kUnknown;
  }
  RTC_HISTOGRAM_ENUMERATION(
      "WebRTC.DesktopCapture.Win.DesktopCapturerImpl",
      static_cast<int>(sequential_id),
      static_cast<int>(SequentialDesktopCapturerId::kMaxValue));
}

}
