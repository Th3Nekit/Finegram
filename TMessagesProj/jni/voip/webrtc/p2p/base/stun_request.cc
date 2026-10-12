/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_request.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/time_utils.h"

namespace cricket {
using ::webrtc::SafeTask;

const int STUN_INITIAL_RTO = 250;

const int STUN_MAX_RETRANSMISSIONS = 8;

const int STUN_MAX_RTO = 8000;

StunRequestManager::StunRequestManager(
    webrtc::TaskQueueBase* thread,
    std::function<void(const void*, size_t, StunRequest*)> send_packet)
    : thread_(thread), send_packet_(std::move(send_packet)) {}

StunRequestManager::~StunRequestManager() = default;

void StunRequestManager::Send(StunRequest* request) {
  SendDelayed(request, 0);
}

void StunRequestManager::SendDelayed(StunRequest* request, int delay) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK_EQ(this, request->manager());
  RTC_DCHECK(!request->AuthenticationRequired() ||
             request->msg()->integrity() !=
                 StunMessage::IntegrityStatus::kNotSet)
      << "Sending request w/o integrity!";
  auto [iter, was_inserted] =
      requests_.emplace(request->id(), absl::WrapUnique(request));
  RTC_DCHECK(was_inserted);
  request->Send(webrtc::TimeDelta::Millis(delay));
}

void StunRequestManager::FlushForTest(int msg_type) {
  RTC_DCHECK_RUN_ON(thread_);
  for (const auto& [unused, request] : requests_) {
    if (msg_type == kAllRequestsForTest || msg_type == request->type()) {

      request->ResetTasksForTest();
      request->Send(webrtc::TimeDelta::Zero());
    }
  }
}

bool StunRequestManager::HasRequestForTest(int msg_type) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK_NE(msg_type, kAllRequestsForTest);
  for (const auto& [unused, request] : requests_) {
    if (msg_type == request->type()) {
      return true;
    }
  }
  return false;
}

void StunRequestManager::Clear() {
  RTC_DCHECK_RUN_ON(thread_);
  requests_.clear();
}

bool StunRequestManager::CheckResponse(StunMessage* msg) {
  RTC_DCHECK_RUN_ON(thread_);
  RequestMap::iterator iter = requests_.find(msg->transaction_id());
  if (iter == requests_.end())
    return false;

  StunRequest* request = iter->second.get();

  bool skip_integrity_checking =
      (request->msg()->integrity() == StunMessage::IntegrityStatus::kNotSet);
  if (!request->AuthenticationRequired()) {

  } else if (skip_integrity_checking) {

    RTC_LOG(LS_ERROR)
        << "CheckResponse called on a passwordless request. Fix test!";
    RTC_DCHECK(false)
        << "CheckResponse called on a passwordless request. Fix test!";
  } else {
    if (msg->integrity() == StunMessage::IntegrityStatus::kNotSet) {

      msg->ValidateMessageIntegrity(request->msg()->password());
    } else if (msg->integrity() == StunMessage::IntegrityStatus::kIntegrityOk &&
               msg->password() == request->msg()->password()) {

    } else if (msg->integrity() ==
               StunMessage::IntegrityStatus::kIntegrityBad) {

      msg->RevalidateMessageIntegrity(request->msg()->password());
    } else {
      RTC_CHECK_NOTREACHED();
    }
  }

  if (!msg->GetNonComprehendedAttributes().empty()) {

    RTC_LOG(LS_ERROR) << ": Discarding response due to unknown "
                         "comprehension-required attribute.";
    requests_.erase(iter);
    return false;
  } else if (msg->type() == GetStunSuccessResponseType(request->type())) {
    if (!msg->IntegrityOk() && !skip_integrity_checking) {
      return false;
    }

    std::unique_ptr<StunRequest> owned_request = std::move(iter->second);
    requests_.erase(iter);
    owned_request->OnResponse(msg);
    return true;
  } else if (msg->type() == GetStunErrorResponseType(request->type())) {

    std::unique_ptr<StunRequest> owned_request = std::move(iter->second);
    requests_.erase(iter);
    owned_request->OnErrorResponse(msg);
    return true;
  } else {
    RTC_LOG(LS_ERROR) << "Received response with wrong type: " << msg->type()
                      << " (expecting "
                      << GetStunSuccessResponseType(request->type()) << ")";
    return false;
  }
}

bool StunRequestManager::empty() const {
  RTC_DCHECK_RUN_ON(thread_);
  return requests_.empty();
}

bool StunRequestManager::CheckResponse(const char* data, size_t size) {
  RTC_DCHECK_RUN_ON(thread_);

  if (size < 20)
    return false;

  std::string id;
  id.append(data + kStunTransactionIdOffset, kStunTransactionIdLength);

  RequestMap::iterator iter = requests_.find(id);
  if (iter == requests_.end())
    return false;

  rtc::ByteBufferReader buf(
      rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data), size));
  std::unique_ptr<StunMessage> response(iter->second->msg_->CreateNew());
  if (!response->Read(&buf)) {
    RTC_LOG(LS_WARNING) << "Failed to read STUN response "
                        << rtc::hex_encode(id);
    return false;
  }

  return CheckResponse(response.get());
}

void StunRequestManager::OnRequestTimedOut(StunRequest* request) {
  RTC_DCHECK_RUN_ON(thread_);
  requests_.erase(request->id());
}

void StunRequestManager::SendPacket(const void* data,
                                    size_t size,
                                    StunRequest* request) {
  RTC_DCHECK_EQ(this, request->manager());
  send_packet_(data, size, request);
}

StunRequest::StunRequest(StunRequestManager& manager)
    : manager_(manager),
      msg_(new StunMessage(STUN_INVALID_MESSAGE_TYPE)),
      tstamp_(0),
      count_(0),
      timeout_(false) {
  RTC_DCHECK_RUN_ON(network_thread());
}

StunRequest::StunRequest(StunRequestManager& manager,
                         std::unique_ptr<StunMessage> message)
    : manager_(manager),
      msg_(std::move(message)),
      tstamp_(0),
      count_(0),
      timeout_(false) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(!msg_->transaction_id().empty());
}

StunRequest::~StunRequest() {}

int StunRequest::type() {
  RTC_DCHECK(msg_ != NULL);
  return msg_->type();
}

const StunMessage* StunRequest::msg() const {
  return msg_.get();
}

int StunRequest::Elapsed() const {
  RTC_DCHECK_RUN_ON(network_thread());
  return static_cast<int>(rtc::TimeMillis() - tstamp_);
}

void StunRequest::SendInternal() {
  RTC_DCHECK_RUN_ON(network_thread());
  if (timeout_) {
    OnTimeout();
    manager_.OnRequestTimedOut(this);
    return;
  }

  tstamp_ = rtc::TimeMillis();

  rtc::ByteBufferWriter buf;
  msg_->Write(&buf);
  manager_.SendPacket(buf.Data(), buf.Length(), this);

  OnSent();
  SendDelayed(webrtc::TimeDelta::Millis(resend_delay()));
}

void StunRequest::SendDelayed(webrtc::TimeDelta delay) {
  network_thread()->PostDelayedTask(
      SafeTask(task_safety_.flag(), [this]() { SendInternal(); }), delay);
}

void StunRequest::Send(webrtc::TimeDelta delay) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK_GE(delay.ms(), 0);

  RTC_DCHECK(!task_safety_.flag()->alive()) << "Send already called?";
  task_safety_.flag()->SetAlive();

  delay.IsZero() ? SendInternal() : SendDelayed(delay);
}

void StunRequest::ResetTasksForTest() {
  RTC_DCHECK_RUN_ON(network_thread());
  task_safety_.reset(webrtc::PendingTaskSafetyFlag::CreateDetachedInactive());
  count_ = 0;
  RTC_DCHECK(!timeout_);
}

void StunRequest::OnSent() {
  RTC_DCHECK_RUN_ON(network_thread());
  count_ += 1;
  int retransmissions = (count_ - 1);
  if (retransmissions >= STUN_MAX_RETRANSMISSIONS) {
    timeout_ = true;
  }
  RTC_DLOG(LS_VERBOSE) << "Sent STUN request " << count_
                       << "; resend delay = " << resend_delay();
}

int StunRequest::resend_delay() {
  RTC_DCHECK_RUN_ON(network_thread());
  if (count_ == 0) {
    return 0;
  }
  int retransmissions = (count_ - 1);
  int rto = STUN_INITIAL_RTO << retransmissions;
  return std::min(rto, STUN_MAX_RTO);
}

void StunRequest::set_timed_out() {
  RTC_DCHECK_RUN_ON(network_thread());
  timeout_ = true;
}

}
