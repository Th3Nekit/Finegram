/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtcp_mux_filter.h"

#include "rtc_base/logging.h"

namespace cricket {

RtcpMuxFilter::RtcpMuxFilter() : state_(ST_INIT), offer_enable_(false) {}

bool RtcpMuxFilter::IsFullyActive() const {
  return state_ == ST_ACTIVE;
}

bool RtcpMuxFilter::IsProvisionallyActive() const {
  return state_ == ST_SENTPRANSWER || state_ == ST_RECEIVEDPRANSWER;
}

bool RtcpMuxFilter::IsActive() const {
  return IsFullyActive() || IsProvisionallyActive();
}

void RtcpMuxFilter::SetActive() {
  state_ = ST_ACTIVE;
}

bool RtcpMuxFilter::SetOffer(bool offer_enable, ContentSource src) {
  if (state_ == ST_ACTIVE) {

    return offer_enable;
  }

  if (!ExpectOffer(offer_enable, src)) {
    RTC_LOG(LS_ERROR) << "Invalid state for change of RTCP mux offer";
    return false;
  }

  offer_enable_ = offer_enable;
  state_ = (src == CS_LOCAL) ? ST_SENTOFFER : ST_RECEIVEDOFFER;
  return true;
}

bool RtcpMuxFilter::SetProvisionalAnswer(bool answer_enable,
                                         ContentSource src) {
  if (state_ == ST_ACTIVE) {

    return answer_enable;
  }

  if (!ExpectAnswer(src)) {
    RTC_LOG(LS_ERROR) << "Invalid state for RTCP mux provisional answer";
    return false;
  }

  if (offer_enable_) {
    if (answer_enable) {
      if (src == CS_REMOTE)
        state_ = ST_RECEIVEDPRANSWER;
      else
        state_ = ST_SENTPRANSWER;
    } else {

      if (src == CS_REMOTE)
        state_ = ST_SENTOFFER;
      else
        state_ = ST_RECEIVEDOFFER;
    }
  } else if (answer_enable) {

    RTC_LOG(LS_WARNING) << "Invalid parameters in RTCP mux provisional answer";
    return false;
  }

  return true;
}

bool RtcpMuxFilter::SetAnswer(bool answer_enable, ContentSource src) {
  if (state_ == ST_ACTIVE) {

    return answer_enable;
  }

  if (!ExpectAnswer(src)) {
    RTC_LOG(LS_ERROR) << "Invalid state for RTCP mux answer, state is "
                      << state_ << ", source is " << src;
    return false;
  }

  if (offer_enable_ && answer_enable) {
    state_ = ST_ACTIVE;
  } else if (answer_enable) {

    RTC_LOG(LS_WARNING) << "Invalid parameters in RTCP mux answer";
    return false;
  } else {
    state_ = ST_INIT;
  }
  return true;
}

bool RtcpMuxFilter::ExpectOffer(bool offer_enable, ContentSource source) {
  return ((state_ == ST_INIT) ||
          (state_ == ST_ACTIVE && offer_enable == offer_enable_) ||
          (state_ == ST_SENTOFFER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_REMOTE));
}

bool RtcpMuxFilter::ExpectAnswer(ContentSource source) {
  return ((state_ == ST_SENTOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_SENTPRANSWER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDPRANSWER && source == CS_REMOTE));
}

}
