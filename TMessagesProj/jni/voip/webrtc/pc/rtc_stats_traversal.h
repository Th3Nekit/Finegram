/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTC_STATS_TRAVERSAL_H_
#define PC_RTC_STATS_TRAVERSAL_H_

#include <string>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats.h"
#include "api/stats/rtc_stats_report.h"

namespace webrtc {

rtc::scoped_refptr<RTCStatsReport> TakeReferencedStats(
    rtc::scoped_refptr<RTCStatsReport> report,
    const std::vector<std::string>& ids);

std::vector<const std::string*> GetStatsReferencedIds(const RTCStats& stats);

}

#endif
