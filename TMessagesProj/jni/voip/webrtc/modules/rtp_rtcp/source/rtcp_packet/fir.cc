/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/fir.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t Fir::kFeedbackMessageType;

Fir::Fir() = default;

Fir::Fir(const Fir& fir) = default;

Fir::~Fir() = default;

bool Fir::Parse(const CommonHeader& packet) {
  RTC_DCHECK_EQ(packet.type(), kPacketType);
  RTC_DCHECK_EQ(packet.fmt(), kFeedbackMessageType);

  if (packet.payload_size_bytes() < kCommonFeedbackLength + kFciLength) {
    RTC_LOG(LS_WARNING) << "Packet is too small to be a valid FIR packet.";
    return false;
  }

  if ((packet.payload_size_bytes() - kCommonFeedbackLength) % kFciLength != 0) {
    RTC_LOG(LS_WARNING) << "Invalid size for a valid FIR packet.";
    return false;
  }

  ParseCommonFeedback(packet.payload());

  size_t number_of_fci_items =
      (packet.payload_size_bytes() - kCommonFeedbackLength) / kFciLength;
  const uint8_t* next_fci = packet.payload() + kCommonFeedbackLength;
  items_.resize(number_of_fci_items);
  for (Request& request : items_) {
    request.ssrc = ByteReader<uint32_t>::ReadBigEndian(next_fci);
    request.seq_nr = ByteReader<uint8_t>::ReadBigEndian(next_fci + 4);
    next_fci += kFciLength;
  }
  return true;
}

size_t Fir::BlockLength() const {
  return kHeaderLength + kCommonFeedbackLength + kFciLength * items_.size();
}

bool Fir::Create(uint8_t* packet,
                 size_t* index,
                 size_t max_length,
                 PacketReadyCallback callback) const {
  RTC_DCHECK(!items_.empty());
  while (*index + BlockLength() > max_length) {
    if (!OnBufferFull(packet, index, callback))
      return false;
  }
  size_t index_end = *index + BlockLength();
  CreateHeader(kFeedbackMessageType, kPacketType, HeaderLength(), packet,
               index);
  RTC_DCHECK_EQ(Psfb::media_ssrc(), 0);
  CreateCommonFeedback(packet + *index);
  *index += kCommonFeedbackLength;

  constexpr uint32_t kReserved = 0;
  for (const Request& request : items_) {
    ByteWriter<uint32_t>::WriteBigEndian(packet + *index, request.ssrc);
    ByteWriter<uint8_t>::WriteBigEndian(packet + *index + 4, request.seq_nr);
    ByteWriter<uint32_t, 3>::WriteBigEndian(packet + *index + 5, kReserved);
    *index += kFciLength;
  }
  RTC_CHECK_EQ(*index, index_end);
  return true;
}
}
}
