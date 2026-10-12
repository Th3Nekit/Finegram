/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_COMMON_INTERNAL_TYPES_H_
#define NET_DCSCTP_COMMON_INTERNAL_TYPES_H_

#include <functional>
#include <utility>

#include "net/dcsctp/public/types.h"
#include "rtc_base/strong_alias.h"

namespace dcsctp {

using SSN = webrtc::StrongAlias<class SSNTag, uint16_t>;

using MID = webrtc::StrongAlias<class MIDTag, uint32_t>;

using FSN = webrtc::StrongAlias<class FSNTag, uint32_t>;

using TSN = webrtc::StrongAlias<class TSNTag, uint32_t>;

using ReconfigRequestSN =
    webrtc::StrongAlias<class ReconfigRequestSNTag, uint32_t>;

using VerificationTag = webrtc::StrongAlias<class VerificationTagTag, uint32_t>;

using TieTag = webrtc::StrongAlias<class TieTagTag, uint64_t>;

using OutgoingMessageId =
    webrtc::StrongAlias<class OutgoingMessageIdTag, uint32_t>;

}
#endif
