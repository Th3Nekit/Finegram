/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETWORK_STATE_PREDICTOR_H_
#define API_NETWORK_STATE_PREDICTOR_H_

#include <memory>
#include <vector>

namespace webrtc {

enum class BandwidthUsage {
  kBwNormal = 0,
  kBwUnderusing = 1,
  kBwOverusing = 2,
  kLast
};

class NetworkStatePredictor {
 public:
  virtual ~NetworkStatePredictor() {}

  virtual BandwidthUsage Update(int64_t send_time_ms,
                                int64_t arrival_time_ms,
                                BandwidthUsage network_state) = 0;
};

class NetworkStatePredictorFactoryInterface {
 public:
  virtual std::unique_ptr<NetworkStatePredictor>
  CreateNetworkStatePredictor() = 0;
  virtual ~NetworkStatePredictorFactoryInterface() = default;
};

}

#endif
