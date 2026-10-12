/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_CREATE_PEER_CONNECTION_QUALITY_TEST_FRAME_GENERATOR_H_
#define API_TEST_CREATE_PEER_CONNECTION_QUALITY_TEST_FRAME_GENERATOR_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"

namespace webrtc {
namespace webrtc_pc_e2e {

std::unique_ptr<test::FrameGeneratorInterface> CreateSquareFrameGenerator(
    const VideoConfig& video_config,
    absl::optional<test::FrameGeneratorInterface::OutputType> type);

std::unique_ptr<test::FrameGeneratorInterface> CreateFromYuvFileFrameGenerator(
    const VideoConfig& video_config,
    std::string filename);

std::unique_ptr<test::FrameGeneratorInterface> CreateScreenShareFrameGenerator(
    const VideoConfig& video_config,
    const ScreenShareConfig& screen_share_config);

}
}

#endif
