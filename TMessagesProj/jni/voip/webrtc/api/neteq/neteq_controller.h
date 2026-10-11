/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NETEQ_NETEQ_CONTROLLER_H_
#define API_NETEQ_NETEQ_CONTROLLER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

#include "absl/types/optional.h"
#include "api/neteq/neteq.h"
#include "api/neteq/tick_timer.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class NetEqController {
 public:

  struct Config {
    bool allow_time_stretching;
    bool enable_rtx_handling;
    int max_packets_in_buffer;
    int base_min_delay_ms;
    TickTimer* tick_timer;
    webrtc::Clock* clock = nullptr;
  };

  struct PacketInfo {
    uint32_t timestamp;
    bool is_dtx;
    bool is_cng;
  };

  struct PacketBufferInfo {
    bool dtx_or_cng;
    size_t num_samples;
    size_t span_samples;
    size_t span_samples_wait_time;
    size_t num_packets;
  };

  struct NetEqStatus {
    uint32_t target_timestamp;
    int16_t expand_mutefactor;
    size_t last_packet_samples;
    absl::optional<PacketInfo> next_packet;
    NetEq::Mode last_mode;
    bool play_dtmf;
    size_t generated_noise_samples;
    PacketBufferInfo packet_buffer_info;
    size_t sync_buffer_samples;
  };

  struct PacketArrivedInfo {
    size_t packet_length_samples;
    uint32_t main_timestamp;
    uint16_t main_sequence_number;
    bool is_cng_or_dtmf;
    bool is_dtx;
    bool buffer_flush;
  };

  virtual ~NetEqController() = default;

  virtual void Reset() = 0;

  virtual void SoftReset() = 0;

  virtual NetEq::Operation GetDecision(const NetEqStatus& status,
                                       bool* reset_decoder) = 0;

  virtual void RegisterEmptyPacket() = 0;

  virtual void SetSampleRate(int fs_hz, size_t output_size_samples) = 0;

  virtual bool SetMaximumDelay(int delay_ms) = 0;
  virtual bool SetMinimumDelay(int delay_ms) = 0;

  virtual bool SetBaseMinimumDelay(int delay_ms) = 0;
  virtual int GetBaseMinimumDelay() const = 0;

  virtual void ExpandDecision(NetEq::Operation operation) = 0;

  virtual void AddSampleMemory(int32_t value) = 0;

  virtual int TargetLevelMs() const = 0;

  virtual int UnlimitedTargetLevelMs() const { return 0; }

  virtual absl::optional<int> PacketArrived(int fs_hz,
                                            bool should_update_stats,
                                            const PacketArrivedInfo& info) = 0;

  virtual void NotifyMutedState() {}

  virtual bool PeakFound() const = 0;

  virtual int GetFilteredBufferLevel() const = 0;

  virtual void set_sample_memory(int32_t value) = 0;
  virtual size_t noise_fast_forward() const = 0;
  virtual size_t packet_length_samples() const = 0;
  virtual void set_packet_length_samples(size_t value) = 0;
  virtual void set_prev_time_scale(bool value) = 0;
};

}
#endif
