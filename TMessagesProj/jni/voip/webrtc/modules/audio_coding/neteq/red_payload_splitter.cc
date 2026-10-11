/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/red_payload_splitter.h"

#include <stddef.h>

#include <cstdint>
#include <list>
#include <utility>
#include <vector>

#include "modules/audio_coding/neteq/decoder_database.h"
#include "modules/audio_coding/neteq/packet.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

bool RedPayloadSplitter::SplitRed(PacketList* packet_list) {

  const size_t kMaxRedBlocks = 32;
  bool ret = true;
  PacketList::iterator it = packet_list->begin();
  while (it != packet_list->end()) {
    Packet& red_packet = *it;
    RTC_DCHECK(!red_packet.payload.empty());
    const uint8_t* payload_ptr = red_packet.payload.data();
    size_t payload_length = red_packet.payload.size();

    struct RedHeader {
      uint8_t payload_type;
      uint32_t timestamp;
      size_t payload_length;
    };

    std::vector<RedHeader> new_headers;
    bool last_block = false;
    size_t sum_length = 0;
    while (!last_block) {
      if (payload_length == 0) {
        RTC_LOG(LS_WARNING) << "SplitRed header too short";
        return false;
      }
      RedHeader new_header;

      last_block = ((*payload_ptr & 0x80) == 0);

      new_header.payload_type = payload_ptr[0] & 0x7F;
      if (last_block) {

        sum_length += kRedLastHeaderLength;
        new_header.timestamp = red_packet.timestamp;
        new_header.payload_length = red_packet.payload.size() - sum_length;
        payload_ptr += kRedLastHeaderLength;
        payload_length -= kRedLastHeaderLength;
      } else {
        if (payload_length < kRedHeaderLength) {
          RTC_LOG(LS_WARNING) << "SplitRed header too short";
          return false;
        }

        int timestamp_offset =
            (payload_ptr[1] << 6) + ((payload_ptr[2] & 0xFC) >> 2);
        new_header.timestamp = red_packet.timestamp - timestamp_offset;

        new_header.payload_length =
            ((payload_ptr[2] & 0x03) << 8) + payload_ptr[3];

        sum_length += new_header.payload_length;
        sum_length += kRedHeaderLength;

        payload_ptr += kRedHeaderLength;
        payload_length -= kRedHeaderLength;
      }

      if (new_header.payload_length > 0) {
        new_headers.push_back(new_header);
      }
    }

    if (new_headers.size() <= kMaxRedBlocks) {

      PacketList new_packets;
      for (size_t i = 0; i != new_headers.size(); ++i) {
        const auto& new_header = new_headers[i];
        size_t payload_length = new_header.payload_length;
        if (payload_ptr + payload_length >
            red_packet.payload.data() + red_packet.payload.size()) {

          RTC_LOG(LS_WARNING) << "SplitRed length mismatch";
          ret = false;
          break;
        }

        Packet new_packet;
        new_packet.timestamp = new_header.timestamp;
        new_packet.payload_type = new_header.payload_type;
        new_packet.sequence_number = red_packet.sequence_number;
        new_packet.priority.red_level =
            rtc::dchecked_cast<int>((new_headers.size() - 1) - i);
        new_packet.payload.SetData(payload_ptr, payload_length);
        new_packets.push_front(std::move(new_packet));
        payload_ptr += payload_length;
      }

      packet_list->splice(it, std::move(new_packets));
    } else {
      RTC_LOG(LS_WARNING) << "SplitRed too many blocks: " << new_headers.size();
      ret = false;
    }

    it = packet_list->erase(it);
  }
  return ret;
}

void RedPayloadSplitter::CheckRedPayloads(
    PacketList* packet_list,
    const DecoderDatabase& decoder_database) {
  int main_payload_type = -1;
  for (auto it = packet_list->begin(); it != packet_list->end();      ) {
    uint8_t this_payload_type = it->payload_type;
    if (decoder_database.IsRed(this_payload_type)) {
      it = packet_list->erase(it);
      continue;
    }
    if (!decoder_database.IsDtmf(this_payload_type) &&
        !decoder_database.IsComfortNoise(this_payload_type)) {
      if (main_payload_type == -1) {

        main_payload_type = this_payload_type;
      } else {
        if (this_payload_type != main_payload_type) {

          it = packet_list->erase(it);
          continue;
        }
      }
    }
    ++it;
  }
}

}
