/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_UNITTEST_HELPER_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_DELAY_BASED_BWE_UNITTEST_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/transport/field_trial_based_config.h"
#include "api/transport/network_types.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

class TestBitrateObserver {
 public:
  TestBitrateObserver() : updated_(false), latest_bitrate_(0) {}
  ~TestBitrateObserver() {}

  void OnReceiveBitrateChanged(uint32_t bitrate);

  void Reset() { updated_ = false; }

  bool updated() const { return updated_; }

  uint32_t latest_bitrate() const { return latest_bitrate_; }

 private:
  bool updated_;
  uint32_t latest_bitrate_;
};

class RtpStream {
 public:
  enum { kSendSideOffsetUs = 1000000 };

  RtpStream(int fps, int bitrate_bps);

  RtpStream(const RtpStream&) = delete;
  RtpStream& operator=(const RtpStream&) = delete;

  int64_t GenerateFrame(int64_t time_now_us,
                        int64_t* next_sequence_number,
                        std::vector<PacketResult>* packets);

  int64_t next_rtp_time() const;

  void set_bitrate_bps(int bitrate_bps);

  int bitrate_bps() const;

  static bool Compare(const std::unique_ptr<RtpStream>& lhs,
                      const std::unique_ptr<RtpStream>& rhs);

 private:
  int fps_;
  int bitrate_bps_;
  int64_t next_rtp_time_;
};

class StreamGenerator {
 public:
  StreamGenerator(int capacity, int64_t time_now);
  ~StreamGenerator();

  StreamGenerator(const StreamGenerator&) = delete;
  StreamGenerator& operator=(const StreamGenerator&) = delete;

  void AddStream(RtpStream* stream);

  void set_capacity_bps(int capacity_bps);

  void SetBitrateBps(int bitrate_bps);

  void set_rtp_timestamp_offset(uint32_t ssrc, uint32_t offset);

  int64_t GenerateFrame(int64_t time_now_us,
                        int64_t* next_sequence_number,
                        std::vector<PacketResult>* packets);

 private:

  int capacity_;

  int64_t prev_arrival_time_us_;

  std::vector<std::unique_ptr<RtpStream>> streams_;
};
}

class DelayBasedBweTest : public ::testing::Test {
 public:
  DelayBasedBweTest();
  ~DelayBasedBweTest() override;

 protected:
  void AddDefaultStream();

  void IncomingFeedback(int64_t arrival_time_ms,
                        int64_t send_time_ms,
                        size_t payload_size);
  void IncomingFeedback(int64_t arrival_time_ms,
                        int64_t send_time_ms,
                        size_t payload_size,
                        const PacedPacketInfo& pacing_info);
  void IncomingFeedback(Timestamp receive_time,
                        Timestamp send_time,
                        size_t payload_size,
                        const PacedPacketInfo& pacing_info);

  bool GenerateAndProcessFrame(uint32_t ssrc, uint32_t bitrate_bps);

  uint32_t SteadyStateRun(uint32_t ssrc,
                          int number_of_frames,
                          uint32_t start_bitrate,
                          uint32_t min_bitrate,
                          uint32_t max_bitrate,
                          uint32_t target_bitrate);

  void TestTimestampGroupingTestHelper();

  void TestWrappingHelper(int silence_time_s);

  void InitialBehaviorTestHelper(uint32_t expected_converge_bitrate);
  void RateIncreaseReorderingTestHelper(uint32_t expected_bitrate);
  void RateIncreaseRtpTimestampsTestHelper(int expected_iterations);
  void CapacityDropTestHelper(int number_of_streams,
                              bool wrap_time_stamp,
                              uint32_t expected_bitrate_drop_delta,
                              int64_t receiver_clock_offset_change_ms);

  static const uint32_t kDefaultSsrc;
  FieldTrialBasedConfig field_trial_config_;

  std::unique_ptr<test::ScopedFieldTrials>
      field_trial;
  SimulatedClock clock_;
  test::TestBitrateObserver bitrate_observer_;
  std::unique_ptr<AcknowledgedBitrateEstimatorInterface>
      acknowledged_bitrate_estimator_;
  const std::unique_ptr<ProbeBitrateEstimator> probe_bitrate_estimator_;
  std::unique_ptr<DelayBasedBwe> bitrate_estimator_;
  std::unique_ptr<test::StreamGenerator> stream_generator_;
  int64_t arrival_time_offset_ms_;
  int64_t next_sequence_number_;
  bool first_update_;
};

}

#endif
