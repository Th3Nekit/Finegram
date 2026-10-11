/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_MEDIA_FACTORY_H_
#define PC_MEDIA_FACTORY_H_

#include <memory>

#include "api/environment/environment.h"
#include "call/call.h"
#include "call/call_config.h"
#include "media/base/media_engine.h"

namespace webrtc {

struct PeerConnectionFactoryDependencies;

class MediaFactory {
 public:
  virtual ~MediaFactory() = default;

  virtual std::unique_ptr<Call> CreateCall(const CallConfig& config) = 0;
  virtual std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      const Environment& env,
      PeerConnectionFactoryDependencies& dependencies) = 0;
};

}

#endif
