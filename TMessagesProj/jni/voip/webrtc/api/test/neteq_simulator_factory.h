/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_FACTORY_H_
#define API_TEST_NETEQ_SIMULATOR_FACTORY_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/neteq/neteq_factory.h"
#include "api/test/neteq_simulator.h"

namespace webrtc {
namespace test {

class NetEqTestFactory;

class NetEqSimulatorFactory {
 public:
  NetEqSimulatorFactory();
  ~NetEqSimulatorFactory();
  struct Config {

    int max_nr_packets_in_buffer = 0;

    int initial_dummy_packets = 0;

    int skip_get_audio_events = 0;

    std::string field_trial_string;

    absl::optional<std::string> output_audio_filename;

    absl::optional<std::string> python_plot_filename;

    absl::optional<std::string> text_log_filename;

    NetEqFactory* neteq_factory = nullptr;
  };
  std::unique_ptr<NetEqSimulator> CreateSimulatorFromFile(
      absl::string_view event_log_filename,
      absl::string_view replacement_audio_filename,
      Config simulation_config);

  std::unique_ptr<NetEqSimulator> CreateSimulatorFromString(
      absl::string_view event_log_file_contents,
      absl::string_view replacement_audio_file,
      Config simulation_config);

 private:
  std::unique_ptr<NetEqTestFactory> factory_;
};

}
}

#endif
