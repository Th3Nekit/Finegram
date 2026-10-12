/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RING_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RING_BUFFER_H_

#include <array>
#include <cstring>
#include <type_traits>

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

template <typename T, int S, int N>
class RingBuffer {
  static_assert(S > 0, "");
  static_assert(N > 0, "");
  static_assert(std::is_arithmetic<T>::value,
                "Integral or floating point required.");

 public:
  RingBuffer() : tail_(0) {}
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  ~RingBuffer() = default;

  void Reset() { buffer_.fill(0); }

  void Push(rtc::ArrayView<const T, S> new_values) {
    std::memcpy(buffer_.data() + S * tail_, new_values.data(), S * sizeof(T));
    tail_ += 1;
    if (tail_ == N)
      tail_ = 0;
  }

  rtc::ArrayView<const T, S> GetArrayView(int delay) const {
    RTC_DCHECK_LE(0, delay);
    RTC_DCHECK_LT(delay, N);
    int offset = tail_ - 1 - delay;
    if (offset < 0)
      offset += N;
    return {buffer_.data() + S * offset, S};
  }

 private:
  int tail_;
  std::array<T, S * N> buffer_{};
};

}
}

#endif
