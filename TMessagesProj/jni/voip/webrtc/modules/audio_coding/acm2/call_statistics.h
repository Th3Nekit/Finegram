/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_CALL_STATISTICS_H_
#define MODULES_AUDIO_CODING_ACM2_CALL_STATISTICS_H_

#include "api/audio/audio_frame.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"

namespace webrtc {

namespace acm2 {

class CallStatistics {
 public:
  CallStatistics() {}
  ~CallStatistics() {}

  void DecodedByNetEq(AudioFrame::SpeechType speech_type, bool muted);

  void DecodedBySilenceGenerator();

  const AudioDecodingCallStats& GetDecodingStatistics() const;

 private:

  void ResetDecodingStatistics();

  AudioDecodingCallStats decoding_stat_;
};

}

}

#endif
