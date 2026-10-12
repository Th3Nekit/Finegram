/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_AV1_PROFILE_H_
#define API_VIDEO_CODECS_AV1_PROFILE_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class AV1Profile {
  kProfile0 = 0,
  kProfile1 = 1,
  kProfile2 = 2,
};

RTC_EXPORT absl::string_view AV1ProfileToString(AV1Profile profile);

absl::optional<AV1Profile> StringToAV1Profile(absl::string_view profile);

RTC_EXPORT absl::optional<AV1Profile> ParseSdpForAV1Profile(
    const CodecParameterMap& params);

bool AV1IsSameProfile(const CodecParameterMap& params1,
                      const CodecParameterMap& params2);

}

#endif
