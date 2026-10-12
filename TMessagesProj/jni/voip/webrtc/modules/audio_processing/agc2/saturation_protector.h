/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_SATURATION_PROTECTOR_H_

#include <memory>

namespace webrtc {
class ApmDataDumper;

class SaturationProtector {
 public:
  virtual ~SaturationProtector() = default;

  virtual float HeadroomDb() = 0;

  virtual void Analyze(float speech_probability,
                       float peak_dbfs,
                       float speech_level_dbfs) = 0;

  virtual void Reset() = 0;
};

std::unique_ptr<SaturationProtector> CreateSaturationProtector(
    float initial_headroom_db,
    int adjacent_speech_frames_threshold,
    ApmDataDumper* apm_data_dumper);

}

#endif
