/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_UTILS_H_
#define PC_SCTP_UTILS_H_

#include <string>

#include "api/data_channel_interface.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "media/sctp/sctp_transport_internal.h"
#include "net/dcsctp/public/types.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace rtc {
class CopyOnWriteBuffer;
}

namespace webrtc {
struct DataChannelInit;

class StreamId {
 public:
  StreamId() = default;
  explicit StreamId(int id)
      : id_(id >= cricket::kMinSctpSid && id <= cricket::kSpecMaxSctpSid
                ? absl::optional<uint16_t>(static_cast<uint16_t>(id))
                : absl::nullopt) {}
  StreamId(const StreamId& sid) = default;
  StreamId& operator=(const StreamId& sid) = default;

  bool HasValue() const { return id_.has_value(); }

  int stream_id_int() const {
    return id_.has_value() ? static_cast<int>(id_.value().value()) : -1;
  }

  void reset() { id_ = absl::nullopt; }

  bool operator==(const StreamId& sid) const { return id_ == sid.id_; }
  bool operator<(const StreamId& sid) const { return id_ < sid.id_; }
  bool operator!=(const StreamId& sid) const { return !(operator==(sid)); }

 private:
  absl::optional<dcsctp::StreamID> id_;
};

bool IsOpenMessage(const rtc::CopyOnWriteBuffer& payload);

bool ParseDataChannelOpenMessage(const rtc::CopyOnWriteBuffer& payload,
                                 std::string* label,
                                 DataChannelInit* config);

bool ParseDataChannelOpenAckMessage(const rtc::CopyOnWriteBuffer& payload);

bool WriteDataChannelOpenMessage(const std::string& label,
                                 const std::string& protocol,
                                 absl::optional<Priority> priority,
                                 bool ordered,
                                 absl::optional<int> max_retransmits,
                                 absl::optional<int> max_retransmit_time,
                                 rtc::CopyOnWriteBuffer* payload);
bool WriteDataChannelOpenMessage(const std::string& label,
                                 const DataChannelInit& config,
                                 rtc::CopyOnWriteBuffer* payload);
void WriteDataChannelOpenAckMessage(rtc::CopyOnWriteBuffer* payload);

}

#endif
