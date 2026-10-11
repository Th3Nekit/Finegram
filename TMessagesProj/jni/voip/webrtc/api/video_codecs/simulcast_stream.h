/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SIMULCAST_STREAM_H_
#define API_VIDEO_CODECS_SIMULCAST_STREAM_H_

#include "api/video_codecs/scalability_mode.h"

namespace webrtc {

struct SimulcastStream {

  unsigned char GetNumberOfTemporalLayers() const;
  ScalabilityMode GetScalabilityMode() const;
  void SetNumberOfTemporalLayers(unsigned char n);

  int width = 0;
  int height = 0;
  float maxFramerate = 0;
  unsigned char numberOfTemporalLayers = 1;
  unsigned int maxBitrate = 0;
  unsigned int targetBitrate = 0;
  unsigned int minBitrate = 0;
  unsigned int qpMax = 0;
  bool active = false;
};

}
#endif
