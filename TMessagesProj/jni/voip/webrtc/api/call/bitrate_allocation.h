/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_CALL_BITRATE_ALLOCATION_H_
#define API_CALL_BITRATE_ALLOCATION_H_

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"

namespace webrtc {

struct BitrateAllocationUpdate {

  DataRate target_bitrate = DataRate::Zero();

  DataRate stable_target_bitrate = DataRate::Zero();

  double packet_loss_ratio = 0;

  TimeDelta round_trip_time = TimeDelta::PlusInfinity();

  TimeDelta bwe_period = TimeDelta::PlusInfinity();

  double cwnd_reduce_ratio = 0;
};

}

#endif
