/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_PACKET_H_
#define MODULES_AUDIO_CODING_NETEQ_PACKET_H_

#include <stdint.h>

#include <list>
#include <memory>

#include "api/audio_codecs/audio_decoder.h"
#include "api/neteq/tick_timer.h"
#include "api/rtp_packet_info.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"

namespace webrtc {

struct Packet {
  struct Priority {
    Priority() : codec_level(0), red_level(0) {}
    Priority(int codec_level, int red_level)
        : codec_level(codec_level), red_level(red_level) {
      CheckInvariant();
    }

    int codec_level;
    int red_level;

    bool operator<(const Priority& b) const {
      CheckInvariant();
      b.CheckInvariant();
      if (codec_level == b.codec_level)
        return red_level < b.red_level;

      return codec_level < b.codec_level;
    }
    bool operator==(const Priority& b) const {
      CheckInvariant();
      b.CheckInvariant();
      return codec_level == b.codec_level && red_level == b.red_level;
    }
    bool operator!=(const Priority& b) const { return !(*this == b); }
    bool operator>(const Priority& b) const { return b < *this; }
    bool operator<=(const Priority& b) const { return !(b > *this); }
    bool operator>=(const Priority& b) const { return !(b < *this); }

   private:
    void CheckInvariant() const {
      RTC_DCHECK_GE(codec_level, 0);
      RTC_DCHECK_GE(red_level, 0);
    }
  };

  uint32_t timestamp;
  uint16_t sequence_number;
  uint8_t payload_type;

  rtc::Buffer payload;
  Priority priority;
  absl::optional<RtpPacketInfo> packet_info;
  std::unique_ptr<TickTimer::Stopwatch> waiting_time;
  std::unique_ptr<AudioDecoder::EncodedAudioFrame> frame;

  Packet();
  Packet(Packet&& b);
  ~Packet();

  Packet Clone() const;

  Packet& operator=(Packet&& b);

  bool operator==(const Packet& rhs) const {
    return (this->timestamp == rhs.timestamp &&
            this->sequence_number == rhs.sequence_number &&
            this->priority == rhs.priority);
  }
  bool operator!=(const Packet& rhs) const { return !operator==(rhs); }
  bool operator<(const Packet& rhs) const {
    if (this->timestamp == rhs.timestamp) {
      if (this->sequence_number == rhs.sequence_number) {

        return this->priority < rhs.priority;
      }
      return (static_cast<uint16_t>(rhs.sequence_number -
                                    this->sequence_number) < 0xFFFF / 2);
    }
    return (static_cast<uint32_t>(rhs.timestamp - this->timestamp) <
            0xFFFFFFFF / 2);
  }
  bool operator>(const Packet& rhs) const { return rhs.operator<(*this); }
  bool operator<=(const Packet& rhs) const { return !operator>(rhs); }
  bool operator>=(const Packet& rhs) const { return !operator<(rhs); }

  bool empty() const { return !frame && payload.empty(); }
};

typedef std::list<Packet> PacketList;

}
#endif
