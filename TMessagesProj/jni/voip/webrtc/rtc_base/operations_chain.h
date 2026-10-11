/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPERATIONS_CHAIN_H_
#define RTC_BASE_OPERATIONS_CHAIN_H_

#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <type_traits>
#include <utility>

#include "absl/types/optional.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/system/no_unique_address.h"

namespace rtc {

namespace rtc_operations_chain_internal {

class Operation {
 public:
  virtual ~Operation() {}

  virtual void Run() = 0;
};

template <typename FunctorT>
class OperationWithFunctor final : public Operation {
 public:
  OperationWithFunctor(FunctorT&& functor, std::function<void()> callback)
      : functor_(std::forward<FunctorT>(functor)),
        callback_(std::move(callback)) {}

  ~OperationWithFunctor() override {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(has_run_);
#endif
  }

  void Run() override {
#if RTC_DCHECK_IS_ON
    RTC_DCHECK(!has_run_);
    has_run_ = true;
#endif

    auto functor = std::move(functor_);
    functor(std::move(callback_));

  }

 private:
  typename std::remove_reference<FunctorT>::type functor_;
  std::function<void()> callback_;
#if RTC_DCHECK_IS_ON
  bool has_run_ = false;
#endif
};

}

class OperationsChain final : public RefCountedNonVirtual<OperationsChain> {
 public:
  static scoped_refptr<OperationsChain> Create();
  ~OperationsChain();

  OperationsChain(const OperationsChain&) = delete;
  OperationsChain& operator=(const OperationsChain&) = delete;

  void SetOnChainEmptyCallback(std::function<void()> on_chain_empty_callback);
  bool IsEmpty() const;

  template <typename FunctorT>
  void ChainOperation(FunctorT&& functor) {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    chained_operations_.push(
        std::make_unique<
            rtc_operations_chain_internal::OperationWithFunctor<FunctorT>>(
            std::forward<FunctorT>(functor), CreateOperationsChainCallback()));

    if (chained_operations_.size() == 1) {
      chained_operations_.front()->Run();
    }
  }

 private:
  friend class CallbackHandle;

  class CallbackHandle final : public RefCountedNonVirtual<CallbackHandle> {
   public:
    explicit CallbackHandle(scoped_refptr<OperationsChain> operations_chain);
    ~CallbackHandle();

    CallbackHandle(const CallbackHandle&) = delete;
    CallbackHandle& operator=(const CallbackHandle&) = delete;

    void OnOperationComplete();

   private:
    scoped_refptr<OperationsChain> operations_chain_;
#if RTC_DCHECK_IS_ON
    bool has_run_ = false;
#endif
  };

  OperationsChain();

  std::function<void()> CreateOperationsChainCallback();
  void OnOperationComplete();

  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;

  std::queue<std::unique_ptr<rtc_operations_chain_internal::Operation>>
      chained_operations_ RTC_GUARDED_BY(sequence_checker_);
  absl::optional<std::function<void()>> on_chain_empty_callback_
      RTC_GUARDED_BY(sequence_checker_);
};

}

#endif
