/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ASYNC_DNS_RESOLVER_H_
#define API_ASYNC_DNS_RESOLVER_H_

#include <functional>
#include <memory>

#include "absl/functional/any_invocable.h"
#include "rtc_base/checks.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class AsyncDnsResolverResult {
 public:
  virtual ~AsyncDnsResolverResult() = default;

  virtual bool GetResolvedAddress(int family,
                                  rtc::SocketAddress* addr) const = 0;

  virtual int GetError() const = 0;
};

class RTC_EXPORT AsyncDnsResolverInterface {
 public:
  virtual ~AsyncDnsResolverInterface() = default;

  virtual void Start(const rtc::SocketAddress& addr,
                     absl::AnyInvocable<void()> callback) = 0;

  virtual void Start(const rtc::SocketAddress& addr,
                     int family,
                     absl::AnyInvocable<void()> callback) = 0;
  virtual const AsyncDnsResolverResult& result() const = 0;
};

class AsyncDnsResolverFactoryInterface {
 public:
  virtual ~AsyncDnsResolverFactoryInterface() = default;

  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      absl::AnyInvocable<void()> callback) = 0;

  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(
      const rtc::SocketAddress& addr,
      int family,
      absl::AnyInvocable<void()> callback) = 0;

  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface> Create() = 0;
};

}

#endif
