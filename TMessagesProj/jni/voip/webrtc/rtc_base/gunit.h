/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_GUNIT_H_
#define RTC_BASE_GUNIT_H_

#include "absl/strings/string_view.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

#define WAIT(ex, timeout)                                       \
  for (int64_t start = rtc::SystemTimeMillis();                 \
       !(ex) && rtc::SystemTimeMillis() < start + (timeout);) { \
    rtc::Thread::Current()->ProcessMessages(0);                 \
    rtc::Thread::Current()->SleepMs(1);                         \
  }

#define WAIT_(ex, timeout, res)                                   \
  do {                                                            \
    int64_t start = rtc::SystemTimeMillis();                      \
    res = (ex) && true;                                           \
    while (!res && rtc::SystemTimeMillis() < start + (timeout)) { \
      rtc::Thread::Current()->ProcessMessages(0);                 \
      rtc::Thread::Current()->SleepMs(1);                         \
      res = (ex) && true;                                         \
    }                                                             \
  } while (0)

#define EXPECT_TRUE_WAIT(ex, timeout)                   \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                         \
  if (bool res = true) {                                \
    WAIT_(ex, timeout, res);                            \
    if (!res)                                           \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__); \
  } else                                                \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : EXPECT_TRUE(ex)

#define EXPECT_EQ_WAIT(v1, v2, timeout)                 \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                         \
  if (bool res = true) {                                \
    WAIT_(v1 == v2, timeout, res);                      \
    if (!res)                                           \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__); \
  } else                                                \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : EXPECT_EQ(v1, v2)

#define ASSERT_TRUE_WAIT(ex, timeout)                   \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                         \
  if (bool res = true) {                                \
    WAIT_(ex, timeout, res);                            \
    if (!res)                                           \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__); \
  } else                                                \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : ASSERT_TRUE(ex)

#define ASSERT_EQ_WAIT(v1, v2, timeout)                 \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                         \
  if (bool res = true) {                                \
    WAIT_(v1 == v2, timeout, res);                      \
    if (!res)                                           \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__); \
  } else                                                \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : ASSERT_EQ(v1, v2)

#define EXPECT_TRUE_WAIT_MARGIN(ex, timeout, margin)                           \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                                \
  if (bool res = true) {                                                       \
    WAIT_(ex, timeout, res);                                                   \
    if (res)                                                                   \
      break;                                                                   \
    RTC_LOG(LS_WARNING) << "Expression " << #ex << " still not true after "    \
                        << (timeout) << "ms; waiting an additional " << margin \
                        << "ms";                                               \
    WAIT_(ex, margin, res);                                                    \
    if (!res)                                                                  \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__);                        \
  } else                                                                       \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : EXPECT_TRUE(ex)

#define SIMULATED_WAIT(ex, timeout, clock)                \
  for (int64_t start = rtc::TimeMillis();                 \
       !(ex) && rtc::TimeMillis() < start + (timeout);) { \
    (clock).AdvanceTime(webrtc::TimeDelta::Millis(1));    \
  }

#define SIMULATED_WAIT_(ex, timeout, res, clock)            \
  do {                                                      \
    int64_t start = rtc::TimeMillis();                      \
    res = (ex);                                             \
    while (!res && rtc::TimeMillis() < start + (timeout)) { \
      (clock).AdvanceTime(webrtc::TimeDelta::Millis(1));    \
      res = (ex);                                           \
    }                                                       \
  } while (0)

#define EXPECT_TRUE_SIMULATED_WAIT(ex, timeout, clock) \
  do {                                                 \
    bool res;                                          \
    SIMULATED_WAIT_(ex, timeout, res, clock);          \
    if (!res) {                                        \
      EXPECT_TRUE(ex);                                 \
    }                                                  \
  } while (0)

#define EXPECT_EQ_SIMULATED_WAIT(v1, v2, timeout, clock) \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                          \
  if (bool res = true) {                                 \
    SIMULATED_WAIT_(v1 == v2, timeout, res, clock);      \
    if (!res)                                            \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__);  \
  } else                                                 \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : EXPECT_EQ(v1, v2)

#define ASSERT_TRUE_SIMULATED_WAIT(ex, timeout, clock)  \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                         \
  if (bool res = true) {                                \
    SIMULATED_WAIT_(ex, timeout, res, clock);           \
    if (!res)                                           \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__); \
  } else                                                \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : ASSERT_TRUE(ex)

#define ASSERT_EQ_SIMULATED_WAIT(v1, v2, timeout, clock) \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                          \
  if (bool res = true) {                                 \
    SIMULATED_WAIT_(v1 == v2, timeout, res, clock);      \
    if (!res)                                            \
      goto GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__);  \
  } else                                                 \
    GTEST_CONCAT_TOKEN_(gunit_label_, __LINE__) : ASSERT_EQ(v1, v2)

#endif
