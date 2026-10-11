/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_TYPES_H_
#define MODULES_DESKTOP_CAPTURE_DESKTOP_CAPTURE_TYPES_H_

#include <stdint.h>

namespace webrtc {

enum class CaptureType { kWindow, kScreen, kAnyScreenContent };

typedef intptr_t WindowId;

const WindowId kNullWindowId = 0;

const int64_t kInvalidDisplayId = -1;

#if defined(CHROMEOS)
typedef int64_t ScreenId;
#else
typedef intptr_t ScreenId;
#endif

const ScreenId kFullDesktopScreenId = -1;

const ScreenId kInvalidScreenId = -2;

namespace DesktopCapturerId {
constexpr uint32_t CreateFourCC(char a, char b, char c, char d) {
  return ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) |
          (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24));
}

constexpr uint32_t kUnknown = 0;
constexpr uint32_t kWgcCapturerWin = 1;
constexpr uint32_t kScreenCapturerWinMagnifier = 2;
constexpr uint32_t kWindowCapturerWinGdi = 3;
constexpr uint32_t kScreenCapturerWinGdi = CreateFourCC('G', 'D', 'I', ' ');
constexpr uint32_t kScreenCapturerWinDirectx = CreateFourCC('D', 'X', 'G', 'I');
constexpr uint32_t kX11CapturerLinux = CreateFourCC('X', '1', '1', ' ');
constexpr uint32_t kWaylandCapturerLinux = CreateFourCC('W', 'L', ' ', ' ');
}

}

#endif
