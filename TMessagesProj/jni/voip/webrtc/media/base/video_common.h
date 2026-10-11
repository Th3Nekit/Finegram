/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Common definition for video, including fourcc and VideoFormat.

#ifndef MEDIA_BASE_VIDEO_COMMON_H_
#define MEDIA_BASE_VIDEO_COMMON_H_

#include <stdint.h>

#include <string>

#include "rtc_base/system/rtc_export.h"
#include "rtc_base/time_utils.h"

namespace cricket {

#define CRICKET_FOURCC(a, b, c, d)                                \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

enum FourCC {

  FOURCC_I420 = CRICKET_FOURCC('I', '4', '2', '0'),
  FOURCC_I422 = CRICKET_FOURCC('I', '4', '2', '2'),
  FOURCC_I444 = CRICKET_FOURCC('I', '4', '4', '4'),
  FOURCC_I411 = CRICKET_FOURCC('I', '4', '1', '1'),
  FOURCC_I400 = CRICKET_FOURCC('I', '4', '0', '0'),
  FOURCC_NV21 = CRICKET_FOURCC('N', 'V', '2', '1'),
  FOURCC_NV12 = CRICKET_FOURCC('N', 'V', '1', '2'),
  FOURCC_YUY2 = CRICKET_FOURCC('Y', 'U', 'Y', '2'),
  FOURCC_UYVY = CRICKET_FOURCC('U', 'Y', 'V', 'Y'),

  FOURCC_M420 = CRICKET_FOURCC('M', '4', '2', '0'),

  FOURCC_ARGB = CRICKET_FOURCC('A', 'R', 'G', 'B'),
  FOURCC_BGRA = CRICKET_FOURCC('B', 'G', 'R', 'A'),
  FOURCC_ABGR = CRICKET_FOURCC('A', 'B', 'G', 'R'),
  FOURCC_24BG = CRICKET_FOURCC('2', '4', 'B', 'G'),
  FOURCC_RAW = CRICKET_FOURCC('r', 'a', 'w', ' '),
  FOURCC_RGBA = CRICKET_FOURCC('R', 'G', 'B', 'A'),
  FOURCC_RGBP = CRICKET_FOURCC('R', 'G', 'B', 'P'),
  FOURCC_RGBO = CRICKET_FOURCC('R', 'G', 'B', 'O'),
  FOURCC_R444 = CRICKET_FOURCC('R', '4', '4', '4'),

  FOURCC_RGGB = CRICKET_FOURCC('R', 'G', 'G', 'B'),
  FOURCC_BGGR = CRICKET_FOURCC('B', 'G', 'G', 'R'),
  FOURCC_GRBG = CRICKET_FOURCC('G', 'R', 'B', 'G'),
  FOURCC_GBRG = CRICKET_FOURCC('G', 'B', 'R', 'G'),

  FOURCC_MJPG = CRICKET_FOURCC('M', 'J', 'P', 'G'),

  FOURCC_YV12 = CRICKET_FOURCC('Y', 'V', '1', '2'),
  FOURCC_YV16 = CRICKET_FOURCC('Y', 'V', '1', '6'),
  FOURCC_YV24 = CRICKET_FOURCC('Y', 'V', '2', '4'),
  FOURCC_YU12 = CRICKET_FOURCC('Y', 'U', '1', '2'),
  FOURCC_J420 = CRICKET_FOURCC('J', '4', '2', '0'),
  FOURCC_J400 = CRICKET_FOURCC('J', '4', '0', '0'),

  FOURCC_IYUV = CRICKET_FOURCC('I', 'Y', 'U', 'V'),
  FOURCC_YU16 = CRICKET_FOURCC('Y', 'U', '1', '6'),
  FOURCC_YU24 = CRICKET_FOURCC('Y', 'U', '2', '4'),
  FOURCC_YUYV = CRICKET_FOURCC('Y', 'U', 'Y', 'V'),
  FOURCC_YUVS = CRICKET_FOURCC('y', 'u', 'v', 's'),
  FOURCC_HDYC = CRICKET_FOURCC('H', 'D', 'Y', 'C'),
  FOURCC_2VUY = CRICKET_FOURCC('2', 'v', 'u', 'y'),
  FOURCC_JPEG = CRICKET_FOURCC('J', 'P', 'E', 'G'),
  FOURCC_DMB1 = CRICKET_FOURCC('d', 'm', 'b', '1'),
  FOURCC_BA81 = CRICKET_FOURCC('B', 'A', '8', '1'),
  FOURCC_RGB3 = CRICKET_FOURCC('R', 'G', 'B', '3'),
  FOURCC_BGR3 = CRICKET_FOURCC('B', 'G', 'R', '3'),
  FOURCC_CM32 = CRICKET_FOURCC(0, 0, 0, 32),
  FOURCC_CM24 = CRICKET_FOURCC(0, 0, 0, 24),

  FOURCC_H264 = CRICKET_FOURCC('H', '2', '6', '4'),
};

#undef CRICKET_FOURCC

static const uint32_t FOURCC_ANY = 0xFFFFFFFF;

uint32_t CanonicalFourCC(uint32_t fourcc);

inline std::string GetFourccName(uint32_t fourcc) {
  std::string name;
  name.push_back(static_cast<char>(fourcc & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 8) & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 16) & 0xFF));
  name.push_back(static_cast<char>((fourcc >> 24) & 0xFF));
  return name;
}

struct VideoFormatPod {
  int width;
  int height;
  int64_t interval;
  uint32_t fourcc;
};

struct RTC_EXPORT VideoFormat : VideoFormatPod {
  static const int64_t kMinimumInterval =
      rtc::kNumNanosecsPerSec / 10000;

  VideoFormat() { Construct(0, 0, 0, 0); }

  VideoFormat(int w, int h, int64_t interval_ns, uint32_t cc) {
    Construct(w, h, interval_ns, cc);
  }

  explicit VideoFormat(const VideoFormatPod& format) {
    Construct(format.width, format.height, format.interval, format.fourcc);
  }

  void Construct(int w, int h, int64_t interval_ns, uint32_t cc) {
    width = w;
    height = h;
    interval = interval_ns;
    fourcc = cc;
  }

  static int64_t FpsToInterval(int fps) {
    return fps ? rtc::kNumNanosecsPerSec / fps : kMinimumInterval;
  }

  static int IntervalToFps(int64_t interval) {
    if (!interval) {
      return 0;
    }
    return static_cast<int>(rtc::kNumNanosecsPerSec / interval);
  }

  static float IntervalToFpsFloat(int64_t interval) {
    if (!interval) {
      return 0.f;
    }
    return static_cast<float>(rtc::kNumNanosecsPerSec) /
           static_cast<float>(interval);
  }

  bool operator==(const VideoFormat& format) const {
    return width == format.width && height == format.height &&
           interval == format.interval && fourcc == format.fourcc;
  }

  bool operator!=(const VideoFormat& format) const {
    return !(*this == format);
  }

  bool operator<(const VideoFormat& format) const {
    return (fourcc < format.fourcc) ||
           (fourcc == format.fourcc && width < format.width) ||
           (fourcc == format.fourcc && width == format.width &&
            height < format.height) ||
           (fourcc == format.fourcc && width == format.width &&
            height == format.height && interval > format.interval);
  }

  int framerate() const { return IntervalToFps(interval); }

  bool IsSize0x0() const { return 0 == width && 0 == height; }

  bool IsPixelRateLess(const VideoFormat& format) const {
    return width * height * framerate() <
           format.width * format.height * format.framerate();
  }

  std::string ToString() const;
};

int GreatestCommonDivisor(int a, int b);

int LeastCommonMultiple(int a, int b);

}

#endif
