/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_ADAPTATION_CONSTRAINT_H_
#define CALL_ADAPTATION_ADAPTATION_CONSTRAINT_H_

#include <string>

#include "api/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"

namespace webrtc {

class AdaptationConstraint {
 public:
  virtual ~AdaptationConstraint();

  virtual std::string Name() const = 0;

  virtual bool IsAdaptationUpAllowed(
      const VideoStreamInputState& input_state,
      const VideoSourceRestrictions& restrictions_before,
      const VideoSourceRestrictions& restrictions_after) const = 0;
};

}

#endif
