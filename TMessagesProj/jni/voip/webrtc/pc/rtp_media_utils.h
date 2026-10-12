/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_MEDIA_UTILS_H_
#define PC_RTP_MEDIA_UTILS_H_

#include <ostream>

#include "api/rtp_transceiver_direction.h"

namespace webrtc {

RtpTransceiverDirection RtpTransceiverDirectionFromSendRecv(bool send,
                                                            bool recv);

bool RtpTransceiverDirectionHasSend(RtpTransceiverDirection direction);

bool RtpTransceiverDirectionHasRecv(RtpTransceiverDirection direction);

RtpTransceiverDirection RtpTransceiverDirectionReversed(
    RtpTransceiverDirection direction);

RtpTransceiverDirection RtpTransceiverDirectionWithSendSet(
    RtpTransceiverDirection direction,
    bool send = true);

RtpTransceiverDirection RtpTransceiverDirectionWithRecvSet(
    RtpTransceiverDirection direction,
    bool recv = true);

const char* RtpTransceiverDirectionToString(RtpTransceiverDirection direction);

RtpTransceiverDirection RtpTransceiverDirectionIntersection(
    RtpTransceiverDirection lhs,
    RtpTransceiverDirection rhs);

#ifdef WEBRTC_UNIT_TEST
inline std::ostream& operator<<(
    std::ostream& os,
    RtpTransceiverDirection direction) {
  return os << RtpTransceiverDirectionToString(direction);
}
#endif

}

#endif
