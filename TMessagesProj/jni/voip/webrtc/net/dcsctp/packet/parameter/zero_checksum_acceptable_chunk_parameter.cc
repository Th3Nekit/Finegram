/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/parameter/zero_checksum_acceptable_chunk_parameter.h"

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/strings/string_builder.h"

namespace dcsctp {

constexpr int ZeroChecksumAcceptableChunkParameter::kType;

absl::optional<ZeroChecksumAcceptableChunkParameter>
ZeroChecksumAcceptableChunkParameter::Parse(
    rtc::ArrayView<const uint8_t> data) {
  absl::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return absl::nullopt;
  }

  ZeroChecksumAlternateErrorDetectionMethod method(reader->Load32<4>());
  if (method == ZeroChecksumAlternateErrorDetectionMethod::None()) {
    return absl::nullopt;
  }
  return ZeroChecksumAcceptableChunkParameter(method);
}

void ZeroChecksumAcceptableChunkParameter::SerializeTo(
    std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer = AllocateTLV(out);
  writer.Store32<4>(*error_detection_method_);
}

std::string ZeroChecksumAcceptableChunkParameter::ToString() const {
  rtc::StringBuilder sb;
  sb << "Zero Checksum Acceptable (" << *error_detection_method_ << ")";
  return sb.Release();
}
}
