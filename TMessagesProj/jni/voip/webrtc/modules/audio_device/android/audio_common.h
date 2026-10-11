/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AUDIO_COMMON_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AUDIO_COMMON_H_

namespace webrtc {

const int kDefaultSampleRate = 44100;

const int kLowLatencyModeDelayEstimateInMilliseconds = 50;
const int kHighLatencyModeDelayEstimateInMilliseconds = 150;

}

#endif
