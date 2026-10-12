/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NET_DCSCTP_PACKET_BOUNDED_BYTE_WRITER_H_
#define NET_DCSCTP_PACKET_BOUNDED_BYTE_WRITER_H_

#include <algorithm>

#include "api/array_view.h"

namespace dcsctp {

namespace internal {

inline void StoreBigEndian8(uint8_t* data, uint8_t val) {
  data[0] = val;
}

inline void StoreBigEndian16(uint8_t* data, uint16_t val) {
  data[0] = val >> 8;
  data[1] = val;
}

inline void StoreBigEndian32(uint8_t* data, uint32_t val) {
  data[0] = val >> 24;
  data[1] = val >> 16;
  data[2] = val >> 8;
  data[3] = val;
}
}

template <int FixedSize>
class BoundedByteWriter {
 public:
  explicit BoundedByteWriter(rtc::ArrayView<uint8_t> data) : data_(data) {
    RTC_CHECK(data.size() >= FixedSize);
  }

  template <size_t offset>
  void Store8(uint8_t value) {
    static_assert(offset + sizeof(uint8_t) <= FixedSize, "Out-of-bounds");
    internal::StoreBigEndian8(&data_[offset], value);
  }

  template <size_t offset>
  void Store16(uint16_t value) {
    static_assert(offset + sizeof(uint16_t) <= FixedSize, "Out-of-bounds");
    static_assert((offset % sizeof(uint16_t)) == 0, "Unaligned access");
    internal::StoreBigEndian16(&data_[offset], value);
  }

  template <size_t offset>
  void Store32(uint32_t value) {
    static_assert(offset + sizeof(uint32_t) <= FixedSize, "Out-of-bounds");
    static_assert((offset % sizeof(uint32_t)) == 0, "Unaligned access");
    internal::StoreBigEndian32(&data_[offset], value);
  }

  template <size_t SubSize>
  BoundedByteWriter<SubSize> sub_writer(size_t variable_offset) {
    RTC_CHECK(FixedSize + variable_offset + SubSize <= data_.size());

    return BoundedByteWriter<SubSize>(
        data_.subview(FixedSize + variable_offset, SubSize));
  }

  void CopyToVariableData(rtc::ArrayView<const uint8_t> source) {
    size_t copy_size = std::min(source.size(), data_.size() - FixedSize);
    if (source.data() == nullptr || copy_size == 0) {
      return;
    }
    memcpy(data_.data() + FixedSize, source.data(), copy_size);
  }

 private:
  rtc::ArrayView<uint8_t> data_;
};
}

#endif
