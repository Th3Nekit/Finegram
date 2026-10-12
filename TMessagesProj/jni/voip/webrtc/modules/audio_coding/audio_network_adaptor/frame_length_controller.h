/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FRAME_LENGTH_CONTROLLER_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FRAME_LENGTH_CONTROLLER_H_

#include <stddef.h>

#include <map>
#include <set>

#include "absl/types/optional.h"
#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"

namespace webrtc {

class FrameLengthController final : public Controller {
 public:
  struct Config {
    struct FrameLengthChange {
      FrameLengthChange(int from_frame_length_ms, int to_frame_length_ms);
      bool operator<(const FrameLengthChange& rhs) const;
      int from_frame_length_ms;
      int to_frame_length_ms;
    };
    Config(const std::set<int>& encoder_frame_lengths_ms,
           int initial_frame_length_ms,
           int min_encoder_bitrate_bps,
           float fl_increasing_packet_loss_fraction,
           float fl_decreasing_packet_loss_fraction,
           int fl_increase_overhead_offset,
           int fl_decrease_overhead_offset,
           std::map<FrameLengthChange, int> fl_changing_bandwidths_bps);
    Config(const Config& other);
    ~Config();
    std::set<int> encoder_frame_lengths_ms;
    int initial_frame_length_ms;
    int min_encoder_bitrate_bps;

    float fl_increasing_packet_loss_fraction;

    float fl_decreasing_packet_loss_fraction;

    int fl_increase_overhead_offset;

    int fl_decrease_overhead_offset;
    std::map<FrameLengthChange, int> fl_changing_bandwidths_bps;
  };

  explicit FrameLengthController(const Config& config);

  ~FrameLengthController() override;

  FrameLengthController(const FrameLengthController&) = delete;
  FrameLengthController& operator=(const FrameLengthController&) = delete;

  void UpdateNetworkMetrics(const NetworkMetrics& network_metrics) override;

  void MakeDecision(AudioEncoderRuntimeConfig* config) override;

 private:
  bool FrameLengthIncreasingDecision(const AudioEncoderRuntimeConfig& config);

  bool FrameLengthDecreasingDecision(const AudioEncoderRuntimeConfig& config);

  const Config config_;

  std::set<int>::const_iterator frame_length_ms_;

  absl::optional<int> uplink_bandwidth_bps_;

  absl::optional<float> uplink_packet_loss_fraction_;

  absl::optional<size_t> overhead_bytes_per_packet_;

  bool prev_decision_increase_ = false;
};

}

#endif
