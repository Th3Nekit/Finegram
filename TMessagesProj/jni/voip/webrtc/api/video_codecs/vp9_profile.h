/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP9_PROFILE_H_
#define API_VIDEO_CODECS_VP9_PROFILE_H_

#include <string>

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

extern RTC_EXPORT const char kVP9FmtpProfileId[];

enum class VP9Profile {
  kProfile0,
  kProfile1,
  kProfile2,
  kProfile3,
};

RTC_EXPORT std::string VP9ProfileToString(VP9Profile profile);

absl::optional<VP9Profile> StringToVP9Profile(const std::string& str);

RTC_EXPORT absl::optional<VP9Profile> ParseSdpForVP9Profile(
    const CodecParameterMap& params);

bool VP9IsSameProfile(const CodecParameterMap& params1,
                      const CodecParameterMap& params2);

}

#endif
