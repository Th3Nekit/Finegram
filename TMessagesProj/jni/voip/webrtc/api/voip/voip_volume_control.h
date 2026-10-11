/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_VOLUME_CONTROL_H_
#define API_VOIP_VOIP_VOLUME_CONTROL_H_

#include "api/voip/voip_base.h"

namespace webrtc {

struct VolumeInfo {

  double audio_level = 0;

  double total_energy = 0.0;

  double total_duration = 0.0;
};

class VoipVolumeControl {
 public:

  virtual VoipResult SetInputMuted(ChannelId channel_id, bool enable) = 0;

  virtual VoipResult GetInputVolumeInfo(ChannelId channel_id,
                                        VolumeInfo& volume_info) = 0;

  virtual VoipResult GetOutputVolumeInfo(ChannelId channel_id,
                                         VolumeInfo& volume_info) = 0;

 protected:
  virtual ~VoipVolumeControl() = default;
};

}

#endif
