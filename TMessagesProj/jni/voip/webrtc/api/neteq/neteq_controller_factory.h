/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_NETEQ_CONTROLLER_FACTORY_H_
#define API_NETEQ_NETEQ_CONTROLLER_FACTORY_H_

#include <memory>

#include "api/neteq/neteq_controller.h"

namespace webrtc {

class NetEqControllerFactory {
 public:
  virtual ~NetEqControllerFactory() = default;

  virtual std::unique_ptr<NetEqController> CreateNetEqController(
      const NetEqController::Config& config) const = 0;
};

}
#endif
