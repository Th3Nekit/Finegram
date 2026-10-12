/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_BITSTREAM_PARSER_H_
#define API_VIDEO_CODECS_BITSTREAM_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {

class BitstreamParser {
 public:
  virtual ~BitstreamParser() = default;

  virtual void ParseBitstream(rtc::ArrayView<const uint8_t> bitstream) = 0;

  virtual absl::optional<int> GetLastSliceQp() const = 0;
};

}

#endif
