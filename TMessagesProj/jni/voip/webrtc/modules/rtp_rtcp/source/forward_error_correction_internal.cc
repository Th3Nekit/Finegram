/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"

#include <string.h>

#include <algorithm>

#include "modules/rtp_rtcp/source/fec_private_tables_bursty.h"
#include "modules/rtp_rtcp/source/fec_private_tables_random.h"
#include "rtc_base/checks.h"

namespace {

enum ProtectionMode {
  kModeNoOverlap,
  kModeOverlap,
  kModeBiasFirstPacket,
};

void FitSubMask(int num_mask_bytes,
                int num_sub_mask_bytes,
                int num_rows,
                const uint8_t* sub_mask,
                uint8_t* packet_mask) {
  if (num_mask_bytes == num_sub_mask_bytes) {
    memcpy(packet_mask, sub_mask, num_rows * num_sub_mask_bytes);
  } else {
    for (int i = 0; i < num_rows; ++i) {
      int pkt_mask_idx = i * num_mask_bytes;
      int pkt_mask_idx2 = i * num_sub_mask_bytes;
      for (int j = 0; j < num_sub_mask_bytes; ++j) {
        packet_mask[pkt_mask_idx] = sub_mask[pkt_mask_idx2];
        pkt_mask_idx++;
        pkt_mask_idx2++;
      }
    }
  }
}

void ShiftFitSubMask(int num_mask_bytes,
                     int res_mask_bytes,
                     int num_column_shift,
                     int end_row,
                     const uint8_t* sub_mask,
                     uint8_t* packet_mask) {

  const int num_bit_shifts = (num_column_shift % 8);
  const int num_byte_shifts = num_column_shift >> 3;

  for (int i = num_column_shift; i < end_row; ++i) {

    int pkt_mask_idx =
        i * num_mask_bytes + res_mask_bytes - 1 + num_byte_shifts;

    int pkt_mask_idx2 =
        (i - num_column_shift) * res_mask_bytes + res_mask_bytes - 1;

    uint8_t shift_right_curr_byte = 0;
    uint8_t shift_left_prev_byte = 0;
    uint8_t comb_new_byte = 0;

    if (num_mask_bytes > res_mask_bytes) {
      shift_left_prev_byte = (sub_mask[pkt_mask_idx2] << (8 - num_bit_shifts));
      packet_mask[pkt_mask_idx + 1] = shift_left_prev_byte;
    }

    for (int j = res_mask_bytes - 1; j > 0; j--) {

      shift_right_curr_byte = sub_mask[pkt_mask_idx2] >> num_bit_shifts;

      shift_left_prev_byte =
          (sub_mask[pkt_mask_idx2 - 1] << (8 - num_bit_shifts));

      comb_new_byte = shift_right_curr_byte | shift_left_prev_byte;

      packet_mask[pkt_mask_idx] = comb_new_byte;
      pkt_mask_idx--;
      pkt_mask_idx2--;
    }

    shift_right_curr_byte = sub_mask[pkt_mask_idx2] >> num_bit_shifts;
    packet_mask[pkt_mask_idx] = shift_right_curr_byte;
  }
}

}

namespace webrtc {
namespace internal {

PacketMaskTable::PacketMaskTable(FecMaskType fec_mask_type,
                                 int num_media_packets)
    : table_(PickTable(fec_mask_type, num_media_packets)) {}

PacketMaskTable::~PacketMaskTable() = default;

rtc::ArrayView<const uint8_t> PacketMaskTable::LookUp(int num_media_packets,
                                                      int num_fec_packets) {
  RTC_DCHECK_GT(num_media_packets, 0);
  RTC_DCHECK_GT(num_fec_packets, 0);
  RTC_DCHECK_LE(num_media_packets, kUlpfecMaxMediaPackets);
  RTC_DCHECK_LE(num_fec_packets, num_media_packets);

  if (num_media_packets <= 12) {
    return LookUpInFecTable(table_, num_media_packets - 1, num_fec_packets - 1);
  }
  int mask_length =
      static_cast<int>(PacketMaskSize(static_cast<size_t>(num_media_packets)));

  for (int row = 0; row < num_fec_packets; row++) {

    for (int col = 0; col < mask_length; col++) {
      fec_packet_mask_[row * mask_length + col] =
          ((col * 8) % num_fec_packets == row && (col * 8) < num_media_packets
               ? 0x80
               : 0x00) |
          ((col * 8 + 1) % num_fec_packets == row &&
                   (col * 8 + 1) < num_media_packets
               ? 0x40
               : 0x00) |
          ((col * 8 + 2) % num_fec_packets == row &&
                   (col * 8 + 2) < num_media_packets
               ? 0x20
               : 0x00) |
          ((col * 8 + 3) % num_fec_packets == row &&
                   (col * 8 + 3) < num_media_packets
               ? 0x10
               : 0x00) |
          ((col * 8 + 4) % num_fec_packets == row &&
                   (col * 8 + 4) < num_media_packets
               ? 0x08
               : 0x00) |
          ((col * 8 + 5) % num_fec_packets == row &&
                   (col * 8 + 5) < num_media_packets
               ? 0x04
               : 0x00) |
          ((col * 8 + 6) % num_fec_packets == row &&
                   (col * 8 + 6) < num_media_packets
               ? 0x02
               : 0x00) |
          ((col * 8 + 7) % num_fec_packets == row &&
                   (col * 8 + 7) < num_media_packets
               ? 0x01
               : 0x00);
    }
  }
  return {&fec_packet_mask_[0],
          static_cast<size_t>(num_fec_packets * mask_length)};
}

const uint8_t* PacketMaskTable::PickTable(FecMaskType fec_mask_type,
                                          int num_media_packets) {
  RTC_DCHECK_GE(num_media_packets, 0);
  RTC_DCHECK_LE(static_cast<size_t>(num_media_packets), kUlpfecMaxMediaPackets);

  if (fec_mask_type != kFecMaskRandom &&
      num_media_packets <=
          static_cast<int>(fec_private_tables::kPacketMaskBurstyTbl[0])) {
    return &fec_private_tables::kPacketMaskBurstyTbl[0];
  }

  return &fec_private_tables::kPacketMaskRandomTbl[0];
}

void RemainingPacketProtection(int num_media_packets,
                               int num_fec_remaining,
                               int num_fec_for_imp_packets,
                               int num_mask_bytes,
                               ProtectionMode mode,
                               uint8_t* packet_mask,
                               PacketMaskTable* mask_table) {
  if (mode == kModeNoOverlap) {

    const int res_mask_bytes =
        PacketMaskSize(num_media_packets - num_fec_for_imp_packets);

    auto end_row = (num_fec_for_imp_packets + num_fec_remaining);
    rtc::ArrayView<const uint8_t> packet_mask_sub_21 = mask_table->LookUp(
        num_media_packets - num_fec_for_imp_packets, num_fec_remaining);

    ShiftFitSubMask(num_mask_bytes, res_mask_bytes, num_fec_for_imp_packets,
                    end_row, &packet_mask_sub_21[0], packet_mask);

  } else if (mode == kModeOverlap || mode == kModeBiasFirstPacket) {

    rtc::ArrayView<const uint8_t> packet_mask_sub_22 =
        mask_table->LookUp(num_media_packets, num_fec_remaining);

    FitSubMask(num_mask_bytes, num_mask_bytes, num_fec_remaining,
               &packet_mask_sub_22[0],
               &packet_mask[num_fec_for_imp_packets * num_mask_bytes]);

    if (mode == kModeBiasFirstPacket) {
      for (int i = 0; i < num_fec_remaining; ++i) {
        int pkt_mask_idx = i * num_mask_bytes;
        packet_mask[pkt_mask_idx] = packet_mask[pkt_mask_idx] | (1 << 7);
      }
    }
  } else {
    RTC_DCHECK_NOTREACHED();
  }
}

void ImportantPacketProtection(int num_fec_for_imp_packets,
                               int num_imp_packets,
                               int num_mask_bytes,
                               uint8_t* packet_mask,
                               PacketMaskTable* mask_table) {
  const int num_imp_mask_bytes = PacketMaskSize(num_imp_packets);

  rtc::ArrayView<const uint8_t> packet_mask_sub_1 =
      mask_table->LookUp(num_imp_packets, num_fec_for_imp_packets);

  FitSubMask(num_mask_bytes, num_imp_mask_bytes, num_fec_for_imp_packets,
             &packet_mask_sub_1[0], packet_mask);
}

int SetProtectionAllocation(int num_media_packets,
                            int num_fec_packets,
                            int num_imp_packets) {

  float alloc_par = 0.5;
  int max_num_fec_for_imp = alloc_par * num_fec_packets;

  int num_fec_for_imp_packets = (num_imp_packets < max_num_fec_for_imp)
                                    ? num_imp_packets
                                    : max_num_fec_for_imp;

  if (num_fec_packets == 1 && (num_media_packets > 2 * num_imp_packets)) {
    num_fec_for_imp_packets = 0;
  }

  return num_fec_for_imp_packets;
}

void UnequalProtectionMask(int num_media_packets,
                           int num_fec_packets,
                           int num_imp_packets,
                           int num_mask_bytes,
                           uint8_t* packet_mask,
                           PacketMaskTable* mask_table) {

  ProtectionMode mode = kModeOverlap;
  int num_fec_for_imp_packets = 0;

  if (mode != kModeBiasFirstPacket) {
    num_fec_for_imp_packets = SetProtectionAllocation(
        num_media_packets, num_fec_packets, num_imp_packets);
  }

  int num_fec_remaining = num_fec_packets - num_fec_for_imp_packets;

  if (num_fec_for_imp_packets > 0) {
    ImportantPacketProtection(num_fec_for_imp_packets, num_imp_packets,
                              num_mask_bytes, packet_mask, mask_table);
  }

  if (num_fec_remaining > 0) {
    RemainingPacketProtection(num_media_packets, num_fec_remaining,
                              num_fec_for_imp_packets, num_mask_bytes, mode,
                              packet_mask, mask_table);
  }
}

rtc::ArrayView<const uint8_t> LookUpInFecTable(const uint8_t* table,
                                               int media_packet_index,
                                               int fec_index) {
  RTC_DCHECK_LT(media_packet_index, table[0]);

  const uint8_t* entry = &table[1];

  uint8_t entry_size_increment = 2;

  for (int i = 0; i < media_packet_index; ++i) {
    if (i == 16)
      entry_size_increment = 6;
    uint8_t count = entry[0];
    ++entry;
    for (int j = 0; j < count; ++j) {
      entry += entry_size_increment * (j + 1);
    }
  }

  if (media_packet_index == 16)
    entry_size_increment = 6;

  RTC_DCHECK_LT(fec_index, entry[0]);
  ++entry;

  for (int i = 0; i < fec_index; ++i)
    entry += entry_size_increment * (i + 1);

  size_t size = entry_size_increment * (fec_index + 1);
  return {&entry[0], size};
}

void GeneratePacketMasks(int num_media_packets,
                         int num_fec_packets,
                         int num_imp_packets,
                         bool use_unequal_protection,
                         PacketMaskTable* mask_table,
                         uint8_t* packet_mask) {
  RTC_DCHECK_GT(num_media_packets, 0);
  RTC_DCHECK_GT(num_fec_packets, 0);
  RTC_DCHECK_LE(num_fec_packets, num_media_packets);
  RTC_DCHECK_LE(num_imp_packets, num_media_packets);
  RTC_DCHECK_GE(num_imp_packets, 0);

  const int num_mask_bytes = PacketMaskSize(num_media_packets);

  if (!use_unequal_protection || num_imp_packets == 0) {

    rtc::ArrayView<const uint8_t> mask =
        mask_table->LookUp(num_media_packets, num_fec_packets);
    memcpy(packet_mask, &mask[0], mask.size());
  } else {
    UnequalProtectionMask(num_media_packets, num_fec_packets, num_imp_packets,
                          num_mask_bytes, packet_mask, mask_table);
  }
}

size_t PacketMaskSize(size_t num_sequence_numbers) {
  RTC_DCHECK_LE(num_sequence_numbers, 8 * kUlpfecPacketMaskSizeLBitSet);
  if (num_sequence_numbers > 8 * kUlpfecPacketMaskSizeLBitClear) {
    return kUlpfecPacketMaskSizeLBitSet;
  }
  return kUlpfecPacketMaskSizeLBitClear;
}

void InsertZeroColumns(int num_zeros,
                       uint8_t* new_mask,
                       int new_mask_bytes,
                       int num_fec_packets,
                       int new_bit_index) {
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    const int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    const int max_shifts = (7 - (new_bit_index % 8));
    new_mask[new_byte_index] <<= std::min(num_zeros, max_shifts);
  }
}

void CopyColumn(uint8_t* new_mask,
                int new_mask_bytes,
                uint8_t* old_mask,
                int old_mask_bytes,
                int num_fec_packets,
                int new_bit_index,
                int old_bit_index) {
  RTC_CHECK_LT(new_bit_index, 8 * new_mask_bytes);

  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    int old_byte_index = row * old_mask_bytes + old_bit_index / 8;
    new_mask[new_byte_index] |= ((old_mask[old_byte_index] & 0x80) >> 7);
    if (new_bit_index % 8 != 7) {
      new_mask[new_byte_index] <<= 1;
    }
    old_mask[old_byte_index] <<= 1;
  }
}

}
}
