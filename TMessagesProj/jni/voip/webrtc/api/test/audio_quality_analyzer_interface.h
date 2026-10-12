/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_AUDIO_QUALITY_ANALYZER_INTERFACE_H_
#define API_TEST_AUDIO_QUALITY_ANALYZER_INTERFACE_H_

#include <string>

#include "api/test/stats_observer_interface.h"
#include "api/test/track_id_stream_info_map.h"

namespace webrtc {
namespace webrtc_pc_e2e {

class AudioQualityAnalyzerInterface : public StatsObserverInterface {
 public:
  ~AudioQualityAnalyzerInterface() override = default;

  virtual void Start(std::string test_case_name,
                     TrackIdStreamInfoMap* analyzer_helper) = 0;

  virtual void Stop() = 0;
};

}
}

#endif
