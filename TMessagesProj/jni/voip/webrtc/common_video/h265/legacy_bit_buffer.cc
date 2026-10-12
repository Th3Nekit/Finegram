/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h265/legacy_bit_buffer.h"

#include <algorithm>
#include <limits>

#include "rtc_base/checks.h"

namespace {

uint8_t LowestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  return byte & ((1 << bit_count) - 1);
}

uint8_t HighestBits(uint8_t byte, size_t bit_count) {
  RTC_DCHECK_LE(bit_count, 8);
  uint8_t shift = 8 - static_cast<uint8_t>(bit_count);
  uint8_t mask = 0xFF << shift;
  return (byte & mask) >> shift;
}

size_t CountBits(uint64_t val) {
  size_t bit_count = 0;
  while (val != 0) {
    bit_count++;
    val >>= 1;
  }
  return bit_count;
}

}

namespace rtc {

BitBuffer::BitBuffer(const uint8_t* bytes, size_t byte_count)
    : bytes_(bytes), byte_count_(byte_count), byte_offset_(), bit_offset_() {
  RTC_DCHECK(static_cast<uint64_t>(byte_count_) <=
             std::numeric_limits<uint32_t>::max());
}

uint64_t BitBuffer::RemainingBitCount() const {
  return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitBuffer::ReadUInt8(uint8_t* val) {
  uint32_t bit_val;
  if (!ReadBits(&bit_val, sizeof(uint8_t) * 8)) {
    return false;
  }
  RTC_DCHECK(bit_val <= std::numeric_limits<uint8_t>::max());
  *val = static_cast<uint8_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt16(uint16_t* val) {
  uint32_t bit_val;
  if (!ReadBits(&bit_val, sizeof(uint16_t) * 8)) {
    return false;
  }
  RTC_DCHECK(bit_val <= std::numeric_limits<uint16_t>::max());
  *val = static_cast<uint16_t>(bit_val);
  return true;
}

bool BitBuffer::ReadUInt32(uint32_t* val) {
  return ReadBits(val, sizeof(uint32_t) * 8);
}

bool BitBuffer::PeekBits(uint32_t* val, size_t bit_count) {

  RTC_DCHECK(bit_count > 0);
  if (!val || bit_count > RemainingBitCount() || bit_count > 32) {
    return false;
  }
  const uint8_t* bytes = bytes_ + byte_offset_;
  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  uint32_t bits = LowestBits(*bytes++, remaining_bits_in_current_byte);

  if (bit_count < remaining_bits_in_current_byte) {
    *val = HighestBits(bits, bit_offset_ + bit_count);
    return true;
  }

  bit_count -= remaining_bits_in_current_byte;
  while (bit_count >= 8) {
    bits = (bits << 8) | *bytes++;
    bit_count -= 8;
  }

  if (bit_count > 0) {
    bits <<= bit_count;
    bits |= HighestBits(*bytes, bit_count);
  }
  *val = bits;
  return true;
}

bool BitBuffer::ReadBits(uint32_t* val, size_t bit_count) {
  return PeekBits(val, bit_count) && ConsumeBits(bit_count);
}

bool BitBuffer::ConsumeBytes(size_t byte_count) {
  return ConsumeBits(byte_count * 8);
}

bool BitBuffer::ConsumeBits(size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }

  byte_offset_ += (bit_offset_ + bit_count) / 8;
  bit_offset_ = (bit_offset_ + bit_count) % 8;
  return true;
}

bool BitBuffer::ReadNonSymmetric(uint32_t* val, uint32_t num_values) {
  RTC_DCHECK_GT(num_values, 0);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  if (num_values == 1) {

    *val = 0;
    return true;
  }
  size_t count_bits = CountBits(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  if (!ReadBits(val, count_bits - 1)) {
    return false;
  }

  if (*val < num_min_bits_values) {
    return true;
  }

  uint32_t extra_bit;
  if (!ReadBits(&extra_bit,               1)) {
    return false;
  }

  *val = (*val << 1) + extra_bit - num_min_bits_values;
  return true;
}

bool BitBuffer::ReadExponentialGolomb(uint32_t* val) {
  if (!val) {
    return false;
  }

  size_t original_byte_offset = byte_offset_;
  size_t original_bit_offset = bit_offset_;

  size_t zero_bit_count = 0;
  uint32_t peeked_bit;
  while (PeekBits(&peeked_bit, 1) && peeked_bit == 0) {
    zero_bit_count++;
    ConsumeBits(1);
  }

  RTC_DCHECK(!PeekBits(&peeked_bit, 1) || peeked_bit == 1);

  size_t value_bit_count = zero_bit_count + 1;
  if (value_bit_count > 32 || !ReadBits(val, value_bit_count)) {
    RTC_CHECK(Seek(original_byte_offset, original_bit_offset));
    return false;
  }
  *val -= 1;
  return true;
}

bool BitBuffer::ReadSignedExponentialGolomb(int32_t* val) {
  uint32_t unsigned_val;
  if (!ReadExponentialGolomb(&unsigned_val)) {
    return false;
  }
  if ((unsigned_val & 1) == 0) {
    *val = -static_cast<int32_t>(unsigned_val / 2);
  } else {
    *val = (unsigned_val + 1) / 2;
  }
  return true;
}

void BitBuffer::GetCurrentOffset(size_t* out_byte_offset,
                                 size_t* out_bit_offset) {
  RTC_CHECK(out_byte_offset != nullptr);
  RTC_CHECK(out_bit_offset != nullptr);
  *out_byte_offset = byte_offset_;
  *out_bit_offset = bit_offset_;
}

bool BitBuffer::Seek(size_t byte_offset, size_t bit_offset) {
  if (byte_offset > byte_count_ || bit_offset > 7 ||
      (byte_offset == byte_count_ && bit_offset > 0)) {
    return false;
  }
  byte_offset_ = byte_offset;
  bit_offset_ = bit_offset;
  return true;
}

}
