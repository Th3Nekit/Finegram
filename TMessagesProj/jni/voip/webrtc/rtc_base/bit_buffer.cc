/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bit_buffer.h"

#include <algorithm>
#include <limits>

#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"

namespace {

uint8_t HighestByte(uint64_t val) {
  return static_cast<uint8_t>(val >> 56);
}

uint8_t WritePartialByte(uint8_t source,
                         size_t source_bit_count,
                         uint8_t target,
                         size_t target_bit_offset) {
  RTC_DCHECK(target_bit_offset < 8);
  RTC_DCHECK(source_bit_count < 9);
  RTC_DCHECK(source_bit_count <= (8 - target_bit_offset));

  uint8_t mask =

      static_cast<uint8_t>(0xFF << (8 - source_bit_count))

      >> target_bit_offset;

  return (target & ~mask) | (source >> target_bit_offset);
}

}

namespace rtc {

BitBufferWriter::BitBufferWriter(uint8_t* bytes, size_t byte_count)
    : writable_bytes_(bytes),
      byte_count_(byte_count),
      byte_offset_(),
      bit_offset_() {
  RTC_DCHECK(static_cast<uint64_t>(byte_count_) <=
             std::numeric_limits<uint32_t>::max());
}

uint64_t BitBufferWriter::RemainingBitCount() const {
  return (static_cast<uint64_t>(byte_count_) - byte_offset_) * 8 - bit_offset_;
}

bool BitBufferWriter::ConsumeBytes(size_t byte_count) {
  return ConsumeBits(byte_count * 8);
}

bool BitBufferWriter::ConsumeBits(size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }

  byte_offset_ += (bit_offset_ + bit_count) / 8;
  bit_offset_ = (bit_offset_ + bit_count) % 8;
  return true;
}

void BitBufferWriter::GetCurrentOffset(size_t* out_byte_offset,
                                       size_t* out_bit_offset) {
  RTC_CHECK(out_byte_offset != nullptr);
  RTC_CHECK(out_bit_offset != nullptr);
  *out_byte_offset = byte_offset_;
  *out_bit_offset = bit_offset_;
}

bool BitBufferWriter::Seek(size_t byte_offset, size_t bit_offset) {
  if (byte_offset > byte_count_ || bit_offset > 7 ||
      (byte_offset == byte_count_ && bit_offset > 0)) {
    return false;
  }
  byte_offset_ = byte_offset;
  bit_offset_ = bit_offset;
  return true;
}

bool BitBufferWriter::WriteUInt8(uint8_t val) {
  return WriteBits(val, sizeof(uint8_t) * 8);
}

bool BitBufferWriter::WriteUInt16(uint16_t val) {
  return WriteBits(val, sizeof(uint16_t) * 8);
}

bool BitBufferWriter::WriteUInt32(uint32_t val) {
  return WriteBits(val, sizeof(uint32_t) * 8);
}

bool BitBufferWriter::WriteBits(uint64_t val, size_t bit_count) {
  if (bit_count > RemainingBitCount()) {
    return false;
  }
  size_t total_bits = bit_count;

  val <<= (sizeof(uint64_t) * 8 - bit_count);

  uint8_t* bytes = writable_bytes_ + byte_offset_;

  size_t remaining_bits_in_current_byte = 8 - bit_offset_;
  size_t bits_in_first_byte =
      std::min(bit_count, remaining_bits_in_current_byte);
  *bytes = WritePartialByte(HighestByte(val), bits_in_first_byte, *bytes,
                            bit_offset_);
  if (bit_count <= remaining_bits_in_current_byte) {

    return ConsumeBits(total_bits);
  }

  val <<= bits_in_first_byte;
  bytes++;
  bit_count -= bits_in_first_byte;
  while (bit_count >= 8) {
    *bytes++ = HighestByte(val);
    val <<= 8;
    bit_count -= 8;
  }

  if (bit_count > 0) {
    *bytes = WritePartialByte(HighestByte(val), bit_count, *bytes, 0);
  }

  return ConsumeBits(total_bits);
}

bool BitBufferWriter::WriteNonSymmetric(uint32_t val, uint32_t num_values) {
  RTC_DCHECK_LT(val, num_values);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  if (num_values == 1) {

    return true;
  }
  size_t count_bits = absl::bit_width(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  return val < num_min_bits_values
             ? WriteBits(val, count_bits - 1)
             : WriteBits(val + num_min_bits_values, count_bits);
}

size_t BitBufferWriter::SizeNonSymmetricBits(uint32_t val,
                                             uint32_t num_values) {
  RTC_DCHECK_LT(val, num_values);
  RTC_DCHECK_LE(num_values, uint32_t{1} << 31);
  size_t count_bits = absl::bit_width(num_values);
  uint32_t num_min_bits_values = (uint32_t{1} << count_bits) - num_values;

  return val < num_min_bits_values ? (count_bits - 1) : count_bits;
}

bool BitBufferWriter::WriteExponentialGolomb(uint32_t val) {

  if (val == std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  uint64_t val_to_encode = static_cast<uint64_t>(val) + 1;

  return WriteBits(val_to_encode, absl::bit_width(val_to_encode) * 2 - 1);
}

bool BitBufferWriter::WriteSignedExponentialGolomb(int32_t val) {
  if (val == 0) {
    return WriteExponentialGolomb(0);
  } else if (val > 0) {
    uint32_t signed_val = val;
    return WriteExponentialGolomb((signed_val * 2) - 1);
  } else {
    if (val == std::numeric_limits<int32_t>::min())
      return false;
    uint32_t signed_val = -val;
    return WriteExponentialGolomb(signed_val * 2);
  }
}

bool BitBufferWriter::WriteLeb128(uint64_t val) {
  bool success = true;
  do {
    uint8_t byte = static_cast<uint8_t>(val & 0x7f);
    val >>= 7;
    if (val > 0) {
      byte |= 0x80;
    }
    success &= WriteUInt8(byte);
  } while (val > 0);
  return success;
}

bool BitBufferWriter::WriteString(absl::string_view data) {
  bool success = true;
  for (char c : data) {
    success &= WriteUInt8(c);
  }
  return success;
}

}
