/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_RTP_UTILS_H_
#define MEDIA_BASE_RTP_UTILS_H_

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {
struct PacketTimeUpdateParams;
}

namespace cricket {

const size_t kMinRtpPacketLen = 12;
const size_t kMaxRtpPacketLen = 2048;
const size_t kMinRtcpPacketLen = 4;

enum RtcpTypes {
  kRtcpTypeSR = 200,
  kRtcpTypeRR = 201,
  kRtcpTypeSDES = 202,
  kRtcpTypeBye = 203,
  kRtcpTypeApp = 204,
  kRtcpTypeRTPFB = 205,
  kRtcpTypePSFB = 206,
};

enum class RtpPacketType {
  kRtp,
  kRtcp,
  kUnknown,
};

bool GetRtcpType(const void* data, size_t len, int* value);
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value);

RtpPacketType InferRtpPacketType(rtc::ArrayView<const char> packet);

bool IsValidRtpPayloadType(int payload_type);

bool IsValidRtpPacketSize(RtpPacketType packet_type, size_t size);

absl::string_view RtpPacketTypeToString(RtpPacketType packet_type);

bool RTC_EXPORT ValidateRtpHeader(const uint8_t* rtp,
                                  size_t length,
                                  size_t* header_length);

bool UpdateRtpAbsSendTimeExtension(uint8_t* rtp,
                                   size_t length,
                                   int extension_id,
                                   uint64_t time_us);

bool RTC_EXPORT
ApplyPacketOptions(uint8_t* data,
                   size_t length,
                   const rtc::PacketTimeUpdateParams& packet_time_params,
                   uint64_t time_us);

}

#endif
