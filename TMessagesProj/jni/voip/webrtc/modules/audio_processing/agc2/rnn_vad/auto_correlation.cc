/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/auto_correlation.h"

#include <algorithm>

#include "rtc_base/checks.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kAutoCorrelationFftOrder = 9;
static_assert(1 << kAutoCorrelationFftOrder >
                  kNumLags12kHz + kBufSize12kHz - kMaxPitch12kHz,
              "");

}

AutoCorrelationCalculator::AutoCorrelationCalculator()
    : fft_(1 << kAutoCorrelationFftOrder, Pffft::FftType::kReal),
      tmp_(fft_.CreateBuffer()),
      X_(fft_.CreateBuffer()),
      H_(fft_.CreateBuffer()) {}

AutoCorrelationCalculator::~AutoCorrelationCalculator() = default;

void AutoCorrelationCalculator::ComputeOnPitchBuffer(
    rtc::ArrayView<const float, kBufSize12kHz> pitch_buf,
    rtc::ArrayView<float, kNumLags12kHz> auto_corr) {
  RTC_DCHECK_LT(auto_corr.size(), kMaxPitch12kHz);
  RTC_DCHECK_GT(pitch_buf.size(), kMaxPitch12kHz);
  constexpr int kFftFrameSize = 1 << kAutoCorrelationFftOrder;
  constexpr int kConvolutionLength = kBufSize12kHz - kMaxPitch12kHz;
  static_assert(kConvolutionLength == kFrameSize20ms12kHz,
                "Mismatch between pitch buffer size, frame size and maximum "
                "pitch period.");
  static_assert(kFftFrameSize > kNumLags12kHz + kConvolutionLength,
                "The FFT length is not sufficiently big to avoid cyclic "
                "convolution errors.");
  auto tmp = tmp_->GetView();

  std::reverse_copy(pitch_buf.end() - kConvolutionLength, pitch_buf.end(),
                    tmp.begin());
  std::fill(tmp.begin() + kConvolutionLength, tmp.end(), 0.f);
  fft_.ForwardTransform(*tmp_, H_.get(),             false);

  std::copy(pitch_buf.begin(),
            pitch_buf.begin() + kConvolutionLength + kNumLags12kHz,
            tmp.begin());
  std::fill(tmp.begin() + kNumLags12kHz + kConvolutionLength, tmp.end(), 0.f);
  fft_.ForwardTransform(*tmp_, X_.get(),             false);

  constexpr float kScalingFactor = 1.f / static_cast<float>(kFftFrameSize);
  std::fill(tmp.begin(), tmp.end(), 0.f);
  fft_.FrequencyDomainConvolve(*X_, *H_, tmp_.get(), kScalingFactor);
  fft_.BackwardTransform(*tmp_, tmp_.get(),             false);

  std::copy(tmp.begin() + kConvolutionLength - 1,
            tmp.begin() + kConvolutionLength + kNumLags12kHz - 1,
            auto_corr.begin());
}

}
}
