/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TURN_CUSTOMIZER_H_
#define API_TURN_CUSTOMIZER_H_

#include <stdlib.h>

#include "api/transport/stun.h"

namespace cricket {
class PortInterface;
}

namespace webrtc {

class TurnCustomizer {
 public:

  virtual void MaybeModifyOutgoingStunMessage(
      cricket::PortInterface* port,
      cricket::StunMessage* message) = 0;

  virtual bool AllowChannelData(cricket::PortInterface* port,
                                const void* data,
                                size_t size,
                                bool payload) = 0;

  virtual ~TurnCustomizer() {}
};

}

#endif
