/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SEQUENCE_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SEQUENCE_BUFFER_H_

#include <algorithm>
#include <cstring>
#include <type_traits>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {

template <typename T, int S, int N, int M = N>
class SequenceBuffer {
  static_assert(N <= S,
                "The new chunk size cannot be larger than the sequence buffer "
                "size.");
  static_assert(std::is_arithmetic<T>::value,
                "Integral or floating point required.");

 public:
  SequenceBuffer() : buffer_(S) {
    RTC_DCHECK_EQ(S, buffer_.size());
    Reset();
  }
  SequenceBuffer(const SequenceBuffer&) = delete;
  SequenceBuffer& operator=(const SequenceBuffer&) = delete;
  ~SequenceBuffer() = default;
  int size() const { return S; }
  int chunks_size() const { return N; }

  void Reset() { std::fill(buffer_.begin(), buffer_.end(), 0); }

  rtc::ArrayView<const T, S> GetBufferView() const {
    return {buffer_.data(), S};
  }

  rtc::ArrayView<const T, M> GetMostRecentValuesView() const {
    static_assert(M <= S,
                  "The number of most recent values cannot be larger than the "
                  "sequence buffer size.");
    return {buffer_.data() + S - M, M};
  }

  void Push(rtc::ArrayView<const T, N> new_values) {

    if (S > N)
      std::memmove(buffer_.data(), buffer_.data() + N, (S - N) * sizeof(T));

    std::memcpy(buffer_.data() + S - N, new_values.data(), N * sizeof(T));
  }

 private:
  std::vector<T> buffer_;
};

}
}

#endif
