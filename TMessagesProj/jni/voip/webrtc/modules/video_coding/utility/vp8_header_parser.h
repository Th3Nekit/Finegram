/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_VP8_HEADER_PARSER_H_
#define MODULES_VIDEO_CODING_UTILITY_VP8_HEADER_PARSER_H_

#include <stdint.h>
#include <stdio.h>

namespace webrtc {

namespace vp8 {

typedef struct VP8BitReader VP8BitReader;
struct VP8BitReader {

  uint32_t value_;
  uint32_t range_;
  int bits_;

  const uint8_t* buf_;
  const uint8_t* buf_end_;
};

bool GetQp(const uint8_t* buf, size_t length, int* qp);

}

}

#endif
