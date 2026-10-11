/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_H265_PROFILE_TIER_LEVEL_H_
#define API_VIDEO_CODECS_H265_PROFILE_TIER_LEVEL_H_

#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class H265Profile {
  kProfileMain = 1,
  kProfileMain10 = 2,
  kProfileMainStill = 3,
  kProfileRangeExtensions = 4,
  kProfileHighThroughput = 5,
  kProfileMultiviewMain = 6,
  kProfileScalableMain = 7,
  kProfile3dMain = 8,
  kProfileScreenContentCoding = 9,
  kProfileScalableRangeExtensions = 10,
  kProfileHighThroughputScreenContentCoding = 11,
};

enum class H265Tier {
  kTier0,
  kTier1,
};

enum class H265Level {
  kLevel1 = 30,
  kLevel2 = 60,
  kLevel2_1 = 63,
  kLevel3 = 90,
  kLevel3_1 = 93,
  kLevel4 = 120,
  kLevel4_1 = 123,
  kLevel5 = 150,
  kLevel5_1 = 153,
  kLevel5_2 = 156,
  kLevel6 = 180,
  kLevel6_1 = 183,
  kLevel6_2 = 186,
};

struct H265ProfileTierLevel {
  constexpr H265ProfileTierLevel(H265Profile profile,
                                 H265Tier tier,
                                 H265Level level)
      : profile(profile), tier(tier), level(level) {}
  H265Profile profile;
  H265Tier tier;
  H265Level level;
};

RTC_EXPORT std::string H265ProfileToString(H265Profile profile);

RTC_EXPORT std::string H265TierToString(H265Tier tier);

RTC_EXPORT std::string H265LevelToString(H265Level level);

RTC_EXPORT absl::optional<H265Profile> StringToH265Profile(
    const std::string& profile);

RTC_EXPORT absl::optional<H265Tier> StringToH265Tier(const std::string& tier);

RTC_EXPORT absl::optional<H265Level> StringToH265Level(
    const std::string& level);

RTC_EXPORT absl::optional<H265ProfileTierLevel> ParseSdpForH265ProfileTierLevel(
    const CodecParameterMap& params);

RTC_EXPORT bool H265IsSameProfileTierLevel(const CodecParameterMap& params1,
                                           const CodecParameterMap& params2);

}

#endif
