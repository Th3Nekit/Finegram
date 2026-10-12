/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contain functions for parsing and serializing SDP messages.
// Related RFC/draft including:
// * RFC 4566 - SDP
// * RFC 5245 - ICE
// * RFC 3388 - Grouping of Media Lines in SDP
// * RFC 4568 - SDP Security Descriptions for Media Streams
// * draft-lennox-mmusic-sdp-source-selection-02 -
//   Mechanisms for Media Source Selection in SDP

#ifndef PC_WEBRTC_SDP_H_
#define PC_WEBRTC_SDP_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "api/jsep_ice_candidate.h"
#include "api/jsep_session_description.h"
#include "media/base/codec.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {
class Candidate;
}

namespace rtc {
class StringBuilder;
}

namespace webrtc {
class IceCandidateInterface;
class JsepIceCandidate;
class JsepSessionDescription;
struct SdpParseError;

std::string SdpSerialize(const JsepSessionDescription& jdesc);

std::string SdpSerializeCandidate(const IceCandidateInterface& candidate);

RTC_EXPORT std::string SdpSerializeCandidate(
    const cricket::Candidate& candidate);

bool SdpDeserialize(absl::string_view message,
                    JsepSessionDescription* jdesc,
                    SdpParseError* error);

RTC_EXPORT bool SdpDeserializeCandidate(absl::string_view message,
                                        JsepIceCandidate* candidate,
                                        SdpParseError* error);

RTC_EXPORT bool SdpDeserializeCandidate(absl::string_view transport_name,
                                        absl::string_view message,
                                        cricket::Candidate* candidate,
                                        SdpParseError* error);

RTC_EXPORT bool ParseCandidate(absl::string_view message,
                               cricket::Candidate* candidate,
                               SdpParseError* error,
                               bool is_raw);

bool WriteFmtpParameters(const webrtc::CodecParameterMap& parameters,
                         rtc::StringBuilder* os);

}

#endif
