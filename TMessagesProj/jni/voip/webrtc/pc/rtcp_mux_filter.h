/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTCP_MUX_FILTER_H_
#define PC_RTCP_MUX_FILTER_H_

#include "pc/session_description.h"

namespace cricket {

class RtcpMuxFilter {
 public:
  RtcpMuxFilter();

  bool IsFullyActive() const;

  bool IsProvisionallyActive() const;

  bool IsActive() const;

  void SetActive();

  bool SetOffer(bool offer_enable, ContentSource src);

  bool SetProvisionalAnswer(bool answer_enable, ContentSource src);

  bool SetAnswer(bool answer_enable, ContentSource src);

 private:
  bool ExpectOffer(bool offer_enable, ContentSource source);
  bool ExpectAnswer(ContentSource source);
  enum State {

    ST_INIT,

    ST_RECEIVEDOFFER,

    ST_SENTOFFER,

    ST_SENTPRANSWER,

    ST_RECEIVEDPRANSWER,

    ST_ACTIVE
  };
  State state_;
  bool offer_enable_;
};

}

#endif
