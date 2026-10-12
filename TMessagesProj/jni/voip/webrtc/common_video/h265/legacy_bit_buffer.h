/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_LEGACY_BIT_BUFFER_H_
#define COMMON_VIDEO_H265_LEGACY_BIT_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

namespace rtc {

class BitBuffer {
 public:
  BitBuffer(const uint8_t* bytes, size_t byte_count);

  BitBuffer(const BitBuffer&) = delete;
  BitBuffer& operator=(const BitBuffer&) = delete;

  void GetCurrentOffset(size_t* out_byte_offset, size_t* out_bit_offset);

  uint64_t RemainingBitCount() const;

  bool ReadUInt8(uint8_t* val);
  bool ReadUInt16(uint16_t* val);
  bool ReadUInt32(uint32_t* val);

  bool ReadBits(uint32_t* val, size_t bit_count);

  bool PeekBits(uint32_t* val, size_t bit_count);

  bool ReadNonSymmetric(uint32_t* val, uint32_t num_values);

  bool ReadExponentialGolomb(uint32_t* val);

  bool ReadSignedExponentialGolomb(int32_t* val);

  bool ConsumeBytes(size_t byte_count);

  bool ConsumeBits(size_t bit_count);

  bool Seek(size_t byte_offset, size_t bit_offset);

 protected:
  const uint8_t* const bytes_;

  size_t byte_count_;

  size_t byte_offset_;

  size_t bit_offset_;
};

}

#endif
