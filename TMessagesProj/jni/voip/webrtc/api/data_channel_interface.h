/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for DataChannels
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcdatachannel

#ifndef API_DATA_CHANNEL_INTERFACE_H_
#define API_DATA_CHANNEL_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "api/priority.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct DataChannelInit {

  bool reliable = false;

  bool ordered = true;

  absl::optional<int> maxRetransmitTime;

  absl::optional<int> maxRetransmits;

  std::string protocol;

  bool negotiated = false;

  int id = -1;

  absl::optional<Priority> priority;
};

struct DataBuffer {
  DataBuffer(const rtc::CopyOnWriteBuffer& data, bool binary)
      : data(data), binary(binary) {}

  explicit DataBuffer(const std::string& text)
      : data(text.data(), text.length()), binary(false) {}
  size_t size() const { return data.size(); }

  rtc::CopyOnWriteBuffer data;

  bool binary;
};

class DataChannelObserver {
 public:

  virtual void OnStateChange() = 0;

  virtual void OnMessage(const DataBuffer& buffer) = 0;

  virtual void OnBufferedAmountChange(uint64_t sent_data_size) {}

  virtual bool IsOkToCallOnTheNetworkThread() { return false; }

 protected:
  virtual ~DataChannelObserver() = default;
};

class RTC_EXPORT DataChannelInterface : public RefCountInterface {
 public:

  enum DataState {
    kConnecting,
    kOpen,
    kClosing,
    kClosed
  };

  static const char* DataStateString(DataState state) {
    switch (state) {
      case kConnecting:
        return "connecting";
      case kOpen:
        return "open";
      case kClosing:
        return "closing";
      case kClosed:
        return "closed";
    }
    RTC_CHECK(false) << "Unknown DataChannel state: " << state;
    return "";
  }

  virtual void RegisterObserver(DataChannelObserver* observer) = 0;
  virtual void UnregisterObserver() = 0;

  virtual std::string label() const = 0;

  virtual bool reliable() const = 0;

  virtual bool ordered() const;

  virtual uint16_t maxRetransmitTime() const;
  virtual uint16_t maxRetransmits() const;
  virtual absl::optional<int> maxRetransmitsOpt() const;
  virtual absl::optional<int> maxPacketLifeTime() const;
  virtual std::string protocol() const;
  virtual bool negotiated() const;

  virtual int id() const = 0;
  virtual Priority priority() const { return Priority::kLow; }
  virtual DataState state() const = 0;

  virtual RTCError error() const { return RTCError(); }
  virtual uint32_t messages_sent() const = 0;
  virtual uint64_t bytes_sent() const = 0;
  virtual uint32_t messages_received() const = 0;
  virtual uint64_t bytes_received() const = 0;

  virtual uint64_t buffered_amount() const = 0;

  virtual void Close() = 0;

  virtual bool Send(const DataBuffer& buffer);

  virtual void SendAsync(DataBuffer buffer,
                         absl::AnyInvocable<void(RTCError) &&> on_complete);

  static uint64_t MaxSendQueueSize();

 protected:
  ~DataChannelInterface() override = default;
};

}

#endif
