/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NAT_TYPES_H_
#define RTC_BASE_NAT_TYPES_H_

namespace rtc {

enum NATType {
  NAT_OPEN_CONE,
  NAT_ADDR_RESTRICTED,
  NAT_PORT_RESTRICTED,
  NAT_SYMMETRIC
};

class NAT {
 public:
  virtual ~NAT() {}

  virtual bool IsSymmetric() = 0;

  virtual bool FiltersIP() = 0;

  virtual bool FiltersPort() = 0;

  static NAT* Create(NATType type);
};

}

#endif
