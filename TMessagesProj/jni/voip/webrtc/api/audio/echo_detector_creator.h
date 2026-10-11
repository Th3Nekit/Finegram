/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_ECHO_DETECTOR_CREATOR_H_
#define API_AUDIO_ECHO_DETECTOR_CREATOR_H_

#include "api/scoped_refptr.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

rtc::scoped_refptr<EchoDetector> CreateEchoDetector();

}

#endif
