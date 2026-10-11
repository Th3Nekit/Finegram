/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/error_cause/restart_of_an_association_with_new_address_cause.h"

#include <stdint.h>

#include <type_traits>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/packet/bounded_byte_reader.h"
#include "net/dcsctp/packet/bounded_byte_writer.h"
#include "net/dcsctp/packet/tlv_trait.h"

namespace dcsctp {

constexpr int RestartOfAnAssociationWithNewAddressesCause::kType;

absl::optional<RestartOfAnAssociationWithNewAddressesCause>
RestartOfAnAssociationWithNewAddressesCause::Parse(
    rtc::ArrayView<const uint8_t> data) {
  absl::optional<BoundedByteReader<kHeaderSize>> reader = ParseTLV(data);
  if (!reader.has_value()) {
    return absl::nullopt;
  }
  return RestartOfAnAssociationWithNewAddressesCause(reader->variable_data());
}

void RestartOfAnAssociationWithNewAddressesCause::SerializeTo(
    std::vector<uint8_t>& out) const {
  BoundedByteWriter<kHeaderSize> writer =
      AllocateTLV(out, new_address_tlvs_.size());
  writer.CopyToVariableData(new_address_tlvs_);
}

std::string RestartOfAnAssociationWithNewAddressesCause::ToString() const {
  return "Restart of an Association with New Addresses";
}

}
