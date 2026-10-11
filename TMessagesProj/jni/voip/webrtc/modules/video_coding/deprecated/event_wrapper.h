/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_DEPRECATED_EVENT_WRAPPER_H_
#define MODULES_VIDEO_CODING_DEPRECATED_EVENT_WRAPPER_H_

namespace webrtc {
enum EventTypeWrapper { kEventSignaled = 1, kEventTimeout = 2 };

class EventWrapper {
 public:

  static EventWrapper* Create();

  virtual ~EventWrapper() {}

  virtual bool Set() = 0;

  virtual EventTypeWrapper Wait(int max_time_ms) = 0;
};

}

#endif
