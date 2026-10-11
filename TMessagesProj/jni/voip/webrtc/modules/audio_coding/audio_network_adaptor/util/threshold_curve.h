/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_UTIL_THRESHOLD_CURVE_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_UTIL_THRESHOLD_CURVE_H_

#include "rtc_base/checks.h"

namespace webrtc {

class ThresholdCurve {
 public:
  struct Point {
    constexpr Point(float x, float y) : x(x), y(y) {}
    float x;
    float y;
  };

  ThresholdCurve(const Point& left, const Point& right)
      : a(GetPoint(left, right, true)),
        b(GetPoint(left, right, false)),
        slope(b.x - a.x == 0.0f ? 0.0f : (b.y - a.y) / (b.x - a.x)),
        offset(a.y - slope * a.x) {

  }

  ThresholdCurve(float a_x, float a_y, float b_x, float b_y)
      : ThresholdCurve(Point{a_x, a_y}, Point{b_x, b_y}) {}

  bool IsBelowCurve(const Point& p) const {
    if (p.x < a.x) {
      return true;
    } else if (p.x == a.x) {

      return p.y < a.y;
    } else if (a.x < p.x && p.x < b.x) {
      return p.y < offset + slope * p.x;
    } else {
      return p.y < b.y;
    }
  }

  bool IsAboveCurve(const Point& p) const {
    if (p.x <= a.x) {
      return false;
    } else if (a.x < p.x && p.x < b.x) {
      return p.y > offset + slope * p.x;
    } else {
      return p.y > b.y;
    }
  }

  bool operator<=(const ThresholdCurve& rhs) const {

    return !IsBelowCurve(rhs.a) && !IsBelowCurve(rhs.b) &&
           !rhs.IsAboveCurve(a) && !rhs.IsAboveCurve(b);
  }

 private:
  static const Point& GetPoint(const Point& left,
                               const Point& right,
                               bool is_for_left) {
    RTC_DCHECK_LE(left.x, right.x);
    RTC_DCHECK_GE(left.y, right.y);

    if (left.x == right.x) {
      return right;
    } else if (left.y == right.y) {
      return left;
    }

    return is_for_left ? left : right;
  }

  const Point a;
  const Point b;
  const float slope;
  const float offset;
};

}

#endif
