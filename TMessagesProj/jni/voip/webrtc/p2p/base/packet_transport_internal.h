/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PACKET_TRANSPORT_INTERNAL_H_
#define P2P_BASE_PACKET_TRANSPORT_INTERNAL_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "p2p/base/port.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {
struct PacketOptions;
struct SentPacket;

class RTC_EXPORT PacketTransportInternal : public sigslot::has_slots<> {
 public:
  virtual const std::string& transport_name() const = 0;

  virtual bool writable() const = 0;

  virtual bool receiving() const = 0;

  virtual int SendPacket(const char* data,
                         size_t len,
                         const rtc::PacketOptions& options,
                         int flags = 0) = 0;

  virtual int SetOption(rtc::Socket::Option opt, int value) = 0;

  virtual bool GetOption(rtc::Socket::Option opt, int* value);

  virtual int GetError() = 0;

  virtual absl::optional<NetworkRoute> network_route() const;

  sigslot::signal1<PacketTransportInternal*> SignalWritableState;

  sigslot::signal1<PacketTransportInternal*> SignalReadyToSend;

  sigslot::signal1<PacketTransportInternal*> SignalReceivingState;

  sigslot::signal5<PacketTransportInternal*,
                   const char*,
                   size_t,

                   const int64_t&,
                   int>
      SignalReadPacket;

  sigslot::signal2<PacketTransportInternal*, const rtc::SentPacket&>
      SignalSentPacket;

  sigslot::signal1<absl::optional<rtc::NetworkRoute>> SignalNetworkRouteChanged;

  sigslot::signal1<PacketTransportInternal*> SignalClosed;

 protected:
  PacketTransportInternal();
  ~PacketTransportInternal() override;
};

}

#endif
