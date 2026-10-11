/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SWAP_QUEUE_H_
#define RTC_BASE_SWAP_QUEUE_H_

#include <stddef.h>

#include <atomic>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace internal {

template <typename T>
bool NoopSwapQueueItemVerifierFunction(const T&) {
  return true;
}

}

template <typename T,
          bool (*QueueItemVerifierFunction)(const T&) =
              internal::NoopSwapQueueItemVerifierFunction>
class SwapQueueItemVerifier {
 public:
  bool operator()(const T& t) const { return QueueItemVerifierFunction(t); }
};

template <typename T, typename QueueItemVerifier = SwapQueueItemVerifier<T>>
class SwapQueue {
 public:

  explicit SwapQueue(size_t size) : queue_(size) {
    RTC_DCHECK(VerifyQueueSlots());
  }

  SwapQueue(size_t size, const QueueItemVerifier& queue_item_verifier)
      : queue_item_verifier_(queue_item_verifier), queue_(size) {
    RTC_DCHECK(VerifyQueueSlots());
  }

  SwapQueue(size_t size, const T& prototype) : queue_(size, prototype) {
    RTC_DCHECK(VerifyQueueSlots());
  }

  SwapQueue(size_t size,
            const T& prototype,
            const QueueItemVerifier& queue_item_verifier)
      : queue_item_verifier_(queue_item_verifier), queue_(size, prototype) {
    RTC_DCHECK(VerifyQueueSlots());
  }

  void Clear() {

    next_read_index_ += std::atomic_exchange_explicit(
        &num_elements_, size_t{0}, std::memory_order_relaxed);
    if (next_read_index_ >= queue_.size()) {
      next_read_index_ -= queue_.size();
    }

    RTC_DCHECK_LT(next_read_index_, queue_.size());
  }

  ABSL_MUST_USE_RESULT bool Insert(T* input) {
    RTC_DCHECK(input);

    RTC_DCHECK(queue_item_verifier_(*input));

    if (std::atomic_load_explicit(&num_elements_, std::memory_order_acquire) ==
        queue_.size()) {
      return false;
    }

    using std::swap;
    swap(*input, queue_[next_write_index_]);

    const size_t old_num_elements = std::atomic_fetch_add_explicit(
        &num_elements_, size_t{1}, std::memory_order_release);

    ++next_write_index_;
    if (next_write_index_ == queue_.size()) {
      next_write_index_ = 0;
    }

    RTC_DCHECK_LT(next_write_index_, queue_.size());
    RTC_DCHECK_LT(old_num_elements, queue_.size());

    return true;
  }

  ABSL_MUST_USE_RESULT bool Remove(T* output) {
    RTC_DCHECK(output);

    RTC_DCHECK(queue_item_verifier_(*output));

    if (std::atomic_load_explicit(&num_elements_, std::memory_order_acquire) ==
        0) {
      return false;
    }

    using std::swap;
    swap(*output, queue_[next_read_index_]);

    std::atomic_fetch_sub_explicit(&num_elements_, size_t{1},
                                   std::memory_order_release);

    ++next_read_index_;
    if (next_read_index_ == queue_.size()) {
      next_read_index_ = 0;
    }

    RTC_DCHECK_LT(next_read_index_, queue_.size());

    return true;
  }

  size_t SizeAtLeast() const {

    return std::atomic_load_explicit(&num_elements_, std::memory_order_acquire);
  }

 private:

  bool VerifyQueueSlots() {
    for (const auto& v : queue_) {
      RTC_DCHECK(queue_item_verifier_(v));
    }
    return true;
  }

  QueueItemVerifier queue_item_verifier_;

  size_t next_write_index_ = 0;

  size_t next_read_index_ = 0;

  std::atomic<size_t> num_elements_{0};

  std::vector<T> queue_;

  SwapQueue(const SwapQueue&) = delete;
  SwapQueue& operator=(const SwapQueue&) = delete;
};

}

#endif
