/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_AEC3_FFT_H_
#define MODULES_AUDIO_PROCESSING_AEC3_AEC3_FFT_H_

#include <array>

#include "api/array_view.h"
#include "common_audio/third_party/ooura/fft_size_128/ooura_fft.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/checks.h"

namespace webrtc {

class Aec3Fft {
 public:
  enum class Window { kRectangular, kHanning, kSqrtHanning };

  Aec3Fft();

  Aec3Fft(const Aec3Fft&) = delete;
  Aec3Fft& operator=(const Aec3Fft&) = delete;

  void Fft(std::array<float, kFftLength>* x, FftData* X) const {
    RTC_DCHECK(x);
    RTC_DCHECK(X);
    ooura_fft_.Fft(x->data());
    X->CopyFromPackedArray(*x);
  }

  void Ifft(const FftData& X, std::array<float, kFftLength>* x) const {
    RTC_DCHECK(x);
    X.CopyToPackedArray(x);
    ooura_fft_.InverseFft(x->data());
  }

  void ZeroPaddedFft(rtc::ArrayView<const float> x,
                     Window window,
                     FftData* X) const;

  void PaddedFft(rtc::ArrayView<const float> x,
                 rtc::ArrayView<const float> x_old,
                 FftData* X) const {
    PaddedFft(x, x_old, Window::kRectangular, X);
  }

  void PaddedFft(rtc::ArrayView<const float> x,
                 rtc::ArrayView<const float> x_old,
                 Window window,
                 FftData* X) const;

 private:
  const OouraFft ooura_fft_;
};

}

#endif
