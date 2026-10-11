/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_BITRATE_CONFIGURATOR_H_
#define CALL_RTP_BITRATE_CONFIGURATOR_H_

#include "absl/types/optional.h"
#include "api/transport/bitrate_settings.h"
#include "api/units/data_rate.h"

namespace webrtc {

class RtpBitrateConfigurator {
 public:
  explicit RtpBitrateConfigurator(const BitrateConstraints& bitrate_config);
  ~RtpBitrateConfigurator();

  RtpBitrateConfigurator(const RtpBitrateConfigurator&) = delete;
  RtpBitrateConfigurator& operator=(const RtpBitrateConfigurator&) = delete;

  BitrateConstraints GetConfig() const;

  absl::optional<BitrateConstraints> UpdateWithSdpParameters(
      const BitrateConstraints& bitrate_config_);

  absl::optional<BitrateConstraints> UpdateWithClientPreferences(
      const BitrateSettings& bitrate_mask);

  absl::optional<BitrateConstraints> UpdateWithRelayCap(DataRate cap);

 private:

  absl::optional<BitrateConstraints> UpdateConstraints(
      const absl::optional<int>& new_start);

  BitrateConstraints bitrate_config_;

  BitrateSettings bitrate_config_mask_;

  BitrateConstraints base_bitrate_config_;

  DataRate max_bitrate_over_relay_ = DataRate::PlusInfinity();
};
}

#endif
