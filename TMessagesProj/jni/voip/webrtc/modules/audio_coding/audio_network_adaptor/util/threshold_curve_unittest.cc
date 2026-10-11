/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/audio_network_adaptor/util/threshold_curve.h"

#include <memory>

#include "test/gtest.h"

namespace webrtc {

namespace {
enum RelativePosition { kBelow, kOn, kAbove };

void CheckRelativePosition(const ThresholdCurve& curve,
                           ThresholdCurve::Point point,
                           RelativePosition pos) {
  RTC_CHECK(pos == kBelow || pos == kOn || pos == kAbove);

  EXPECT_EQ(pos == kBelow, curve.IsBelowCurve(point));
  EXPECT_EQ(pos == kAbove, curve.IsAboveCurve(point));
}
}

TEST(ThresholdCurveTest, PointPositionToCommonCurve) {

  constexpr ThresholdCurve::Point p1{1000, 2000};
  constexpr ThresholdCurve::Point p2{2000, 1000};

  RTC_CHECK_GT((p1.x + p2.x) / 2, p1.x);
  RTC_CHECK_LT((p1.x + p2.x) / 2, p2.x);
  RTC_CHECK_LT((p1.y + p2.y) / 2, p1.y);
  RTC_CHECK_GT((p1.y + p2.y) / 2, p2.y);

  const ThresholdCurve curve(p1, p2);

  {

    constexpr float x = p1.x - 1;
    CheckRelativePosition(curve, {x, p1.y + 1}, kBelow);
    CheckRelativePosition(curve, {x, p1.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, (p1.y + p2.y) / 2}, kBelow);
    CheckRelativePosition(curve, {x, p2.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, p2.y - 1}, kBelow);
  }

  {

    constexpr float x = p1.x;
    CheckRelativePosition(curve, {x, p1.y + 1}, kOn);
    CheckRelativePosition(curve, {x, p1.y + 0}, kOn);
    CheckRelativePosition(curve, {x, (p1.y + p2.y) / 2}, kBelow);
    CheckRelativePosition(curve, {x, p2.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, p2.y - 1}, kBelow);
  }

  {

    RTC_CHECK_LT(((p1.y + p2.y) / 2) + 1, p1.y);
    RTC_CHECK_LT(p2.y, ((p1.y + p2.y) / 2) - 1);

    constexpr float x = (p1.x + p2.x) / 2;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kAbove);
    CheckRelativePosition(curve, {x, ((p1.y + p2.y) / 2) + 1}, kAbove);
    CheckRelativePosition(curve, {x, (p1.y + p2.y) / 2}, kOn);
    CheckRelativePosition(curve, {x, ((p1.y + p2.y) / 2) - 1}, kBelow);
    CheckRelativePosition(curve, {x, p2.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, p2.y - 1}, kBelow);
  }

  {

    constexpr float x = p2.x;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kAbove);
    CheckRelativePosition(curve, {x, (p1.y + p2.y) / 2}, kAbove);
    CheckRelativePosition(curve, {x, p2.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p2.y - 1}, kBelow);
  }

  {

    constexpr float x = p2.x + 1;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kAbove);
    CheckRelativePosition(curve, {x, (p1.y + p2.y) / 2}, kAbove);
    CheckRelativePosition(curve, {x, p2.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p2.y - 1}, kBelow);
  }
}

TEST(ThresholdCurveTest, PointPositionToCurveWithHorizaontalSegment) {

  constexpr ThresholdCurve::Point p1{100, 200};
  constexpr ThresholdCurve::Point p2{p1.x + 1, p1.y};

  RTC_CHECK_GT((p1.x + p2.x) / 2, p1.x);
  RTC_CHECK_LT((p1.x + p2.x) / 2, p2.x);

  const ThresholdCurve curve(p1, p2);

  {

    constexpr float x = p1.x - 1;
    CheckRelativePosition(curve, {x, p1.y + 1}, kBelow);
    CheckRelativePosition(curve, {x, p1.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, p1.y - 1}, kBelow);
  }

  {

    constexpr float x = p1.x;
    CheckRelativePosition(curve, {x, p1.y + 1}, kOn);
    CheckRelativePosition(curve, {x, p1.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p1.y - 1}, kBelow);
  }

  {

    constexpr float x = (p1.x + p2.x) / 2;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p1.y - 1}, kBelow);
  }

  {

    constexpr float x = p2.x;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p1.y - 1}, kBelow);
  }

  {

    constexpr float x = p2.x + 1;
    CheckRelativePosition(curve, {x, p1.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p1.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p1.y - 1}, kBelow);
  }
}

TEST(ThresholdCurveTest, PointPositionToCurveWithVerticalSegment) {

  constexpr ThresholdCurve::Point p1{100, 200};
  constexpr ThresholdCurve::Point p2{p1.x, p1.y - 1};

  constexpr float left = p1.x - 1;
  constexpr float on = p1.x;
  constexpr float right = p1.x + 1;

  RTC_CHECK_LT((p1.y + p2.y) / 2, p1.y);
  RTC_CHECK_GT((p1.y + p2.y) / 2, p2.y);

  const ThresholdCurve curve(p1, p2);

  {

    constexpr float y = p1.y + 1;
    CheckRelativePosition(curve, {left, y}, kBelow);
    CheckRelativePosition(curve, {on, y}, kOn);
    CheckRelativePosition(curve, {right, y}, kAbove);
  }

  {

    constexpr float y = p1.y;
    CheckRelativePosition(curve, {left, y}, kBelow);
    CheckRelativePosition(curve, {on, y}, kOn);
    CheckRelativePosition(curve, {right, y}, kAbove);
  }

  {

    constexpr float y = (p1.y + p2.y) / 2;
    CheckRelativePosition(curve, {left, y}, kBelow);
    CheckRelativePosition(curve, {on, y}, kOn);
    CheckRelativePosition(curve, {right, y}, kAbove);
  }

  {

    constexpr float y = p2.y;
    CheckRelativePosition(curve, {left, y}, kBelow);
    CheckRelativePosition(curve, {on, y}, kOn);
    CheckRelativePosition(curve, {right, y}, kOn);
  }

  {

    constexpr float y = p2.y - 1;
    CheckRelativePosition(curve, {left, y}, kBelow);
    CheckRelativePosition(curve, {on, y}, kBelow);
    CheckRelativePosition(curve, {right, y}, kBelow);
  }
}

TEST(ThresholdCurveTest, PointPositionCurveWithNullSegment) {

  constexpr ThresholdCurve::Point p{100, 200};

  const ThresholdCurve curve(p, p);

  {

    constexpr float x = p.x - 1;
    CheckRelativePosition(curve, {x, p.y + 1}, kBelow);
    CheckRelativePosition(curve, {x, p.y + 0}, kBelow);
    CheckRelativePosition(curve, {x, p.y - 1}, kBelow);
  }

  {

    constexpr float x = p.x + 0;
    CheckRelativePosition(curve, {x, p.y + 1}, kOn);
    CheckRelativePosition(curve, {x, p.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p.y - 1}, kBelow);
  }

  {

    constexpr float x = p.x + 1;
    CheckRelativePosition(curve, {x, p.y + 1}, kAbove);
    CheckRelativePosition(curve, {x, p.y + 0}, kOn);
    CheckRelativePosition(curve, {x, p.y - 1}, kBelow);
  }
}

TEST(ThresholdCurveTest, TwoCurvesSegmentHasSameProjectionAxisX) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{c1_left.x, c1_left.y + 20};
  constexpr ThresholdCurve::Point c2_right{c1_right.x, c1_right.y + 10};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_TRUE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, TwoCurvesSegmentOfHigherSubsetProjectionAxisX) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{6, 11};
  constexpr ThresholdCurve::Point c2_right{9, 7};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_TRUE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest,
     TwoCurvesRightPointOfHigherCurveAboveHorizontalRayOfLower) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{c1_left.x, c1_left.y + 1};
  constexpr ThresholdCurve::Point c2_right{c1_right.x + 1, c1_right.y + 1};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_TRUE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, TwoCurvesPointsOfHigherOnRaysOfLower) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{c1_left.x, c1_left.y + 2};
  constexpr ThresholdCurve::Point c2_right{c1_right.x + 3, c1_right.y};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_TRUE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, SecondCurveCrossesVerticalRayOfFirstCurve) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{c1_left.x - 1, c1_left.y + 1};
  constexpr ThresholdCurve::Point c2_right{c1_right.x, c1_right.y + 1};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_FALSE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, SecondCurveCrossesHorizontalRayOfFirstCurve) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{c1_left.x, c1_left.y + 1};
  constexpr ThresholdCurve::Point c2_right{c1_right.x + 2, c1_right.y - 1};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_FALSE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, TwoCurvesWithCrossingSegments) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_right);

  constexpr ThresholdCurve::Point c2_left{4, 9};
  constexpr ThresholdCurve::Point c2_right{10, 6};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  RTC_CHECK(!c1_curve.IsAboveCurve({8, 7}));
  RTC_CHECK(!c1_curve.IsBelowCurve({8, 7}));
  RTC_CHECK(!c2_curve.IsAboveCurve({8, 7}));
  RTC_CHECK(!c2_curve.IsBelowCurve({8, 7}));

  EXPECT_FALSE(c1_curve <= c2_curve);
  EXPECT_FALSE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, IdenticalCurves) {

  constexpr ThresholdCurve::Point left{5, 10};
  constexpr ThresholdCurve::Point right{10, 5};

  const ThresholdCurve c1_curve(left, right);
  const ThresholdCurve c2_curve(left, right);

  EXPECT_TRUE(c1_curve <= c2_curve);
  EXPECT_TRUE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, NearlyIdenticalCurvesSecondContinuesOnOtherLeftSide) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_left);

  constexpr ThresholdCurve::Point c2_left{c1_left.x - 1, c1_left.y + 1};
  constexpr ThresholdCurve::Point c2_right = c1_right;
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_FALSE(c1_curve <= c2_curve);
  EXPECT_TRUE(c2_curve <= c1_curve);
}

TEST(ThresholdCurveTest, NearlyIdenticalCurvesSecondContinuesOnOtherRightSide) {

  constexpr ThresholdCurve::Point c1_left{5, 10};
  constexpr ThresholdCurve::Point c1_right{10, 5};
  const ThresholdCurve c1_curve(c1_left, c1_left);

  constexpr ThresholdCurve::Point c2_left = c1_left;
  constexpr ThresholdCurve::Point c2_right{c1_right.x + 1, c1_right.y - 1};
  const ThresholdCurve c2_curve(c2_left, c2_right);

  EXPECT_FALSE(c1_curve <= c2_curve);
  EXPECT_TRUE(c2_curve <= c1_curve);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(ThresholdCurveDeathTest, WrongOrderPoints) {
  std::unique_ptr<ThresholdCurve> curve;
  constexpr ThresholdCurve::Point left{5, 10};
  constexpr ThresholdCurve::Point right{10, 5};
  EXPECT_DEATH(curve.reset(new ThresholdCurve(right, left)), "");
}
#endif

}
