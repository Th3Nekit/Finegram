/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_vps_parser.h"
#include "common_video/h265/legacy_bit_buffer.h"
#include "rtc_base/logging.h"

namespace {
typedef absl::optional<webrtc::H265VpsParser::VpsState> OptionalVps;

#define RETURN_EMPTY_ON_FAIL(x) \
  if (!(x)) {                   \
    return OptionalVps();       \
  }
}

namespace webrtc {

H265VpsParser::VpsState::VpsState() = default;

absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseVps(
    const uint8_t* data,
    size_t length) {
  std::vector<uint8_t> unpacked_buffer = H265::ParseRbsp(data, length);
  rtc::BitBuffer bit_buffer(unpacked_buffer.data(), unpacked_buffer.size());
  return ParseInternal(&bit_buffer);
}

absl::optional<H265VpsParser::VpsState> H265VpsParser::ParseInternal(
    rtc::BitBuffer* buffer) {

  VpsState vps;

  vps.id = 0;
  RETURN_EMPTY_ON_FAIL(buffer->ReadBits(&vps.id, 4));

  return OptionalVps(vps);
}

}
