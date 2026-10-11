/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_RTP_STREAM_RECEIVER_CONTROLLER_INTERFACE_H_
#define CALL_RTP_STREAM_RECEIVER_CONTROLLER_INTERFACE_H_

#include <memory>

#include "call/rtp_packet_sink_interface.h"

namespace webrtc {

class RtpStreamReceiverInterface {
 public:
  virtual ~RtpStreamReceiverInterface() {}
};

class RtpStreamReceiverControllerInterface {
 public:
  virtual ~RtpStreamReceiverControllerInterface() {}

  virtual std::unique_ptr<RtpStreamReceiverInterface> CreateReceiver(
      uint32_t ssrc,
      RtpPacketSinkInterface* sink) = 0;
};

}

#endif
