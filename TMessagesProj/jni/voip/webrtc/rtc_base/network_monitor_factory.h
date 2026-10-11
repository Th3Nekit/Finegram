/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_MONITOR_FACTORY_H_
#define RTC_BASE_NETWORK_MONITOR_FACTORY_H_

namespace webrtc {
class FieldTrialsView;
}

namespace rtc {

class NetworkMonitorInterface;

class NetworkMonitorFactory {
 public:
  virtual NetworkMonitorInterface* CreateNetworkMonitor(
      const webrtc::FieldTrialsView& field_trials) = 0;

  virtual ~NetworkMonitorFactory();

 protected:
  NetworkMonitorFactory();
};

}

#endif
