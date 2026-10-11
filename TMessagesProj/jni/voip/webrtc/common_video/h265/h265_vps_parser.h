/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_H265_VPS_PARSER_H_
#define COMMON_VIDEO_H265_H265_VPS_PARSER_H_

#include "absl/types/optional.h"

namespace rtc {
class BitBuffer;
}

namespace webrtc {

class H265VpsParser {
 public:

  struct VpsState {
    VpsState();

    uint32_t id = 0;
  };

  static absl::optional<VpsState> ParseVps(const uint8_t* data, size_t length);

 protected:

  static absl::optional<VpsState> ParseInternal(rtc::BitBuffer* bit_buffer);
};

}
#endif
