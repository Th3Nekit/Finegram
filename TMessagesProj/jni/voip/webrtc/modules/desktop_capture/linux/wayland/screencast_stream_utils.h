/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_SCREENCAST_STREAM_UTILS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "rtc_base/string_encode.h"

struct spa_pod;
struct spa_pod_builder;
struct spa_rectangle;
struct spa_fraction;

namespace webrtc {

struct PipeWireVersion {
  static PipeWireVersion Parse(const absl::string_view& version);

  bool operator>=(const PipeWireVersion& other);

  bool operator<=(const PipeWireVersion& other);

  int major = 0;
  int minor = 0;
  int micro = 0;
};

spa_pod* BuildFormat(spa_pod_builder* builder,
                     uint32_t format,
                     const std::vector<uint64_t>& modifiers,
                     const struct spa_rectangle* resolution,
                     const struct spa_fraction* frame_rate);

}

#endif
