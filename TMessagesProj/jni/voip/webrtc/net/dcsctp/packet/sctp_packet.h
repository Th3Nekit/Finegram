/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_SCTP_PACKET_H_
#define NET_DCSCTP_PACKET_SCTP_PACKET_H_

#include <stddef.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/chunk/chunk.h"
#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {

struct CommonHeader {
  uint16_t source_port;
  uint16_t destination_port;
  VerificationTag verification_tag;
  uint32_t checksum;
};

class SctpPacket {
 public:
  static constexpr size_t kHeaderSize = 12;

  struct ChunkDescriptor {
    ChunkDescriptor(uint8_t type,
                    uint8_t flags,
                    rtc::ArrayView<const uint8_t> data)
        : type(type), flags(flags), data(data) {}
    uint8_t type;
    uint8_t flags;
    rtc::ArrayView<const uint8_t> data;
  };

  SctpPacket(SctpPacket&& other) = default;
  SctpPacket& operator=(SctpPacket&& other) = default;
  SctpPacket(const SctpPacket&) = delete;
  SctpPacket& operator=(const SctpPacket&) = delete;

  class Builder {
   public:
    Builder(VerificationTag verification_tag, const DcSctpOptions& options);

    Builder(Builder&& other) = default;
    Builder& operator=(Builder&& other) = default;

    Builder& Add(const Chunk& chunk);

    size_t bytes_remaining() const;

    bool empty() const { return out_.empty(); }

    std::vector<uint8_t> Build(bool write_checksum = true);

   private:
    VerificationTag verification_tag_;
    uint16_t source_port_;
    uint16_t dest_port_;

    size_t max_packet_size_;
    std::vector<uint8_t> out_;
  };

  static absl::optional<SctpPacket> Parse(rtc::ArrayView<const uint8_t> data,
                                          const DcSctpOptions& options);

  const CommonHeader& common_header() const { return common_header_; }

  rtc::ArrayView<const ChunkDescriptor> descriptors() const {
    return descriptors_;
  }

 private:
  SctpPacket(const CommonHeader& common_header,
             std::vector<uint8_t> data,
             std::vector<ChunkDescriptor> descriptors)
      : common_header_(common_header),
        data_(std::move(data)),
        descriptors_(std::move(descriptors)) {}

  CommonHeader common_header_;

  std::vector<uint8_t> data_;

  std::vector<ChunkDescriptor> descriptors_;
};
}

#endif
