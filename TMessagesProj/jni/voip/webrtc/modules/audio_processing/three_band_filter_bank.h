/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_THREE_BAND_FILTER_BANK_H_
#define MODULES_AUDIO_PROCESSING_THREE_BAND_FILTER_BANK_H_

#include <array>
#include <cstring>
#include <memory>
#include <vector>

#include "api/array_view.h"

namespace webrtc {

constexpr int kSparsity = 4;
constexpr int kStrideLog2 = 2;
constexpr int kStride = 1 << kStrideLog2;
constexpr int kNumZeroFilters = 2;
constexpr int kFilterSize = 4;
constexpr int kMemorySize = kFilterSize * kStride - 1;
static_assert(kMemorySize == 15,
              "The memory size must be sufficient to provide memory for the "
              "shifted filters");

class ThreeBandFilterBank final {
 public:
  static const int kNumBands = 3;
  static const int kFullBandSize = 480;
  static const int kSplitBandSize =
      ThreeBandFilterBank::kFullBandSize / ThreeBandFilterBank::kNumBands;
  static const int kNumNonZeroFilters =
      kSparsity * ThreeBandFilterBank::kNumBands - kNumZeroFilters;

  ThreeBandFilterBank();
  ~ThreeBandFilterBank();

  void Analysis(rtc::ArrayView<const float, kFullBandSize> in,
                rtc::ArrayView<const rtc::ArrayView<float>, kNumBands> out);

  void Synthesis(rtc::ArrayView<const rtc::ArrayView<float>, kNumBands> in,
                 rtc::ArrayView<float, kFullBandSize> out);

 private:
  std::array<std::array<float, kMemorySize>, kNumNonZeroFilters>
      state_analysis_;
  std::array<std::array<float, kMemorySize>, kNumNonZeroFilters>
      state_synthesis_;
};

}

#endif
