/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_SCTP_SCTP_TRANSPORT_INTERNAL_H_
#define MEDIA_SCTP_SCTP_TRANSPORT_INTERNAL_H_

#include <memory>
#include <string>
#include <vector>

#include "api/rtc_error.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "p2p/base/packet_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/thread.h"

namespace cricket {

constexpr int kSctpSendBufferSize = 256 * 1024;

constexpr uint16_t kMaxSctpStreams = 1024;
constexpr uint16_t kMaxSctpSid = kMaxSctpStreams - 1;
constexpr uint16_t kMinSctpSid = 0;

constexpr uint16_t kSpecMaxSctpSid = 65535;

const int kSctpDefaultPort = 5000;

enum class SctpErrorCauseCode : uint16_t {
  kInvalidStreamIdentifier = 1,
  kMissingMandatoryParameter = 2,
  kStaleCookieError = 3,
  kOutOfResource = 4,
  kUnresolvableAddress = 5,
  kUnrecognizedChunkType = 6,
  kInvalidMandatoryParameter = 7,
  kUnrecognizedParameters = 8,
  kNoUserData = 9,
  kCookieReceivedWhileShuttingDown = 10,
  kRestartWithNewAddresses = 11,
  kUserInitiatedAbort = 12,
  kProtocolViolation = 13,
};

class SctpTransportInternal {
 public:
  virtual ~SctpTransportInternal() {}

  virtual void SetOnConnectedCallback(std::function<void()> callback) = 0;
  virtual void SetDataChannelSink(webrtc::DataChannelSink* sink) = 0;

  virtual void SetDtlsTransport(rtc::PacketTransportInternal* transport) = 0;

  virtual bool Start(int local_sctp_port,
                     int remote_sctp_port,
                     int max_message_size) = 0;

  virtual bool OpenStream(int sid) = 0;

  virtual bool ResetStream(int sid) = 0;

  virtual webrtc::RTCError SendData(int sid,
                                    const webrtc::SendDataParams& params,
                                    const rtc::CopyOnWriteBuffer& payload) = 0;

  virtual bool ReadyToSendData() = 0;

  virtual int max_message_size() const = 0;

  virtual absl::optional<int> max_outbound_streams() const = 0;

  virtual absl::optional<int> max_inbound_streams() const = 0;

  virtual void set_debug_name_for_testing(const char* debug_name) = 0;
};

}

#endif
