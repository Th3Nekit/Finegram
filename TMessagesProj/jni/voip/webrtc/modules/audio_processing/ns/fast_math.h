/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_NS_FAST_MATH_H_
#define MODULES_AUDIO_PROCESSING_NS_FAST_MATH_H_

#include "api/array_view.h"

namespace webrtc {

float SqrtFastApproximation(float f);

float LogApproximation(float x);
void LogApproximation(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);

float Pow2Approximation(float p);

float PowApproximation(float x, float p);

float ExpApproximation(float x);
void ExpApproximation(rtc::ArrayView<const float> x, rtc::ArrayView<float> y);
void ExpApproximationSignFlip(rtc::ArrayView<const float> x,
                              rtc::ArrayView<float> y);
}

#endif
