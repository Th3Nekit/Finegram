/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_CREATE_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_
#define API_TEST_CREATE_PEERCONNECTION_QUALITY_TEST_FIXTURE_H_

#include <memory>
#include <string>

#include "api/test/audio_quality_analyzer_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "api/test/time_controller.h"
#include "api/test/video_quality_analyzer_interface.h"

namespace webrtc {
namespace webrtc_pc_e2e {

std::unique_ptr<PeerConnectionE2EQualityTestFixture>
CreatePeerConnectionE2EQualityTestFixture(
    std::string test_case_name,
    TimeController& time_controller,
    std::unique_ptr<AudioQualityAnalyzerInterface> audio_quality_analyzer,
    std::unique_ptr<VideoQualityAnalyzerInterface> video_quality_analyzer);

}
}

#endif
