/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_STUN_REQUEST_H_
#define P2P_BASE_STUN_REQUEST_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"

namespace cricket {

class StunRequest;

const int kAllRequestsForTest = 0;

const int STUN_TOTAL_TIMEOUT = 39750;

class StunRequestManager {
 public:
  StunRequestManager(
      webrtc::TaskQueueBase* thread,
      std::function<void(const void*, size_t, StunRequest*)> send_packet);
  ~StunRequestManager();

  void Send(StunRequest* request);
  void SendDelayed(StunRequest* request, int delay);

  void FlushForTest(int msg_type);

  bool HasRequestForTest(int msg_type);

  void Clear();

  bool CheckResponse(StunMessage* msg);
  bool CheckResponse(const char* data, size_t size);

  void OnRequestTimedOut(StunRequest* request);

  bool empty() const;

  webrtc::TaskQueueBase* network_thread() const { return thread_; }

  void SendPacket(const void* data, size_t size, StunRequest* request);

 private:
  typedef std::map<std::string, std::unique_ptr<StunRequest>> RequestMap;

  webrtc::TaskQueueBase* const thread_;
  RequestMap requests_ RTC_GUARDED_BY(thread_);
  const std::function<void(const void*, size_t, StunRequest*)> send_packet_;
};

class StunRequest {
 public:
  explicit StunRequest(StunRequestManager& manager);
  StunRequest(StunRequestManager& manager,
              std::unique_ptr<StunMessage> message);
  virtual ~StunRequest();

  StunRequestManager* manager() { return &manager_; }

  const std::string& id() const { return msg_->transaction_id(); }

  uint32_t reduced_transaction_id() const {
    return msg_->reduced_transaction_id();
  }

  int type();

  const StunMessage* msg() const;

  int Elapsed() const;

  void SetAuthenticationRequired(bool val) { authentication_required_ = val; }
  bool AuthenticationRequired() const { return authentication_required_; }

 protected:
  friend class StunRequestManager;

  void Send(webrtc::TimeDelta delay);

  void ResetTasksForTest();

  StunMessage* mutable_msg() { return msg_.get(); }

  virtual void OnResponse(StunMessage* response) {}
  virtual void OnErrorResponse(StunMessage* response) {}
  virtual void OnTimeout() {}

  virtual void OnSent();

  virtual int resend_delay();

  webrtc::TaskQueueBase* network_thread() const {
    return manager_.network_thread();
  }

  void set_timed_out();

 private:
  void SendInternal();

  void SendDelayed(webrtc::TimeDelta delay);

  StunRequestManager& manager_;
  const std::unique_ptr<StunMessage> msg_;
  int64_t tstamp_ RTC_GUARDED_BY(network_thread());
  int count_ RTC_GUARDED_BY(network_thread());
  bool timeout_ RTC_GUARDED_BY(network_thread());
  webrtc::ScopedTaskSafety task_safety_{
      webrtc::PendingTaskSafetyFlag::CreateDetachedInactive()};
  bool authentication_required_ = true;
};

}

#endif
