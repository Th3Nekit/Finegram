/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/rtp_utils.h"

#include <string.h>

#include <vector>

#include "media/base/turn_utils.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/message_digest.h"

namespace cricket {

static const size_t kRtcpPayloadTypeOffset = 1;
static const size_t kRtpExtensionHeaderLen = 4;
static const size_t kAbsSendTimeExtensionLen = 3;
static const size_t kOneByteExtensionHeaderLen = 1;
static const size_t kTwoByteExtensionHeaderLen = 2;

namespace {

static const uint8_t kFakeAuthTag[10] = {0xba, 0xdd, 0xba, 0xdd, 0xba,
                                         0xdd, 0xba, 0xdd, 0xba, 0xdd};

void UpdateAbsSendTimeExtensionValue(uint8_t* extension_data,
                                     size_t length,
                                     uint64_t time_us) {

  if (length != kAbsSendTimeExtensionLen) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  uint32_t send_time = ((time_us << 18) / 1000000) & 0x00FFFFFF;
  extension_data[0] = static_cast<uint8_t>(send_time >> 16);
  extension_data[1] = static_cast<uint8_t>(send_time >> 8);
  extension_data[2] = static_cast<uint8_t>(send_time);
}

void UpdateRtpAuthTag(uint8_t* rtp,
                      size_t length,
                      const rtc::PacketTimeUpdateParams& packet_time_params) {

  if (packet_time_params.srtp_auth_key.empty()) {
    return;
  }

  size_t tag_length = packet_time_params.srtp_auth_tag_len;

  const size_t kRocLength = 4;
  if (tag_length < kRocLength || tag_length > length) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  uint8_t* auth_tag = rtp + (length - tag_length);

  RTC_DCHECK_EQ(0, memcmp(auth_tag, kFakeAuthTag, tag_length));

  memcpy(auth_tag, &packet_time_params.srtp_packet_index, kRocLength);

  size_t auth_required_length = length - tag_length + kRocLength;

  uint8_t output[64];
  size_t result =
      rtc::ComputeHmac(rtc::DIGEST_SHA_1, &packet_time_params.srtp_auth_key[0],
                       packet_time_params.srtp_auth_key.size(), rtp,
                       auth_required_length, output, sizeof(output));

  if (result < tag_length) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  memcpy(auth_tag, output, tag_length);
}

bool GetUint8(const void* data, size_t offset, int* value) {
  if (!data || !value) {
    return false;
  }
  *value = *(static_cast<const uint8_t*>(data) + offset);
  return true;
}

}

bool GetRtcpType(const void* data, size_t len, int* value) {
  if (len < kMinRtcpPacketLen) {
    return false;
  }
  return GetUint8(data, kRtcpPayloadTypeOffset, value);
}

bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value) {

  if (!data || len < kMinRtcpPacketLen + 4 || !value)
    return false;
  int pl_type;
  if (!GetRtcpType(data, len, &pl_type))
    return false;

  if (pl_type == kRtcpTypeSDES)
    return false;
  *value = rtc::GetBE32(static_cast<const uint8_t*>(data) + 4);
  return true;
}

bool IsValidRtpPayloadType(int payload_type) {
  return payload_type >= 0 && payload_type <= 127;
}

bool IsValidRtpPacketSize(RtpPacketType packet_type, size_t size) {
  RTC_DCHECK_NE(RtpPacketType::kUnknown, packet_type);
  size_t min_packet_length = packet_type == RtpPacketType::kRtcp
                                 ? kMinRtcpPacketLen
                                 : kMinRtpPacketLen;
  return size >= min_packet_length && size <= kMaxRtpPacketLen;
}

absl::string_view RtpPacketTypeToString(RtpPacketType packet_type) {
  switch (packet_type) {
    case RtpPacketType::kRtp:
      return "RTP";
    case RtpPacketType::kRtcp:
      return "RTCP";
    case RtpPacketType::kUnknown:
      return "Unknown";
  }
  RTC_CHECK_NOTREACHED();
}

RtpPacketType InferRtpPacketType(rtc::ArrayView<const char> packet) {
  if (webrtc::IsRtcpPacket(
          rtc::reinterpret_array_view<const uint8_t>(packet))) {
    return RtpPacketType::kRtcp;
  }
  if (webrtc::IsRtpPacket(rtc::reinterpret_array_view<const uint8_t>(packet))) {
    return RtpPacketType::kRtp;
  }
  return RtpPacketType::kUnknown;
}

bool ValidateRtpHeader(const uint8_t* rtp,
                       size_t length,
                       size_t* header_length) {
  if (header_length) {
    *header_length = 0;
  }

  if (length < kMinRtpPacketLen) {
    return false;
  }

  size_t cc_count = rtp[0] & 0x0F;
  size_t header_length_without_extension = kMinRtpPacketLen + 4 * cc_count;
  if (header_length_without_extension > length) {
    return false;
  }

  if (!(rtp[0] & 0x10)) {
    if (header_length)
      *header_length = header_length_without_extension;

    return true;
  }

  rtp += header_length_without_extension;

  if (header_length_without_extension + kRtpExtensionHeaderLen > length) {
    return false;
  }

  uint16_t extension_length_in_32bits = rtc::GetBE16(rtp + 2);
  size_t extension_length = extension_length_in_32bits * 4;

  size_t rtp_header_length = extension_length +
                             header_length_without_extension +
                             kRtpExtensionHeaderLen;

  if (rtp_header_length > length) {
    return false;
  }

  if (header_length) {
    *header_length = rtp_header_length;
  }
  return true;
}

bool UpdateRtpAbsSendTimeExtension(uint8_t* rtp,
                                   size_t length,
                                   int extension_id,
                                   uint64_t time_us) {

  if (!(rtp[0] & 0x10)) {
    return true;
  }

  size_t cc_count = rtp[0] & 0x0F;
  size_t header_length_without_extension = kMinRtpPacketLen + 4 * cc_count;

  rtp += header_length_without_extension;

  uint16_t profile_id = rtc::GetBE16(rtp);

  uint16_t extension_length_in_32bits = rtc::GetBE16(rtp + 2);
  size_t extension_length = extension_length_in_32bits * 4;

  rtp += kRtpExtensionHeaderLen;

  constexpr uint16_t kOneByteExtensionProfileId = 0xBEDE;
  constexpr uint16_t kTwoByteExtensionProfileId = 0x1000;

  bool found = false;
  if (profile_id == kOneByteExtensionProfileId ||
      profile_id == kTwoByteExtensionProfileId) {

    size_t extension_header_length = profile_id == kOneByteExtensionProfileId
                                         ? kOneByteExtensionHeaderLen
                                         : kTwoByteExtensionHeaderLen;

    const uint8_t* extension_start = rtp;
    const uint8_t* extension_end = extension_start + extension_length;

    while (rtp + 1 < extension_end) {

      const int id =
          profile_id == kOneByteExtensionProfileId ? (*rtp & 0xF0) >> 4 : *rtp;
      const size_t length = profile_id == kOneByteExtensionProfileId
                                ? (*rtp & 0x0F) + 1
                                : *(rtp + 1);
      if (rtp + extension_header_length + length > extension_end) {
        return false;
      }
      if (id == extension_id) {
        UpdateAbsSendTimeExtensionValue(rtp + extension_header_length, length,
                                        time_us);
        found = true;
        break;
      }
      rtp += extension_header_length + length;

      while ((rtp < extension_end) && (*rtp == 0)) {
        ++rtp;
      }
    }
  }
  return found;
}

bool ApplyPacketOptions(uint8_t* data,
                        size_t length,
                        const rtc::PacketTimeUpdateParams& packet_time_params,
                        uint64_t time_us) {
  RTC_DCHECK(data);
  RTC_DCHECK(length);

  if (packet_time_params.rtp_sendtime_extension_id == -1 &&
      packet_time_params.srtp_auth_key.empty()) {
    return true;
  }

  size_t rtp_start_pos;
  size_t rtp_length;
  if (!UnwrapTurnPacket(data, length, &rtp_start_pos, &rtp_length)) {
    RTC_DCHECK_NOTREACHED();
    return false;
  }

  auto packet = rtc::MakeArrayView(data + rtp_start_pos, rtp_length);
  if (!webrtc::IsRtpPacket(packet) ||
      !ValidateRtpHeader(data + rtp_start_pos, rtp_length, nullptr)) {
    RTC_DCHECK_NOTREACHED();
    return false;
  }

  uint8_t* start = data + rtp_start_pos;

  if (packet_time_params.rtp_sendtime_extension_id != -1) {
    UpdateRtpAbsSendTimeExtension(start, rtp_length,
                                  packet_time_params.rtp_sendtime_extension_id,
                                  time_us);
  }

  UpdateRtpAuthTag(start, rtp_length, packet_time_params);
  return true;
}

}
