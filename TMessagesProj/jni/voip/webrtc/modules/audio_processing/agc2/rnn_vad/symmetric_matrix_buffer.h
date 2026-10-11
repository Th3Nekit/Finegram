/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SYMMETRIC_MATRIX_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SYMMETRIC_MATRIX_BUFFER_H_

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_compare.h"

namespace webrtc {
namespace rnn_vad {

template <typename T, int S>
class SymmetricMatrixBuffer {
  static_assert(S > 2, "");

 public:
  SymmetricMatrixBuffer() = default;
  SymmetricMatrixBuffer(const SymmetricMatrixBuffer&) = delete;
  SymmetricMatrixBuffer& operator=(const SymmetricMatrixBuffer&) = delete;
  ~SymmetricMatrixBuffer() = default;

  void Reset() {
    static_assert(std::is_arithmetic<T>::value,
                  "Integral or floating point required.");
    buf_.fill(0);
  }

  void Push(rtc::ArrayView<T, S - 1> values) {

    std::memmove(buf_.data(), buf_.data() + S, (buf_.size() - S) * sizeof(T));

    for (int i = 0; rtc::SafeLt(i, values.size()); ++i) {
      const int index = (S - 1 - i) * (S - 1) - 1;
      RTC_DCHECK_GE(index, 0);
      RTC_DCHECK_LT(index, buf_.size());
      buf_[index] = values[i];
    }
  }

  T GetValue(int delay1, int delay2) const {
    int row = S - 1 - delay1;
    int col = S - 1 - delay2;
    RTC_DCHECK_NE(row, col) << "The diagonal cannot be accessed.";
    if (row > col)
      std::swap(row, col);
    RTC_DCHECK_LE(0, row);
    RTC_DCHECK_LT(row, S - 1) << "Not enforcing row < col and row != col.";
    RTC_DCHECK_LE(1, col) << "Not enforcing row < col and row != col.";
    RTC_DCHECK_LT(col, S);
    const int index = row * (S - 1) + (col - 1);
    RTC_DCHECK_LE(0, index);
    RTC_DCHECK_LT(index, buf_.size());
    return buf_[index];
  }

 private:

  std::array<T, (S - 1) * (S - 1)> buf_{};
};

}
}

#endif
