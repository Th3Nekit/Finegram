/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_dns_resolver.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/make_ref_counted.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
#include <dispatch/dispatch.h>
#endif

namespace webrtc {

namespace {

#ifdef __native_client__
int ResolveHostname(absl::string_view hostname,
                    int family,
                    std::vector<rtc::IPAddress>* addresses) {
  RTC_DCHECK_NOTREACHED();
  RTC_LOG(LS_WARNING) << "ResolveHostname() is not implemented for NaCl";
  return -1;
}
#else
int ResolveHostname(absl::string_view hostname,
                    int family,
                    std::vector<rtc::IPAddress>& addresses) {
  addresses.clear();
  struct addrinfo* result = nullptr;
  struct addrinfo hints = {0};
  hints.ai_family = family;

  hints.ai_flags = AI_ADDRCONFIG;
  int ret =
      getaddrinfo(std::string(hostname).c_str(), nullptr, &hints, &result);
  if (ret != 0) {
    return ret;
  }
  struct addrinfo* cursor = result;
  for (; cursor; cursor = cursor->ai_next) {
    if (family == AF_UNSPEC || cursor->ai_family == family) {
      rtc::IPAddress ip;
      if (IPFromAddrInfo(cursor, &ip)) {
        addresses.push_back(ip);
      }
    }
  }
  freeaddrinfo(result);
  return 0;
}
#endif

#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
void GlobalGcdRunTask(void* context) {
  std::unique_ptr<absl::AnyInvocable<void() &&>> task(
      static_cast<absl::AnyInvocable<void() &&>*>(context));
  std::move (*task)();
}

void PostTaskToGlobalQueue(
    std::unique_ptr<absl::AnyInvocable<void() &&>> task) {
  dispatch_async_f(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
      task.release(), &GlobalGcdRunTask);
}
#endif

}

class AsyncDnsResolver::State : public rtc::RefCountedBase {
 public:
  enum class Status {
    kActive,
    kFinished,
    kDead
  };
  static rtc::scoped_refptr<AsyncDnsResolver::State> Create() {
    return rtc::make_ref_counted<AsyncDnsResolver::State>();
  }

  void Finish(absl::AnyInvocable<void()> function) {
    webrtc::MutexLock lock(&mutex_);
    if (status_ != Status::kActive) {
      return;
    }
    status_ = Status::kFinished;
    function();
  }
  void Kill() {
    webrtc::MutexLock lock(&mutex_);
    status_ = Status::kDead;
  }

 private:
  webrtc::Mutex mutex_;
  Status status_ RTC_GUARDED_BY(mutex_) = Status::kActive;
};

AsyncDnsResolver::AsyncDnsResolver() : state_(State::Create()) {}

AsyncDnsResolver::~AsyncDnsResolver() {
  state_->Kill();
}

void AsyncDnsResolver::Start(const rtc::SocketAddress& addr,
                             absl::AnyInvocable<void()> callback) {
  Start(addr, addr.family(), std::move(callback));
}

void AsyncDnsResolver::Start(const rtc::SocketAddress& addr,
                             int family,
                             absl::AnyInvocable<void()> callback) {
  RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
  result_.addr_ = addr;
  callback_ = std::move(callback);
  auto thread_function = [this, addr, family, flag = safety_.flag(),
                          caller_task_queue = webrtc::TaskQueueBase::Current(),
                          state = state_] {
    std::vector<rtc::IPAddress> addresses;
    int error = ResolveHostname(addr.hostname(), family, addresses);

    state->Finish([this, error, flag, caller_task_queue,
                   addresses = std::move(addresses)]() {
      caller_task_queue->PostTask(
          SafeTask(flag, [this, error, addresses = std::move(addresses)] {
            RTC_DCHECK_RUN_ON(&result_.sequence_checker_);
            result_.addresses_ = addresses;
            result_.error_ = error;
            callback_();
          }));
    });
  };
#if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  PostTaskToGlobalQueue(
      std::make_unique<absl::AnyInvocable<void() &&>>(thread_function));
#else
  rtc::PlatformThread::SpawnDetached(std::move(thread_function),
                                     "AsyncResolver");
#endif
}

const AsyncDnsResolverResult& AsyncDnsResolver::result() const {
  return result_;
}

bool AsyncDnsResolverResultImpl::GetResolvedAddress(
    int family,
    rtc::SocketAddress* addr) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(addr);
  if (error_ != 0 || addresses_.empty())
    return false;

  *addr = addr_;
  for (const auto& address : addresses_) {
    if (family == address.family()) {
      addr->SetResolvedIP(address);
      return true;
    }
  }
  return false;
}

int AsyncDnsResolverResultImpl::GetError() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return error_;
}

}
