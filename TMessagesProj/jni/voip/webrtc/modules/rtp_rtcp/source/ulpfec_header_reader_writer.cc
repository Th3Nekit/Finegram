/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/ulpfec_header_reader_writer.h"

#include <string.h>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

constexpr size_t kMaxMediaPackets = 48;

constexpr size_t kMaxTrackedMediaPackets = 4 * kMaxMediaPackets;

constexpr size_t kMaxFecPackets = kMaxMediaPackets;

constexpr size_t kFecLevel0HeaderSize = 10;

constexpr size_t kFecLevel1HeaderSizeLBitSet = 2 + kUlpfecPacketMaskSizeLBitSet;

constexpr size_t kFecLevel1HeaderSizeLBitClear =
    2 + kUlpfecPacketMaskSizeLBitClear;

constexpr size_t kPacketMaskOffset = kFecLevel0HeaderSize + 2;

size_t UlpfecHeaderSize(size_t packet_mask_size) {
  RTC_DCHECK_LE(packet_mask_size, kUlpfecPacketMaskSizeLBitSet);
  if (packet_mask_size <= kUlpfecPacketMaskSizeLBitClear) {
    return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitClear;
  } else {
    return kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet;
  }
}

}

UlpfecHeaderReader::UlpfecHeaderReader()
    : FecHeaderReader(kMaxTrackedMediaPackets, kMaxFecPackets) {}

UlpfecHeaderReader::~UlpfecHeaderReader() = default;

bool UlpfecHeaderReader::ReadFecHeader(
    ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const {
  uint8_t* data = fec_packet->pkt->data.MutableData();
  if (fec_packet->pkt->data.size() < kPacketMaskOffset) {
    return false;
  }
  bool l_bit = (data[0] & 0x40) != 0u;
  size_t packet_mask_size =
      l_bit ? kUlpfecPacketMaskSizeLBitSet : kUlpfecPacketMaskSizeLBitClear;
  fec_packet->fec_header_size = UlpfecHeaderSize(packet_mask_size);
  uint16_t seq_num_base = ByteReader<uint16_t>::ReadBigEndian(&data[2]);
  fec_packet->protected_streams = {{.ssrc = fec_packet->ssrc,
                                    .seq_num_base = seq_num_base,
                                    .packet_mask_offset = kPacketMaskOffset,
                                    .packet_mask_size = packet_mask_size}};
  fec_packet->protection_length =
      ByteReader<uint16_t>::ReadBigEndian(&data[10]);

  memcpy(&data[2], &data[8], 2);

  return true;
}

UlpfecHeaderWriter::UlpfecHeaderWriter()
    : FecHeaderWriter(kMaxMediaPackets,
                      kMaxFecPackets,
                      kFecLevel0HeaderSize + kFecLevel1HeaderSizeLBitSet) {}

UlpfecHeaderWriter::~UlpfecHeaderWriter() = default;

size_t UlpfecHeaderWriter::MinPacketMaskSize(const uint8_t* packet_mask,
                                             size_t packet_mask_size) const {
  return packet_mask_size;
}

size_t UlpfecHeaderWriter::FecHeaderSize(size_t packet_mask_size) const {
  return UlpfecHeaderSize(packet_mask_size);
}

void UlpfecHeaderWriter::FinalizeFecHeader(
    rtc::ArrayView<const ProtectedStream> protected_streams,
    ForwardErrorCorrection::Packet& fec_packet) const {
  RTC_CHECK_EQ(protected_streams.size(), 1);
  uint16_t seq_num_base = protected_streams[0].seq_num_base;
  const uint8_t* packet_mask = protected_streams[0].packet_mask.data();
  size_t packet_mask_size = protected_streams[0].packet_mask.size();

  uint8_t* data = fec_packet.data.MutableData();

  data[0] &= 0x7f;

  bool l_bit = (packet_mask_size == kUlpfecPacketMaskSizeLBitSet);
  if (l_bit) {
    data[0] |= 0x40;
  } else {
    RTC_DCHECK_EQ(packet_mask_size, kUlpfecPacketMaskSizeLBitClear);
    data[0] &= 0xbf;
  }

  memcpy(&data[8], &data[2], 2);

  ByteWriter<uint16_t>::WriteBigEndian(&data[2], seq_num_base);

  const size_t fec_header_size = FecHeaderSize(packet_mask_size);
  ByteWriter<uint16_t>::WriteBigEndian(
      &data[10], fec_packet.data.size() - fec_header_size);

  memcpy(&data[12], packet_mask, packet_mask_size);
}

}
