/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_STATS_OBSERVER_INTERFACE_H_
#define API_TEST_STATS_OBSERVER_INTERFACE_H_

#include "absl/strings/string_view.h"
#include "api/stats/rtc_stats_report.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class StatsObserverInterface {
 public:
  virtual ~StatsObserverInterface() = default;

  virtual void OnStatsReports(
      absl::string_view pc_label,
      const rtc::scoped_refptr<const RTCStatsReport>& report) = 0;
};

}
}

#endif
