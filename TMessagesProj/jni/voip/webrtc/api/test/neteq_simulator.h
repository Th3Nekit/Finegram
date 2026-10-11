/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_H_
#define API_TEST_NETEQ_SIMULATOR_H_

#include <stdint.h>

#include <map>
#include <vector>

namespace webrtc {
namespace test {

class NetEqSimulator {
 public:
  virtual ~NetEqSimulator() = default;

  enum class Action { kNormal, kExpand, kAccelerate, kPreemptiveExpand };

  struct SimulationStepResult {
    SimulationStepResult();
    SimulationStepResult(const SimulationStepResult& other);
    ~SimulationStepResult();

    bool is_simulation_finished = false;

    std::map<Action, int> action_times_ms;

    int64_t simulation_step_ms = 0;
  };

  struct NetEqState {
    NetEqState();
    NetEqState(const NetEqState& other);
    ~NetEqState();

    int current_delay_ms = 0;

    bool packet_loss_occurred = false;

    bool packet_buffer_flushed = false;

    bool next_packet_available = false;

    std::vector<int> packet_iat_ms;

    int packet_size_ms = 0;
  };

  virtual int64_t Run() = 0;

  virtual SimulationStepResult RunToNextGetAudio() = 0;

  virtual void SetNextAction(Action next_operation) = 0;

  virtual NetEqState GetNetEqState() = 0;
};

}
}

#endif
