/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_H264_PROFILE_LEVEL_ID_H_
#define API_VIDEO_CODECS_H264_PROFILE_LEVEL_ID_H_

#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class H264Profile {
  kProfileConstrainedBaseline,
  kProfileBaseline,
  kProfileMain,
  kProfileConstrainedHigh,
  kProfileHigh,
  kProfilePredictiveHigh444,
};

enum class H264Level {
  kLevel1_b = 0,
  kLevel1 = 10,
  kLevel1_1 = 11,
  kLevel1_2 = 12,
  kLevel1_3 = 13,
  kLevel2 = 20,
  kLevel2_1 = 21,
  kLevel2_2 = 22,
  kLevel3 = 30,
  kLevel3_1 = 31,
  kLevel3_2 = 32,
  kLevel4 = 40,
  kLevel4_1 = 41,
  kLevel4_2 = 42,
  kLevel5 = 50,
  kLevel5_1 = 51,
  kLevel5_2 = 52
};

struct H264ProfileLevelId {
  constexpr H264ProfileLevelId(H264Profile profile, H264Level level)
      : profile(profile), level(level) {}
  H264Profile profile;
  H264Level level;
};

absl::optional<H264ProfileLevelId> ParseH264ProfileLevelId(const char* str);

RTC_EXPORT absl::optional<H264ProfileLevelId> ParseSdpForH264ProfileLevelId(
    const CodecParameterMap& params);

RTC_EXPORT absl::optional<H264Level> H264SupportedLevel(
    int max_frame_pixel_count,
    float max_fps);

RTC_EXPORT absl::optional<std::string> H264ProfileLevelIdToString(
    const H264ProfileLevelId& profile_level_id);

RTC_EXPORT bool H264IsSameProfile(const CodecParameterMap& params1,
                                  const CodecParameterMap& params2);

}

#endif
