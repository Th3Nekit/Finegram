/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_EXP_FILTER_H_
#define RTC_BASE_NUMERICS_EXP_FILTER_H_

namespace rtc {

class ExpFilter {
 public:
  static const float kValueUndefined;

  explicit ExpFilter(float alpha, float max = kValueUndefined) : max_(max) {
    Reset(alpha);
  }

  void Reset(float alpha);

  float Apply(float exp, float sample);

  float filtered() const { return filtered_; }

  void UpdateBase(float alpha);

 private:
  float alpha_;
  float filtered_;
  const float max_;
};
}

#endif
