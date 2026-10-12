/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_ASYNC_DNS_RESOLVER_H_
#define RTC_BASE_ASYNC_DNS_RESOLVER_H_

#include <vector>

#include "api/async_dns_resolver.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class AsyncDnsResolverResultImpl : public AsyncDnsResolverResult {
 public:
  bool GetResolvedAddress(int family, rtc::SocketAddress* addr) const override;

  int GetError() const override;

 private:
  friend class AsyncDnsResolver;
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;
  rtc::SocketAddress addr_ RTC_GUARDED_BY(sequence_checker_);
  std::vector<rtc::IPAddress> addresses_ RTC_GUARDED_BY(sequence_checker_);
  int error_ RTC_GUARDED_BY(sequence_checker_);
};

class RTC_EXPORT AsyncDnsResolver : public AsyncDnsResolverInterface {
 public:
  AsyncDnsResolver();
  ~AsyncDnsResolver();

  void Start(const rtc::SocketAddress& addr,
             absl::AnyInvocable<void()> callback) override;

  void Start(const rtc::SocketAddress& addr,
             int family,
             absl::AnyInvocable<void()> callback) override;
  const AsyncDnsResolverResult& result() const override;

 private:
  class State;
  ScopedTaskSafety safety_;
  rtc::scoped_refptr<State> state_;
  AsyncDnsResolverResultImpl result_;
  absl::AnyInvocable<void()> callback_;
};

}
#endif
