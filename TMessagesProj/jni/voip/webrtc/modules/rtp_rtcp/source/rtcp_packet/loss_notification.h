/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_LOSS_NOTIFICATION_H_

#include "absl/base/attributes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/psfb.h"

namespace webrtc {
namespace rtcp {

class LossNotification : public Psfb {
 public:
  LossNotification();
  LossNotification(uint16_t last_decoded,
                   uint16_t last_received,
                   bool decodability_flag);
  LossNotification(const LossNotification& other);
  ~LossNotification() override;

  size_t BlockLength() const override;

  ABSL_MUST_USE_RESULT
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              PacketReadyCallback callback) const override;

  ABSL_MUST_USE_RESULT
  bool Parse(const CommonHeader& packet);

  ABSL_MUST_USE_RESULT
  bool Set(uint16_t last_decoded,
           uint16_t last_received,
           bool decodability_flag);

  uint16_t last_decoded() const { return last_decoded_; }

  uint16_t last_received() const { return last_received_; }

  bool decodability_flag() const { return decodability_flag_; }

 private:
  static constexpr uint32_t kUniqueIdentifier = 0x4C4E5446;
  static constexpr size_t kLossNotificationPayloadLength = 8;

  uint16_t last_decoded_;
  uint16_t last_received_;
  bool decodability_flag_;
};
}
}
#endif
