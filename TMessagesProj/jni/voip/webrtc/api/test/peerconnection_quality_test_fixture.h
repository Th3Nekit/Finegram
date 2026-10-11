/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
#define API_TEST_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/audio_mixer.h"
#include "api/fec_controller.h"
#include "api/function_view.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory_interface.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "api/test/peer_network_dependencies.h"
#include "api/test/simulated_network.h"
#include "api/test/stats_observer_interface.h"
#include "api/test/track_id_stream_info_map.h"
#include "api/test/video/video_frame_writer.h"
#include "api/test/video_quality_analyzer_interface.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/media_constants.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class PeerConnectionE2EQualityTestFixture {
 public:

  class QualityMetricsReporter : public StatsObserverInterface {
   public:
    virtual ~QualityMetricsReporter() = default;

    virtual void Start(absl::string_view test_case_name,
                       const TrackIdStreamInfoMap* reporter_helper) = 0;

    virtual void StopAndReportResults() = 0;
  };

  class PeerHandle {
   public:
    virtual ~PeerHandle() = default;
  };

  virtual ~PeerConnectionE2EQualityTestFixture() = default;

  virtual void ExecuteAt(TimeDelta target_time_since_start,
                         std::function<void(TimeDelta)> func) = 0;

  virtual void ExecuteEvery(TimeDelta initial_delay_since_start,
                            TimeDelta interval,
                            std::function<void(TimeDelta)> func) = 0;

  virtual void AddQualityMetricsReporter(
      std::unique_ptr<QualityMetricsReporter> quality_metrics_reporter) = 0;

  virtual PeerHandle* AddPeer(std::unique_ptr<PeerConfigurer> configurer) = 0;

  virtual void Run(RunParams run_params) = 0;

  virtual TimeDelta GetRealTestDuration() const = 0;
};

}
}

#endif
