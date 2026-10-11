/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_BASE_H_
#define API_VOIP_VOIP_BASE_H_

#include "absl/base/attributes.h"
#include "absl/types/optional.h"

namespace webrtc {

class Transport;

enum class ChannelId : int {};

enum class ABSL_MUST_USE_RESULT VoipResult {

  kOk,

  kInvalidArgument,

  kFailedPrecondition,

  kInternal,
};

class VoipBase {
 public:

  virtual ChannelId CreateChannel(Transport* transport,
                                  absl::optional<uint32_t> local_ssrc) = 0;

  virtual VoipResult ReleaseChannel(ChannelId channel_id) = 0;

  virtual VoipResult StartSend(ChannelId channel_id) = 0;

  virtual VoipResult StopSend(ChannelId channel_id) = 0;

  virtual VoipResult StartPlayout(ChannelId channel_id) = 0;

  virtual VoipResult StopPlayout(ChannelId channel_id) = 0;

 protected:
  virtual ~VoipBase() = default;
};

}

#endif
