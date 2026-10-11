/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_COMMON_MATH_H_
#define NET_DCSCTP_COMMON_MATH_H_

namespace dcsctp {

template <typename IntType>
IntType RoundUpTo4(IntType val) {
  return (val + 3) & ~3;
}

template <typename IntType>
IntType RoundDownTo4(IntType val) {
  return val & ~3;
}

template <typename IntType>
bool IsDivisibleBy4(IntType val) {
  return (val & 3) == 0;
}

}

#endif
