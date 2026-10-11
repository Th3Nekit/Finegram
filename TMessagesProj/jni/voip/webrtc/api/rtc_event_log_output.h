/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTC_EVENT_LOG_OUTPUT_H_
#define API_RTC_EVENT_LOG_OUTPUT_H_

#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

class RtcEventLogOutput {
 public:
  virtual ~RtcEventLogOutput() = default;

  virtual bool IsActive() const = 0;

  virtual bool Write(absl::string_view output) = 0;

  virtual void Flush() {}
};

}

#endif
