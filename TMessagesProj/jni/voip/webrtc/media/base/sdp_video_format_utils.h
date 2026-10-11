/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_SDP_VIDEO_FORMAT_UTILS_H_
#define MEDIA_BASE_SDP_VIDEO_FORMAT_UTILS_H_

#include "absl/types/optional.h"
#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {

void H264GenerateProfileLevelIdForAnswer(
    const CodecParameterMap& local_supported_params,
    const CodecParameterMap& remote_offered_params,
    CodecParameterMap* answer_params);

#ifdef RTC_ENABLE_H265

void H265GenerateProfileTierLevelForAnswer(
    const CodecParameterMap& local_supported_params,
    const CodecParameterMap& remote_offered_params,
    CodecParameterMap* answer_params);
#endif

absl::optional<int> ParseSdpForVPxMaxFrameRate(const CodecParameterMap& params);

absl::optional<int> ParseSdpForVPxMaxFrameSize(const CodecParameterMap& params);

}

#endif
