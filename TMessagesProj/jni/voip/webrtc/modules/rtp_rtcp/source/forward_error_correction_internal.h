/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_
#define MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include "api/array_view.h"
#include "modules/include/module_fec_types.h"

namespace webrtc {

constexpr size_t kUlpfecMaxMediaPackets = 48;

constexpr size_t kUlpfecPacketMaskSizeLBitClear = 2;
constexpr size_t kUlpfecPacketMaskSizeLBitSet = 6;

constexpr size_t kFECPacketMaskMaxSize = 288;

constexpr size_t kUlpfecMinPacketMaskSize = kUlpfecPacketMaskSizeLBitClear;
constexpr size_t kUlpfecMaxPacketMaskSize = kUlpfecPacketMaskSizeLBitSet;

namespace internal {

class PacketMaskTable {
 public:
  PacketMaskTable(FecMaskType fec_mask_type, int num_media_packets);
  ~PacketMaskTable();

  rtc::ArrayView<const uint8_t> LookUp(int num_media_packets,
                                       int num_fec_packets);

 private:
  static const uint8_t* PickTable(FecMaskType fec_mask_type,
                                  int num_media_packets);
  const uint8_t* table_;
  uint8_t fec_packet_mask_[kFECPacketMaskMaxSize];
};

rtc::ArrayView<const uint8_t> LookUpInFecTable(const uint8_t* table,
                                               int media_packet_index,
                                               int fec_index);

void GeneratePacketMasks(int num_media_packets,
                         int num_fec_packets,
                         int num_imp_packets,
                         bool use_unequal_protection,
                         PacketMaskTable* mask_table,
                         uint8_t* packet_mask);

size_t PacketMaskSize(size_t num_sequence_numbers);

void InsertZeroColumns(int num_zeros,
                       uint8_t* new_mask,
                       int new_mask_bytes,
                       int num_fec_packets,
                       int new_bit_index);

void CopyColumn(uint8_t* new_mask,
                int new_mask_bytes,
                uint8_t* old_mask,
                int old_mask_bytes,
                int num_fec_packets,
                int new_bit_index,
                int old_bit_index);

}
}

#endif
