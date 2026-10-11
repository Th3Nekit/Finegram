/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {

class RtpPacket {
 public:
  using ExtensionType = RTPExtensionType;
  using ExtensionManager = RtpHeaderExtensionMap;

  RtpPacket();
  explicit RtpPacket(const ExtensionManager* extensions);
  RtpPacket(const ExtensionManager* extensions, size_t capacity);

  RtpPacket(const RtpPacket&);
  RtpPacket(RtpPacket&&);
  RtpPacket& operator=(const RtpPacket&);
  RtpPacket& operator=(RtpPacket&&);

  ~RtpPacket();

  bool Parse(const uint8_t* buffer, size_t size);
  bool Parse(rtc::ArrayView<const uint8_t> packet);

  bool Parse(rtc::CopyOnWriteBuffer packet);

  void IdentifyExtensions(ExtensionManager extensions);

  const ExtensionManager& extension_manager() const { return extensions_; }

  bool Marker() const { return marker_; }
  uint8_t PayloadType() const { return payload_type_; }
  uint16_t SequenceNumber() const { return sequence_number_; }
  uint32_t Timestamp() const { return timestamp_; }
  uint32_t Ssrc() const { return ssrc_; }
  std::vector<uint32_t> Csrcs() const;

  size_t headers_size() const { return payload_offset_; }

  size_t payload_size() const { return payload_size_; }
  bool has_padding() const { return buffer_[0] & 0x20; }
  size_t padding_size() const { return padding_size_; }
  rtc::ArrayView<const uint8_t> payload() const {
    return rtc::MakeArrayView(data() + payload_offset_, payload_size_);
  }
  rtc::CopyOnWriteBuffer PayloadBuffer() const {
    return buffer_.Slice(payload_offset_, payload_size_);
  }

  rtc::CopyOnWriteBuffer Buffer() const { return buffer_; }
  size_t capacity() const { return buffer_.capacity(); }
  size_t size() const {
    return payload_offset_ + payload_size_ + padding_size_;
  }
  const uint8_t* data() const { return buffer_.cdata(); }
  size_t FreeCapacity() const { return capacity() - size(); }
  size_t MaxPayloadSize() const { return capacity() - headers_size(); }

  void Clear();

  void CopyHeaderFrom(const RtpPacket& packet);
  void SetMarker(bool marker_bit);
  void SetPayloadType(uint8_t payload_type);
  void SetSequenceNumber(uint16_t seq_no);
  void SetTimestamp(uint32_t timestamp);
  void SetSsrc(uint32_t ssrc);

  void ZeroMutableExtensions();

  bool RemoveExtension(ExtensionType type);

  void SetCsrcs(rtc::ArrayView<const uint32_t> csrcs);

  template <typename Extension>
  bool HasExtension() const;
  bool HasExtension(ExtensionType type) const;

  template <typename Extension>
  bool IsRegistered() const;

  template <typename Extension, typename FirstValue, typename... Values>
  bool GetExtension(FirstValue&&, Values&&...) const;

  template <typename Extension>
  absl::optional<typename Extension::value_type> GetExtension() const;

  template <typename Extension>
  rtc::ArrayView<const uint8_t> GetRawExtension() const;

  template <typename Extension, typename... Values>
  bool SetExtension(const Values&...);

  template <typename Extension>
  bool SetRawExtension(rtc::ArrayView<const uint8_t> data);

  template <typename Extension>
  bool ReserveExtension();

  rtc::ArrayView<uint8_t> AllocateExtension(ExtensionType type, size_t length);

  rtc::ArrayView<const uint8_t> FindExtension(ExtensionType type) const;

  uint8_t* SetPayloadSize(size_t size_bytes);

  uint8_t* AllocatePayload(size_t size_bytes);

  bool SetPadding(size_t padding_size);

  std::string ToString() const;

 private:
  struct ExtensionInfo {
    explicit ExtensionInfo(uint8_t id) : ExtensionInfo(id, 0, 0) {}
    ExtensionInfo(uint8_t id, uint8_t length, uint16_t offset)
        : id(id), length(length), offset(offset) {}
    uint8_t id;
    uint8_t length;
    uint16_t offset;
  };

  bool ParseBuffer(const uint8_t* buffer, size_t size);

  const ExtensionInfo* FindExtensionInfo(int id) const;

  ExtensionInfo& FindOrCreateExtensionInfo(int id);

  rtc::ArrayView<uint8_t> AllocateRawExtension(int id, size_t length);

  void PromoteToTwoByteHeaderExtension();

  uint16_t SetExtensionLengthMaybeAddZeroPadding(size_t extensions_offset);

  uint8_t* WriteAt(size_t offset) { return buffer_.MutableData() + offset; }
  void WriteAt(size_t offset, uint8_t byte) {
    buffer_.MutableData()[offset] = byte;
  }
  const uint8_t* ReadAt(size_t offset) const { return buffer_.data() + offset; }

  bool marker_;
  uint8_t payload_type_;
  uint8_t padding_size_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  uint32_t ssrc_;
  size_t payload_offset_;
  size_t payload_size_;

  ExtensionManager extensions_;
  std::vector<ExtensionInfo> extension_entries_;
  size_t extensions_size_ = 0;
  rtc::CopyOnWriteBuffer buffer_;
};

template <typename Extension>
bool RtpPacket::HasExtension() const {
  return HasExtension(Extension::kId);
}

template <typename Extension>
bool RtpPacket::IsRegistered() const {
  return extensions_.IsRegistered(Extension::kId);
}

template <typename Extension, typename FirstValue, typename... Values>
bool RtpPacket::GetExtension(FirstValue&& first, Values&&... values) const {
  auto raw = FindExtension(Extension::kId);
  if (raw.empty())
    return false;
  return Extension::Parse(raw, std::forward<FirstValue>(first),
                          std::forward<Values>(values)...);
}

template <typename Extension>
absl::optional<typename Extension::value_type> RtpPacket::GetExtension() const {
  absl::optional<typename Extension::value_type> result;
  auto raw = FindExtension(Extension::kId);
  if (raw.empty() || !Extension::Parse(raw, &result.emplace()))
    result = absl::nullopt;
  return result;
}

template <typename Extension>
rtc::ArrayView<const uint8_t> RtpPacket::GetRawExtension() const {
  return FindExtension(Extension::kId);
}

template <typename Extension, typename... Values>
bool RtpPacket::SetExtension(const Values&... values) {
  const size_t value_size = Extension::ValueSize(values...);
  auto buffer = AllocateExtension(Extension::kId, value_size);
  if (buffer.empty())
    return false;
  return Extension::Write(buffer, values...);
}

template <typename Extension>
bool RtpPacket::SetRawExtension(rtc::ArrayView<const uint8_t> data) {
  rtc::ArrayView<uint8_t> buffer =
      AllocateExtension(Extension::kId, data.size());
  if (buffer.empty()) {
    return false;
  }
  std::memcpy(buffer.data(), data.data(), data.size());
  return true;
}

template <typename Extension>
bool RtpPacket::ReserveExtension() {
  auto buffer = AllocateExtension(Extension::kId, Extension::kValueSizeBytes);
  if (buffer.empty())
    return false;
  memset(buffer.data(), 0, Extension::kValueSizeBytes);
  return true;
}

}

#endif
