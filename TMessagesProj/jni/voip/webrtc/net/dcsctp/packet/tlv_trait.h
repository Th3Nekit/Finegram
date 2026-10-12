/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_TLV_TRAIT_H_
#define NET_DCSCTP_PACKET_TLV_TRAIT_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"

namespace dcsctp {
namespace tlv_trait_impl {

void ReportInvalidSize(size_t actual_size, size_t expected_size);
void ReportInvalidType(int actual_type, int expected_type);
void ReportInvalidFixedLengthField(size_t value, size_t expected);
void ReportInvalidVariableLengthField(size_t value, size_t available);
void ReportInvalidPadding(size_t padding_bytes);
void ReportInvalidLengthMultiple(size_t length, size_t alignment);
}

template <typename Config>
class TLVTrait {
 private:
  static constexpr size_t kTlvHeaderSize = 4;

 protected:
  static constexpr size_t kHeaderSize = Config::kHeaderSize;

  static_assert(Config::kTypeSizeInBytes == 1 || Config::kTypeSizeInBytes == 2,
                "kTypeSizeInBytes must be 1 or 2");
  static_assert(Config::kHeaderSize >= kTlvHeaderSize,
                "HeaderSize must be >= 4 bytes");
  static_assert((Config::kHeaderSize % 4 == 0),
                "kHeaderSize must be an even multiple of 4 bytes");
  static_assert((Config::kVariableLengthAlignment == 0 ||
                 Config::kVariableLengthAlignment == 1 ||
                 Config::kVariableLengthAlignment == 2 ||
                 Config::kVariableLengthAlignment == 4 ||
                 Config::kVariableLengthAlignment == 8),
                "kVariableLengthAlignment must be an allowed value");

  static absl::optional<BoundedByteReader<Config::kHeaderSize>> ParseTLV(
      rtc::ArrayView<const uint8_t> data) {
    if (data.size() < Config::kHeaderSize) {
      tlv_trait_impl::ReportInvalidSize(data.size(), Config::kHeaderSize);
      return absl::nullopt;
    }
    BoundedByteReader<kTlvHeaderSize> tlv_header(data);

    const int type = (Config::kTypeSizeInBytes == 1)
                         ? tlv_header.template Load8<0>()
                         : tlv_header.template Load16<0>();

    if (type != Config::kType) {
      tlv_trait_impl::ReportInvalidType(type, Config::kType);
      return absl::nullopt;
    }
    const uint16_t length = tlv_header.template Load16<2>();
    if (Config::kVariableLengthAlignment == 0) {

      if (length != Config::kHeaderSize || data.size() != Config::kHeaderSize) {
        tlv_trait_impl::ReportInvalidFixedLengthField(length,
                                                      Config::kHeaderSize);
        return absl::nullopt;
      }
    } else {

      if (length > data.size() || length < Config::kHeaderSize) {
        tlv_trait_impl::ReportInvalidVariableLengthField(length, data.size());
        return absl::nullopt;
      }
      const size_t padding = data.size() - length;
      if (padding > 3) {

        tlv_trait_impl::ReportInvalidPadding(padding);
        return absl::nullopt;
      }
      if (!ValidateLengthAlignment(length, Config::kVariableLengthAlignment)) {
        tlv_trait_impl::ReportInvalidLengthMultiple(
            length, Config::kVariableLengthAlignment);
        return absl::nullopt;
      }
    }
    return BoundedByteReader<Config::kHeaderSize>(data.subview(0, length));
  }

  static BoundedByteWriter<Config::kHeaderSize> AllocateTLV(
      std::vector<uint8_t>& out,
      size_t variable_size = 0) {
    const size_t offset = out.size();
    const size_t size = Config::kHeaderSize + variable_size;
    out.resize(offset + size);

    BoundedByteWriter<kTlvHeaderSize> tlv_header(
        rtc::ArrayView<uint8_t>(out.data() + offset, kTlvHeaderSize));
    if (Config::kTypeSizeInBytes == 1) {
      tlv_header.template Store8<0>(static_cast<uint8_t>(Config::kType));
    } else {
      tlv_header.template Store16<0>(Config::kType);
    }
    tlv_header.template Store16<2>(size);

    return BoundedByteWriter<Config::kHeaderSize>(
        rtc::ArrayView<uint8_t>(out.data() + offset, size));
  }

 private:
  static bool ValidateLengthAlignment(uint16_t length, size_t alignment) {

    if (alignment == 0) {
      return true;
    }
    return (length % alignment) == 0;
  }
};

}

#endif
