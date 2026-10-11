/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FLEXFEC_RECEIVE_STREAM_H_
#define CALL_FLEXFEC_RECEIVE_STREAM_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "api/call/transport.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "call/receive_stream.h"
#include "call/rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"

namespace webrtc {

class FlexfecReceiveStream : public RtpPacketSinkInterface,
                             public ReceiveStreamInterface {
 public:
  ~FlexfecReceiveStream() override = default;

  struct Config {
    explicit Config(Transport* rtcp_send_transport);
    Config(const Config&);
    ~Config();

    std::string ToString() const;

    bool IsCompleteAndEnabled() const;

    int payload_type = -1;

    ReceiveStreamRtpConfig rtp;

    std::vector<uint32_t> protected_media_ssrcs;

    RtcpMode rtcp_mode = RtcpMode::kCompound;

    Transport* rtcp_send_transport = nullptr;
  };

  virtual void SetRtcpMode(RtcpMode mode) = 0;

  virtual void SetPayloadType(int payload_type) = 0;
  virtual int payload_type() const = 0;

  virtual const ReceiveStatistics* GetStats() const = 0;
};

}

#endif
