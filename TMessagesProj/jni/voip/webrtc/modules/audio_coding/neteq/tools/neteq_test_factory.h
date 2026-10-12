/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "test/field_trial.h"

namespace webrtc {
namespace test {

class SsrcSwitchDetector;
class NetEqStatsGetter;
class NetEqStatsPlotter;

class NetEqTestFactory {
 public:
  NetEqTestFactory();
  ~NetEqTestFactory();
  struct Config {
    Config();
    Config(const Config& other);
    ~Config();

    static constexpr int default_pcmu() { return 0; }
    int pcmu = default_pcmu();

    static constexpr int default_pcma() { return 8; }
    int pcma = default_pcma();

    static constexpr int default_ilbc() { return 102; }
    int ilbc = default_ilbc();

    static constexpr int default_isac() { return 103; }
    int isac = default_isac();

    static constexpr int default_isac_swb() { return 104; }
    int isac_swb = default_isac_swb();

    static constexpr int default_opus() { return 111; }
    int opus = default_opus();

    static constexpr int default_pcm16b() { return 93; }
    int pcm16b = default_pcm16b();

    static constexpr int default_pcm16b_wb() { return 94; }
    int pcm16b_wb = default_pcm16b_wb();

    static constexpr int default_pcm16b_swb32() { return 95; }
    int pcm16b_swb32 = default_pcm16b_swb32();

    static constexpr int default_pcm16b_swb48() { return 96; }
    int pcm16b_swb48 = default_pcm16b_swb48();

    static constexpr int default_g722() { return 9; }
    int g722 = default_g722();

    static constexpr int default_avt() { return 106; }
    int avt = default_avt();

    static constexpr int default_avt_16() { return 114; }
    int avt_16 = default_avt_16();

    static constexpr int default_avt_32() { return 115; }
    int avt_32 = default_avt_32();

    static constexpr int default_avt_48() { return 116; }
    int avt_48 = default_avt_48();

    static constexpr int default_red() { return 117; }
    int red = default_red();

    static constexpr int default_cn_nb() { return 13; }
    int cn_nb = default_cn_nb();

    static constexpr int default_cn_wb() { return 98; }
    int cn_wb = default_cn_wb();

    static constexpr int default_cn_swb32() { return 99; }
    int cn_swb32 = default_cn_swb32();

    static constexpr int default_cn_swb48() { return 100; }
    int cn_swb48 = default_cn_swb48();

    std::string replacement_audio_file;

    absl::optional<uint32_t> ssrc_filter;

    static constexpr int default_audio_level() { return 1; }
    int audio_level = default_audio_level();

    static constexpr int default_abs_send_time() { return 3; }
    int abs_send_time = default_abs_send_time();

    static constexpr int default_transport_seq_no() { return 5; }
    int transport_seq_no = default_transport_seq_no();

    static constexpr int default_video_content_type() { return 7; }
    int video_content_type = default_video_content_type();

    static constexpr int default_video_timing() { return 8; }
    int video_timing = default_video_timing();

    bool matlabplot = false;

    bool pythonplot = false;

    bool concealment_events = false;

    static constexpr int default_max_nr_packets_in_buffer() { return 200; }
    int max_nr_packets_in_buffer = default_max_nr_packets_in_buffer();

    static constexpr int default_initial_dummy_packets() { return 0; }
    int initial_dummy_packets = default_initial_dummy_packets();

    static constexpr int default_skip_get_audio_events() { return 0; }
    int skip_get_audio_events = default_skip_get_audio_events();

    bool enable_fast_accelerate = false;

    bool textlog = false;

    absl::optional<std::string> textlog_filename;

    absl::optional<std::string> plot_scripts_basename;

    absl::optional<std::string> output_audio_filename;

    std::string field_trial_string;
  };

  std::unique_ptr<NetEqTest> InitializeTestFromFile(
      absl::string_view input_filename,
      NetEqFactory* neteq_factory,
      const Config& config);
  std::unique_ptr<NetEqTest> InitializeTestFromString(
      absl::string_view input_string,
      NetEqFactory* neteq_factory,
      const Config& config);

 private:
  std::unique_ptr<NetEqTest> InitializeTest(std::unique_ptr<NetEqInput> input,
                                            NetEqFactory* neteq_factory,
                                            const Config& config);
  std::unique_ptr<SsrcSwitchDetector> ssrc_switch_detector_;
  std::unique_ptr<NetEqStatsPlotter> stats_plotter_;

  std::unique_ptr<ScopedFieldTrials> field_trials_;
};

}
}

#endif
