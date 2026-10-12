/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_AUDIOPROC_FLOAT_H_
#define API_TEST_AUDIOPROC_FLOAT_H_

#include <memory>
#include <vector>

#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace test {

int AudioprocFloat(rtc::scoped_refptr<AudioProcessing> audio_processing,
                   int argc,
                   char* argv[]);

int AudioprocFloat(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                   int argc,
                   char* argv[]);

int AudioprocFloat(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                   int argc,
                   char* argv[],
                   absl::string_view input_aecdump,
                   std::vector<float>* processed_capture_samples);
}
}

#endif
