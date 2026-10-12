/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_CREATE_FRAME_GENERATOR_H_
#define API_TEST_CREATE_FRAME_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/test/frame_generator_interface.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace test {

std::unique_ptr<FrameGeneratorInterface> CreateSquareFrameGenerator(
    int width,
    int height,
    absl::optional<FrameGeneratorInterface::OutputType> type,
    absl::optional<int> num_squares);

std::unique_ptr<FrameGeneratorInterface> CreateFromYuvFileFrameGenerator(
    std::vector<std::string> filenames,
    size_t width,
    size_t height,
    int frame_repeat_count);

std::unique_ptr<FrameGeneratorInterface> CreateFromNV12FileFrameGenerator(
    std::vector<std::string> filenames,
    size_t width,
    size_t height,
    int frame_repeat_count = 1);

std::unique_ptr<FrameGeneratorInterface> CreateFromIvfFileFrameGenerator(
    std::string filename);

std::unique_ptr<FrameGeneratorInterface>
CreateScrollingInputFromYuvFilesFrameGenerator(
    Clock* clock,
    std::vector<std::string> filenames,
    size_t source_width,
    size_t source_height,
    size_t target_width,
    size_t target_height,
    int64_t scroll_time_ms,
    int64_t pause_time_ms);

std::unique_ptr<FrameGeneratorInterface>
CreateSlideFrameGenerator(int width, int height, int frame_repeat_count);

}
}

#endif
