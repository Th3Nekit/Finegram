/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PORTAL_PIPEWIRE_UTILS_H_
#define MODULES_PORTAL_PIPEWIRE_UTILS_H_

struct pw_thread_loop;

namespace webrtc {

constexpr int kInvalidPipeWireFd = -1;

bool InitializePipeWire();

class PipeWireThreadLoopLock {
 public:
  explicit PipeWireThreadLoopLock(pw_thread_loop* loop);
  ~PipeWireThreadLoopLock();

 private:
  pw_thread_loop* const loop_;
};

}

#endif
