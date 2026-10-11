/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SOCKET_SERVER_H_
#define RTC_BASE_SOCKET_SERVER_H_

#include <memory>

#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/socket_factory.h"

namespace rtc {

class Thread;

class NetworkBinderInterface;

class SocketServer : public SocketFactory {
 public:
  static constexpr webrtc::TimeDelta kForever = rtc::Event::kForever;

  static std::unique_ptr<SocketServer> CreateDefault();

  virtual void SetMessageQueue(Thread* queue) {}

  virtual bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) = 0;

  virtual void WakeUp() = 0;

  void set_network_binder(NetworkBinderInterface* binder) {
    network_binder_ = binder;
  }
  NetworkBinderInterface* network_binder() const { return network_binder_; }

 private:
  NetworkBinderInterface* network_binder_ = nullptr;
};

}

#endif
