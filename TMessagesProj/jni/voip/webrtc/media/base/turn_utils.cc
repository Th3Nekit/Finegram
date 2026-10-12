/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/turn_utils.h"

#include "api/transport/stun.h"
#include "rtc_base/byte_order.h"

namespace cricket {

namespace {

const size_t kTurnChannelHeaderLength = 4;

bool IsTurnChannelData(const uint8_t* data, size_t length) {
  return length >= kTurnChannelHeaderLength && ((*data & 0xC0) == 0x40);
}

bool IsTurnSendIndicationPacket(const uint8_t* data, size_t length) {
  if (length < kStunHeaderSize) {
    return false;
  }

  uint16_t type = rtc::GetBE16(data);
  return (type == TURN_SEND_INDICATION);
}

}

bool UnwrapTurnPacket(const uint8_t* packet,
                      size_t packet_size,
                      size_t* content_position,
                      size_t* content_size) {
  if (IsTurnChannelData(packet, packet_size)) {

    size_t length = rtc::GetBE16(&packet[2]);
    if (length + kTurnChannelHeaderLength > packet_size) {
      return false;
    }

    *content_position = kTurnChannelHeaderLength;
    *content_size = length;
    return true;
  }

  if (IsTurnSendIndicationPacket(packet, packet_size)) {

    const size_t stun_message_length = rtc::GetBE16(&packet[2]);
    if (stun_message_length + kStunHeaderSize != packet_size) {
      return false;
    }

    size_t pos = kStunHeaderSize;

    while (pos < packet_size) {

      uint16_t attr_type, attr_length;
      const int kAttrHeaderLength = sizeof(attr_type) + sizeof(attr_length);

      if (packet_size < pos + kAttrHeaderLength) {
        return false;
      }

      attr_type = rtc::GetBE16(&packet[pos]);
      attr_length = rtc::GetBE16(&packet[pos + sizeof(attr_type)]);

      pos += kAttrHeaderLength;

      if (pos + attr_length > packet_size) {
        return false;
      }

      if (attr_type == STUN_ATTR_DATA) {
        *content_position = pos;
        *content_size = attr_length;
        return true;
      }

      pos += attr_length;
      if ((attr_length % 4) != 0) {
        pos += (4 - (attr_length % 4));
      }
    }

    return false;
  }

  *content_position = 0;
  *content_size = packet_size;
  return true;
}

}
