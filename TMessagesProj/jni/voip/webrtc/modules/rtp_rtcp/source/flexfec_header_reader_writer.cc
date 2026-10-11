/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/flexfec_header_reader_writer.h"

#include <string.h>

#include "api/scoped_refptr.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

constexpr size_t kMaxMediaPackets = 48;

constexpr size_t kMaxTrackedMediaPackets = 4 * kMaxMediaPackets;

constexpr size_t kMaxFecPackets = kMaxMediaPackets;

constexpr size_t kFlexfecPacketMaskSizes[] = {2, 6, 14};

constexpr size_t kBaseHeaderSize = 8;

constexpr size_t kStreamSpecificHeaderSize = 2;

constexpr size_t kHeaderSizes[] = {
    kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizes[0],
    kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizes[1],
    kBaseHeaderSize + kStreamSpecificHeaderSize + kFlexfecPacketMaskSizes[2]};

size_t FlexfecHeaderSize(size_t packet_mask_size) {
  RTC_DCHECK_LE(packet_mask_size, kFlexfecPacketMaskSizes[2]);
  if (packet_mask_size <= kFlexfecPacketMaskSizes[0]) {
    return kHeaderSizes[0];
  } else if (packet_mask_size <= kFlexfecPacketMaskSizes[1]) {
    return kHeaderSizes[1];
  }
  return kHeaderSizes[2];
}

}

FlexfecHeaderReader::FlexfecHeaderReader()
    : FecHeaderReader(kMaxTrackedMediaPackets, kMaxFecPackets) {}

FlexfecHeaderReader::~FlexfecHeaderReader() = default;

bool FlexfecHeaderReader::ReadFecHeader(
    ForwardErrorCorrection::ReceivedFecPacket* fec_packet) const {

  if (fec_packet->protected_streams.empty()) {
    RTC_LOG(LS_WARNING)
        << "Discarding FlexFEC packet with no protected sources.";
    return false;
  }
  if (fec_packet->pkt->data.size() <=
      kBaseHeaderSize + kStreamSpecificHeaderSize) {
    RTC_LOG(LS_WARNING) << "Discarding truncated FlexFEC packet.";
    return false;
  }
  uint8_t* const data = fec_packet->pkt->data.MutableData();
  bool r_bit = (data[0] & 0x80) != 0;
  if (r_bit) {
    RTC_LOG(LS_INFO)
        << "FlexFEC packet with retransmission bit set. We do not yet "
           "support this, thus discarding the packet.";
    return false;
  }
  bool f_bit = (data[0] & 0x40) != 0;
  if (f_bit) {
    RTC_LOG(LS_INFO)
        << "FlexFEC packet with inflexible generator matrix. We do "
           "not yet support this, thus discarding packet.";
    return false;
  }

  size_t byte_index = 8;
  for (size_t i = 0; i < fec_packet->protected_streams.size(); ++i) {
    if (fec_packet->pkt->data.size() < byte_index + kStreamSpecificHeaderSize) {
      RTC_LOG(LS_WARNING) << "Discarding truncated FlexFEC packet.";
      return false;
    }

    fec_packet->protected_streams[i].seq_num_base =
        ByteReader<uint16_t>::ReadBigEndian(&data[byte_index]);
    byte_index += kStreamSpecificHeaderSize;

    if (fec_packet->pkt->data.size() <
        (byte_index + kFlexfecPacketMaskSizes[0])) {
      RTC_LOG(LS_WARNING) << "Discarding truncated FlexFEC packet.";
      return false;
    }
    fec_packet->protected_streams[i].packet_mask_offset = byte_index;
    bool k_bit0 = (data[byte_index] & 0x80) != 0;
    uint16_t mask_part0 =
        ByteReader<uint16_t>::ReadBigEndian(&data[byte_index]);

    mask_part0 <<= 1;
    ByteWriter<uint16_t>::WriteBigEndian(&data[byte_index], mask_part0);
    byte_index += kFlexfecPacketMaskSizes[0];
    if (!k_bit0) {

      fec_packet->protected_streams[i].packet_mask_size =
          kFlexfecPacketMaskSizes[0];
    } else {
      if (fec_packet->pkt->data.size() <
          (byte_index + kFlexfecPacketMaskSizes[1] -
           kFlexfecPacketMaskSizes[0])) {
        return false;
      }
      bool k_bit1 = (data[byte_index] & 0x80) != 0;

      uint8_t bit15 = (data[byte_index] >> 6) & 0x01;
      data[byte_index - 1] |= bit15;
      uint32_t mask_part1 =
          ByteReader<uint32_t>::ReadBigEndian(&data[byte_index]);

      mask_part1 <<= 2;
      ByteWriter<uint32_t>::WriteBigEndian(&data[byte_index], mask_part1);
      byte_index += kFlexfecPacketMaskSizes[1] - kFlexfecPacketMaskSizes[0];
      if (!k_bit1) {

        fec_packet->protected_streams[i].packet_mask_size =
            kFlexfecPacketMaskSizes[1];
      } else {
        if (fec_packet->pkt->data.size() <
            (byte_index + kFlexfecPacketMaskSizes[2] -
             kFlexfecPacketMaskSizes[1])) {
          RTC_LOG(LS_WARNING) << "Discarding truncated FlexFEC packet.";
          return false;
        }
        fec_packet->protected_streams[i].packet_mask_size =
            kFlexfecPacketMaskSizes[2];

        uint8_t tail_bits = (data[byte_index] >> 6) & 0x03;
        data[byte_index - 1] |= tail_bits;
        uint64_t mask_part2 =
            ByteReader<uint64_t>::ReadBigEndian(&data[byte_index]);

        mask_part2 <<= 2;
        ByteWriter<uint64_t>::WriteBigEndian(&data[byte_index], mask_part2);
        byte_index += kFlexfecPacketMaskSizes[2] - kFlexfecPacketMaskSizes[1];
      }
    }
  }

  fec_packet->fec_header_size = byte_index;

  fec_packet->protection_length =
      fec_packet->pkt->data.size() - fec_packet->fec_header_size;

  return true;
}

FlexfecHeaderWriter::FlexfecHeaderWriter()
    : FecHeaderWriter(kMaxMediaPackets, kMaxFecPackets, kHeaderSizes[2]) {}

FlexfecHeaderWriter::~FlexfecHeaderWriter() = default;

size_t FlexfecHeaderWriter::MinPacketMaskSize(const uint8_t* packet_mask,
                                              size_t packet_mask_size) const {
  if (packet_mask_size == kUlpfecPacketMaskSizeLBitClear &&
      (packet_mask[1] & 0x01) == 0) {

    return kFlexfecPacketMaskSizes[0];
  } else if (packet_mask_size == kUlpfecPacketMaskSizeLBitClear) {

    return kFlexfecPacketMaskSizes[1];
  } else if (packet_mask_size == kUlpfecPacketMaskSizeLBitSet &&
             (packet_mask[5] & 0x03) == 0) {

    return kFlexfecPacketMaskSizes[1];
  } else if (packet_mask_size == kUlpfecPacketMaskSizeLBitSet) {

    return kFlexfecPacketMaskSizes[2];
  }
  RTC_DCHECK_NOTREACHED() << "Incorrect packet mask size: " << packet_mask_size
                          << ".";
  return kFlexfecPacketMaskSizes[2];
}

size_t FlexfecHeaderWriter::FecHeaderSize(size_t packet_mask_size) const {
  return FlexfecHeaderSize(packet_mask_size);
}

void FlexfecHeaderWriter::FinalizeFecHeader(
    rtc::ArrayView<const ProtectedStream> protected_streams,
    ForwardErrorCorrection::Packet& fec_packet) const {
  uint8_t* data = fec_packet.data.MutableData();
  *data &= 0x7f;
  *data &= 0xbf;

  uint8_t* write_at = data + 8;
  for (const ProtectedStream& protected_stream : protected_streams) {
    ByteWriter<uint16_t>::WriteBigEndian(write_at,
                                         protected_stream.seq_num_base);
    write_at += kStreamSpecificHeaderSize;

    if (protected_stream.packet_mask.size() == kUlpfecPacketMaskSizeLBitSet) {

      uint16_t tmp_mask_part0 =
          ByteReader<uint16_t>::ReadBigEndian(&protected_stream.packet_mask[0]);
      uint32_t tmp_mask_part1 =
          ByteReader<uint32_t>::ReadBigEndian(&protected_stream.packet_mask[2]);

      tmp_mask_part0 >>= 1;
      ByteWriter<uint16_t>::WriteBigEndian(write_at, tmp_mask_part0);
      *write_at |= 0x80;
      write_at += kFlexfecPacketMaskSizes[0];
      tmp_mask_part1 >>= 2;
      ByteWriter<uint32_t>::WriteBigEndian(write_at, tmp_mask_part1);

      bool bit15 = (protected_stream.packet_mask[1] & 0x01) != 0;
      if (bit15)
        *write_at |= 0x40;

      bool bit46 = (protected_stream.packet_mask[5] & 0x02) != 0;
      bool bit47 = (protected_stream.packet_mask[5] & 0x01) != 0;
      if (!bit46 && !bit47) {
        write_at += kFlexfecPacketMaskSizes[1] - kFlexfecPacketMaskSizes[0];
      } else {
        *write_at |= 0x80;
        write_at += kFlexfecPacketMaskSizes[1] - kFlexfecPacketMaskSizes[0];

        memset(write_at, 0,
               kFlexfecPacketMaskSizes[2] - kFlexfecPacketMaskSizes[1]);
        if (bit46)
          *write_at |= 0x80;
        if (bit47)
          *write_at |= 0x40;
        write_at += kFlexfecPacketMaskSizes[2] - kFlexfecPacketMaskSizes[1];
      }
    } else if (protected_stream.packet_mask.size() ==
               kUlpfecPacketMaskSizeLBitClear) {

      uint16_t tmp_mask_part0 =
          ByteReader<uint16_t>::ReadBigEndian(&protected_stream.packet_mask[0]);

      tmp_mask_part0 >>= 1;
      ByteWriter<uint16_t>::WriteBigEndian(write_at, tmp_mask_part0);
      bool bit15 = (protected_stream.packet_mask[1] & 0x01) != 0;
      if (!bit15) {
        write_at += kFlexfecPacketMaskSizes[0];
      } else {
        *write_at |= 0x80;
        write_at += kFlexfecPacketMaskSizes[0];

        memset(write_at, 0U,
               kFlexfecPacketMaskSizes[1] - kFlexfecPacketMaskSizes[0]);
        *write_at |= 0x40;
        write_at += kFlexfecPacketMaskSizes[1] - kFlexfecPacketMaskSizes[0];
      }
    } else {
      RTC_DCHECK_NOTREACHED() << "Incorrect packet mask size: "
                              << protected_stream.packet_mask.size() << ".";
    }
  }
}

}
