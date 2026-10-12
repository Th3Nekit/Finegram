/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_SEQUENCE_CHECKER_H_
#define API_SEQUENCE_CHECKER_H_

#include "rtc_base/checks.h"
#include "rtc_base/synchronization/sequence_checker_internal.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_LOCKABLE SequenceChecker
#if RTC_DCHECK_IS_ON
    : public webrtc_sequence_checker_internal::SequenceCheckerImpl {
  using Impl = webrtc_sequence_checker_internal::SequenceCheckerImpl;
#else
    : public webrtc_sequence_checker_internal::SequenceCheckerDoNothing {
  using Impl = webrtc_sequence_checker_internal::SequenceCheckerDoNothing;
#endif
 public:
  enum InitialState : bool { kDetached = false, kAttached = true };

  explicit SequenceChecker(InitialState initial_state = kAttached)
      : Impl(initial_state) {}
  explicit SequenceChecker(TaskQueueBase* attached_queue)
      : Impl(attached_queue) {}

  bool IsCurrent() const { return Impl::IsCurrent(); }

  void Detach() { Impl::Detach(); }
};

}

#define RTC_RUN_ON(x) \
  RTC_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(x))

#define RTC_DCHECK_RUN_ON(x)                                               \
  RTC_DCHECK((x)->IsCurrent())                                             \
      << webrtc::webrtc_sequence_checker_internal::ExpectationToString(x); \
  []() RTC_ASSERT_EXCLUSIVE_LOCK(x) {}()

#endif
