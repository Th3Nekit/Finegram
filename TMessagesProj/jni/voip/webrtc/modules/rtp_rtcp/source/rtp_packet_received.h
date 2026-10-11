/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_RECEIVED_H_

#include <stdint.h>

#include <utility>

#include "api/array_view.h"
#include "api/ref_counted_base.h"
#include "api/rtp_headers.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

class RtpPacketReceived : public RtpPacket {
 public:
  RtpPacketReceived();
  explicit RtpPacketReceived(
      const ExtensionManager* extensions,
      webrtc::Timestamp arrival_time = webrtc::Timestamp::MinusInfinity());
  RtpPacketReceived(const RtpPacketReceived& packet);
  RtpPacketReceived(RtpPacketReceived&& packet);

  RtpPacketReceived& operator=(const RtpPacketReceived& packet);
  RtpPacketReceived& operator=(RtpPacketReceived&& packet);

  ~RtpPacketReceived();

  void GetHeader(RTPHeader* header) const;

  webrtc::Timestamp arrival_time() const { return arrival_time_; }
  void set_arrival_time(webrtc::Timestamp time) { arrival_time_ = time; }

  bool recovered() const { return recovered_; }
  void set_recovered(bool value) { recovered_ = value; }

  int payload_type_frequency() const { return payload_type_frequency_; }
  void set_payload_type_frequency(int value) {
    payload_type_frequency_ = value;
  }

  rtc::scoped_refptr<rtc::RefCountedBase> additional_data() const {
    return additional_data_;
  }
  void set_additional_data(rtc::scoped_refptr<rtc::RefCountedBase> data) {
    additional_data_ = std::move(data);
  }

 private:
  webrtc::Timestamp arrival_time_ = Timestamp::MinusInfinity();
  int payload_type_frequency_ = 0;
  bool recovered_ = false;
  rtc::scoped_refptr<rtc::RefCountedBase> additional_data_;
};

}
#endif
