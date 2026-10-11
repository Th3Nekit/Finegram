/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONFIG_H_
#define CALL_RTP_TRANSPORT_CONFIG_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/environment/environment.h"
#include "api/network_state_predictor.h"
#include "api/transport/bitrate_settings.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"

namespace webrtc {

struct RtpTransportConfig {
  Environment env;

  BitrateConstraints bitrate_config;

  NetworkStatePredictorFactoryInterface* network_state_predictor_factory =
      nullptr;

  NetworkControllerFactoryInterface* network_controller_factory = nullptr;

  absl::optional<TimeDelta> pacer_burst_interval;
};
}

#endif
