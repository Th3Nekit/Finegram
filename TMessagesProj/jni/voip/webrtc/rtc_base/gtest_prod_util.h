/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_GTEST_PROD_UTIL_H_
#define RTC_BASE_GTEST_PROD_UTIL_H_

#define FRIEND_TEST_WEBRTC(test_case_name, test_name) \
  friend class test_case_name##_##test_name##_Test

#define FRIEND_TEST_ALL_PREFIXES(test_case_name, test_name) \
  FRIEND_TEST_WEBRTC(test_case_name, test_name);            \
  FRIEND_TEST_WEBRTC(test_case_name, DISABLED_##test_name); \
  FRIEND_TEST_WEBRTC(test_case_name, FLAKY_##test_name);    \
  FRIEND_TEST_WEBRTC(test_case_name, FAILS_##test_name)

#endif
