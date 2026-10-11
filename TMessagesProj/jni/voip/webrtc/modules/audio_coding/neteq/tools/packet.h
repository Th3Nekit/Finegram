/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_PACKET_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_PACKET_H_

#include <list>

#include "api/array_view.h"
#include "api/rtp_headers.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "rtc_base/copy_on_write_buffer.h"

namespace webrtc {
namespace test {

class Packet {
 public:

  Packet(rtc::CopyOnWriteBuffer packet,
         size_t virtual_packet_length_bytes,
         double time_ms,
         const RtpHeaderExtensionMap* extension_map = nullptr);

  Packet(rtc::CopyOnWriteBuffer packet,
         double time_ms,
         const RtpHeaderExtensionMap* extension_map = nullptr)
      : Packet(packet, packet.size(), time_ms, extension_map) {}

  Packet(const RTPHeader& header,
         size_t virtual_packet_length_bytes,
         size_t virtual_payload_length_bytes,
         double time_ms);

  virtual ~Packet();

  Packet(const Packet&) = delete;
  Packet& operator=(const Packet&) = delete;

  bool ExtractRedHeaders(std::list<RTPHeader*>* headers) const;

  static void DeleteRedHeaders(std::list<RTPHeader*>* headers);

  const uint8_t* payload() const { return rtp_payload_.data(); }

  size_t packet_length_bytes() const { return packet_.size(); }

  size_t payload_length_bytes() const { return rtp_payload_.size(); }

  size_t virtual_packet_length_bytes() const {
    return virtual_packet_length_bytes_;
  }

  size_t virtual_payload_length_bytes() const {
    return virtual_payload_length_bytes_;
  }

  const RTPHeader& header() const { return header_; }

  double time_ms() const { return time_ms_; }
  bool valid_header() const { return valid_header_; }

 private:
  bool ParseHeader(const RtpHeaderExtensionMap* extension_map);
  void CopyToHeader(RTPHeader* destination) const;

  RTPHeader header_;
  const rtc::CopyOnWriteBuffer packet_;
  rtc::ArrayView<const uint8_t> rtp_payload_;

  const size_t virtual_packet_length_bytes_;
  size_t virtual_payload_length_bytes_ = 0;
  const double time_ms_;
  const bool valid_header_;
};

}
}
#endif
