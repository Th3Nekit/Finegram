/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_DETECTOR_NORMALIZED_COVARIANCE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_ECHO_DETECTOR_NORMALIZED_COVARIANCE_ESTIMATOR_H_

namespace webrtc {

class NormalizedCovarianceEstimator {
 public:
  void Update(float x,
              float x_mean,
              float x_var,
              float y,
              float y_mean,
              float y_var);

  float normalized_cross_correlation() const {
    return normalized_cross_correlation_;
  }
  float covariance() const { return covariance_; }

  void Clear();

 private:
  float normalized_cross_correlation_ = 0.f;

  float covariance_ = 0.f;
};

}

#endif
