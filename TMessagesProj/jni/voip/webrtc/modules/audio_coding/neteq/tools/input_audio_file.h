/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_INPUT_AUDIO_FILE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_INPUT_AUDIO_FILE_H_

#include <stdio.h>

#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {
namespace test {

class InputAudioFile {
 public:
  explicit InputAudioFile(absl::string_view file_name, bool loop_at_end = true);

  virtual ~InputAudioFile();

  InputAudioFile(const InputAudioFile&) = delete;
  InputAudioFile& operator=(const InputAudioFile&) = delete;

  virtual bool Read(size_t samples, int16_t* destination);

  virtual bool Seek(int samples);

  static void DuplicateInterleaved(const int16_t* source,
                                   size_t samples,
                                   size_t channels,
                                   int16_t* destination);

 private:
  FILE* fp_;
  const bool loop_at_end_;
};

}
}
#endif
