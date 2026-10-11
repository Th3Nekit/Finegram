/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BIT_BUFFER_H_
#define RTC_BASE_BIT_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/strings/string_view.h"
#include "api/units/data_size.h"

namespace rtc {

class BitBufferWriter {
 public:
  static constexpr webrtc::DataSize kMaxLeb128Length =
      webrtc::DataSize::Bytes(10);

  BitBufferWriter(uint8_t* bytes, size_t byte_count);

  BitBufferWriter(const BitBufferWriter&) = delete;
  BitBufferWriter& operator=(const BitBufferWriter&) = delete;

  void GetCurrentOffset(size_t* out_byte_offset, size_t* out_bit_offset);

  uint64_t RemainingBitCount() const;

  bool ConsumeBytes(size_t byte_count);

  bool ConsumeBits(size_t bit_count);

  bool Seek(size_t byte_offset, size_t bit_offset);

  bool WriteUInt8(uint8_t val);
  bool WriteUInt16(uint16_t val);
  bool WriteUInt32(uint32_t val);

  bool WriteBits(uint64_t val, size_t bit_count);

  bool WriteNonSymmetric(uint32_t val, uint32_t num_values);

  static size_t SizeNonSymmetricBits(uint32_t val, uint32_t num_values);

  bool WriteExponentialGolomb(uint32_t val);

  bool WriteSignedExponentialGolomb(int32_t val);

  bool WriteLeb128(uint64_t val);

  bool WriteString(absl::string_view data);

 private:

  uint8_t* const writable_bytes_;

  const size_t byte_count_;

  size_t byte_offset_;

  size_t bit_offset_;
};

}

#endif
