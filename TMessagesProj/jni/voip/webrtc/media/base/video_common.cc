/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/video_common.h"

#include "api/array_view.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"

namespace cricket {

struct FourCCAliasEntry {
  uint32_t alias;
  uint32_t canonical;
};

static const FourCCAliasEntry kFourCCAliases[] = {
    {FOURCC_IYUV, FOURCC_I420},
    {FOURCC_YU16, FOURCC_I422},
    {FOURCC_YU24, FOURCC_I444},
    {FOURCC_YUYV, FOURCC_YUY2},
    {FOURCC_YUVS, FOURCC_YUY2},
    {FOURCC_HDYC, FOURCC_UYVY},
    {FOURCC_2VUY, FOURCC_UYVY},
    {FOURCC_JPEG, FOURCC_MJPG},
    {FOURCC_DMB1, FOURCC_MJPG},
    {FOURCC_BA81, FOURCC_BGGR},
    {FOURCC_RGB3, FOURCC_RAW},
    {FOURCC_BGR3, FOURCC_24BG},
    {FOURCC_CM32, FOURCC_BGRA},
    {FOURCC_CM24, FOURCC_RAW},
};

uint32_t CanonicalFourCC(uint32_t fourcc) {
  for (uint32_t i = 0; i < arraysize(kFourCCAliases); ++i) {
    if (kFourCCAliases[i].alias == fourcc) {
      return kFourCCAliases[i].canonical;
    }
  }

  return fourcc;
}

#ifndef _MSC_EXTENSIONS
const int64_t VideoFormat::kMinimumInterval;
#endif

std::string VideoFormat::ToString() const {
  std::string fourcc_name = GetFourccName(fourcc) + " ";
  for (std::string::const_iterator i = fourcc_name.begin();
       i < fourcc_name.end(); ++i) {

    if (*i < 32 || *i >= 127) {
      fourcc_name = "";
      break;
    }
  }

  char buf[256];
  rtc::SimpleStringBuilder sb(buf);
  sb << fourcc_name << width << "x" << height << "x"
     << IntervalToFpsFloat(interval);
  return sb.str();
}

int GreatestCommonDivisor(int a, int b) {
  RTC_DCHECK_GE(a, 0);
  RTC_DCHECK_GT(b, 0);
  int c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }
  return b;
}

int LeastCommonMultiple(int a, int b) {
  RTC_DCHECK_GT(a, 0);
  RTC_DCHECK_GT(b, 0);
  return a * (b / GreatestCommonDivisor(a, b));
}

}
