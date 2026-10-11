/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BYTE_BUFFER_H_
#define RTC_BASE_BYTE_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_order.h"

namespace rtc {

template <class BufferClassT>
class ByteBufferWriterT {
  using value_type = typename BufferClassT::value_type;

 public:
  ByteBufferWriterT() { Construct(nullptr, kDefaultCapacity); }
  ByteBufferWriterT(const value_type* bytes, size_t len) {
    Construct(bytes, len);
  }

  ByteBufferWriterT(const ByteBufferWriterT&) = delete;
  ByteBufferWriterT& operator=(const ByteBufferWriterT&) = delete;

  const value_type* Data() const { return buffer_.data(); }
  size_t Length() const { return buffer_.size(); }
  size_t Capacity() const { return buffer_.capacity(); }
  rtc::ArrayView<const value_type> DataView() const {
    return rtc::MakeArrayView(Data(), Length());
  }

  absl::string_view DataAsStringView() const {
    return absl::string_view(reinterpret_cast<const char*>(Data()), Length());
  }
  const char* DataAsCharPointer() const {
    return reinterpret_cast<const char*>(Data());
  }

  void WriteUInt8(uint8_t val) {
    WriteBytesInternal(reinterpret_cast<const value_type*>(&val), 1);
  }
  void WriteUInt16(uint16_t val) {
    uint16_t v = HostToNetwork16(val);
    WriteBytesInternal(reinterpret_cast<const value_type*>(&v), 2);
  }
  void WriteUInt24(uint32_t val) {
    uint32_t v = HostToNetwork32(val);
    value_type* start = reinterpret_cast<value_type*>(&v);
    ++start;
    WriteBytesInternal(start, 3);
  }
  void WriteUInt32(uint32_t val) {
    uint32_t v = HostToNetwork32(val);
    WriteBytesInternal(reinterpret_cast<const value_type*>(&v), 4);
  }
  void WriteUInt64(uint64_t val) {
    uint64_t v = HostToNetwork64(val);
    WriteBytesInternal(reinterpret_cast<const value_type*>(&v), 8);
  }

  void WriteUVarint(uint64_t val) {
    while (val >= 0x80) {

      value_type byte = static_cast<value_type>(val) | 0x80;
      WriteBytesInternal(&byte, 1);
      val >>= 7;
    }
    value_type last_byte = static_cast<value_type>(val);
    WriteBytesInternal(&last_byte, 1);
  }
  void WriteString(absl::string_view val) {
    WriteBytesInternal(reinterpret_cast<const value_type*>(val.data()),
                       val.size());
  }

  void WriteBytes(const uint8_t* val, size_t len) {
    WriteBytesInternal(reinterpret_cast<const value_type*>(val), len);
  }

  value_type* ReserveWriteBuffer(size_t len) {
    buffer_.SetSize(buffer_.size() + len);
    return buffer_.data();
  }

  void Resize(size_t size) { buffer_.SetSize(size); }

  void Clear() { buffer_.Clear(); }

 private:
  static constexpr size_t kDefaultCapacity = 4096;

  void Construct(const value_type* bytes, size_t size) {
    if (bytes) {
      buffer_.AppendData(bytes, size);
    } else {
      buffer_.EnsureCapacity(size);
    }
  }

  void WriteBytesInternal(const value_type* val, size_t len) {
    buffer_.AppendData(val, len);
  }

  BufferClassT buffer_;

};

class ByteBufferWriter : public ByteBufferWriterT<BufferT<uint8_t>> {
 public:
  ByteBufferWriter();
  ByteBufferWriter(const uint8_t* bytes, size_t len);

  ByteBufferWriter(const ByteBufferWriter&) = delete;
  ByteBufferWriter& operator=(const ByteBufferWriter&) = delete;
};

class ByteBufferReader {
 public:
  explicit ByteBufferReader(
      rtc::ArrayView<const uint8_t> bytes ABSL_ATTRIBUTE_LIFETIME_BOUND);

  explicit ByteBufferReader(const ByteBufferWriter& buf);

  ByteBufferReader(const ByteBufferReader&) = delete;
  ByteBufferReader& operator=(const ByteBufferReader&) = delete;

  const uint8_t* Data() const { return bytes_ + start_; }

  size_t Length() const { return end_ - start_; }

  rtc::ArrayView<const uint8_t> DataView() const {
    return rtc::ArrayView<const uint8_t>(bytes_ + start_, end_ - start_);
  }

  bool ReadUInt8(uint8_t* val);
  bool ReadUInt16(uint16_t* val);
  bool ReadUInt24(uint32_t* val);
  bool ReadUInt32(uint32_t* val);
  bool ReadUInt64(uint64_t* val);
  bool ReadUVarint(uint64_t* val);

  bool ReadBytes(rtc::ArrayView<uint8_t> val);

  bool ReadString(std::string* val, size_t len);

  bool ReadStringView(absl::string_view* val, size_t len);

  bool Consume(size_t size);

 private:
  void Construct(const uint8_t* bytes, size_t size);
  bool ReadBytes(uint8_t* val, size_t len);

  const uint8_t* bytes_;
  size_t size_;
  size_t start_;
  size_t end_;
};

}

#endif
