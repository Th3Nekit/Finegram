/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_SCTP_TRANSPORT_FACTORY_INTERFACE_H_
#define API_TRANSPORT_SCTP_TRANSPORT_FACTORY_INTERFACE_H_

#include <memory>

namespace cricket {
class SctpTransportInternal;
}

namespace rtc {
class PacketTransportInternal;
}

namespace webrtc {

class SctpTransportFactoryInterface {
 public:
  virtual ~SctpTransportFactoryInterface() = default;

  virtual std::unique_ptr<cricket::SctpTransportInternal> CreateSctpTransport(
      rtc::PacketTransportInternal* channel) = 0;
};

}

#endif
