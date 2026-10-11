/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MDNS_RESPONDER_INTERFACE_H_
#define RTC_BASE_MDNS_RESPONDER_INTERFACE_H_

#include <functional>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/ip_address.h"

namespace webrtc {

class MdnsResponderInterface {
 public:
  using NameCreatedCallback =
      std::function<void(const rtc::IPAddress&, absl::string_view)>;
  using NameRemovedCallback = std::function<void(bool)>;

  MdnsResponderInterface() = default;
  virtual ~MdnsResponderInterface() = default;

  virtual void CreateNameForAddress(const rtc::IPAddress& addr,
                                    NameCreatedCallback callback) = 0;

  virtual void RemoveNameForAddress(const rtc::IPAddress& addr,
                                    NameRemovedCallback callback) = 0;
};

}

#endif
