/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CALLBACK_LIST_H_
#define RTC_BASE_CALLBACK_LIST_H_

#include <utility>
#include <vector>

#include "api/function_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/assume.h"
#include "rtc_base/system/inline.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/untyped_function.h"

namespace webrtc {
namespace callback_list_impl {

class RTC_EXPORT CallbackListReceivers {
 public:
  CallbackListReceivers();
  CallbackListReceivers(const CallbackListReceivers&) = delete;
  CallbackListReceivers& operator=(const CallbackListReceivers&) = delete;
  CallbackListReceivers(CallbackListReceivers&&) = delete;
  CallbackListReceivers& operator=(CallbackListReceivers&&) = delete;
  ~CallbackListReceivers();

  template <typename UntypedFunctionArgsT>
  RTC_NO_INLINE void AddReceiver(const void* removal_tag,
                                 UntypedFunctionArgsT args) {
    RTC_CHECK(!send_in_progress_);
    RTC_DCHECK(removal_tag != nullptr);
    receivers_.push_back({removal_tag, UntypedFunction::Create(args)});
  }

  template <typename UntypedFunctionArgsT>
  RTC_NO_INLINE void AddReceiver(UntypedFunctionArgsT args) {
    RTC_CHECK(!send_in_progress_);
    receivers_.push_back({nullptr, UntypedFunction::Create(args)});
  }

  void RemoveReceivers(const void* removal_tag);

  void Foreach(rtc::FunctionView<void(UntypedFunction&)> fv);

 private:

  const void* pending_removal_tag() const { return &send_in_progress_; }

  struct Callback {
    const void* removal_tag;
    UntypedFunction function;
  };

  std::vector<Callback> receivers_;
  bool send_in_progress_ = false;
};

extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::TrivialUntypedFunctionArgs<1>);
extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::TrivialUntypedFunctionArgs<2>);
extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::TrivialUntypedFunctionArgs<3>);
extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::TrivialUntypedFunctionArgs<4>);
extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::NontrivialUntypedFunctionArgs);
extern template void CallbackListReceivers::AddReceiver(
    const void*,
    UntypedFunction::FunctionPointerUntypedFunctionArgs);

extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<1>);
extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<2>);
extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<3>);
extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::TrivialUntypedFunctionArgs<4>);
extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::NontrivialUntypedFunctionArgs);
extern template void CallbackListReceivers::AddReceiver(
    UntypedFunction::FunctionPointerUntypedFunctionArgs);

}

template <typename... ArgT>
class CallbackList {
 public:
  CallbackList() = default;
  CallbackList(const CallbackList&) = delete;
  CallbackList& operator=(const CallbackList&) = delete;
  CallbackList(CallbackList&&) = delete;
  CallbackList& operator=(CallbackList&&) = delete;

  template <typename F>
  void AddReceiver(const void* removal_tag, F&& f) {
    receivers_.AddReceiver(
        removal_tag,
        UntypedFunction::PrepareArgs<void(ArgT...)>(std::forward<F>(f)));
  }

  template <typename F>
  void AddReceiver(F&& f) {
    receivers_.AddReceiver(
        UntypedFunction::PrepareArgs<void(ArgT...)>(std::forward<F>(f)));
  }

  void RemoveReceivers(const void* removal_tag) {
    receivers_.RemoveReceivers(removal_tag);
  }

  template <typename... ArgU>
  void Send(ArgU&&... args) {
    receivers_.Foreach([&](UntypedFunction& f) {
      f.Call<void(ArgT...)>(std::forward<ArgU>(args)...);
    });
  }

 private:
  callback_list_impl::CallbackListReceivers receivers_;
};

}

#endif
