/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_FRAME_PROXIES_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_FRAME_PROXIES_H_

namespace webrtc {

class AudioFrame;
class AudioProcessing;

int ProcessAudioFrame(AudioProcessing* ap, AudioFrame* frame);

int ProcessReverseAudioFrame(AudioProcessing* ap, AudioFrame* frame);

}

#endif
