/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_PARAMETERS_CONVERSION_H_
#define PC_RTP_PARAMETERS_CONVERSION_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "media/base/codec.h"
#include "media/base/stream_params.h"
#include "pc/session_description.h"

namespace webrtc {

RTCErrorOr<cricket::FeedbackParam> ToCricketFeedbackParam(
    const RtcpFeedback& feedback);

RTCErrorOr<cricket::Codec> ToCricketCodec(const RtpCodecParameters& codec);

RTCErrorOr<std::vector<cricket::Codec>> ToCricketCodecs(
    const std::vector<RtpCodecParameters>& codecs);

RTCErrorOr<cricket::StreamParamsVec> ToCricketStreamParamsVec(
    const std::vector<RtpEncodingParameters>& encodings);

absl::optional<RtcpFeedback> ToRtcpFeedback(
    const cricket::FeedbackParam& cricket_feedback);

std::vector<RtpEncodingParameters> ToRtpEncodings(
    const cricket::StreamParamsVec& stream_params);

RtpCodecParameters ToRtpCodecParameters(const cricket::Codec& cricket_codec);
RtpCodecCapability ToRtpCodecCapability(const cricket::Codec& cricket_codec);

RtpCapabilities ToRtpCapabilities(
    const std::vector<cricket::Codec>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions);

RtpParameters ToRtpParameters(
    const std::vector<cricket::Codec>& cricket_codecs,
    const cricket::RtpHeaderExtensions& cricket_extensions,
    const cricket::StreamParamsVec& stream_params);

}

#endif
