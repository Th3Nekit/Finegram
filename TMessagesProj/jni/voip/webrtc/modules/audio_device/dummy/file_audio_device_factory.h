/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_DEVICE_FILE_AUDIO_DEVICE_FACTORY_H_
#define AUDIO_DEVICE_FILE_AUDIO_DEVICE_FACTORY_H_

#include <stdint.h>

#include "absl/strings/string_view.h"

namespace webrtc {

class FileAudioDevice;

class FileAudioDeviceFactory {
 public:
  static FileAudioDevice* CreateFileAudioDevice();

  static void SetFilenamesToUse(absl::string_view inputAudioFilename,
                                absl::string_view outputAudioFilename);

 private:
  enum : uint32_t { MAX_FILENAME_LEN = 512 };
  static bool _isConfigured;
  static char _inputAudioFilename[MAX_FILENAME_LEN];
  static char _outputAudioFilename[MAX_FILENAME_LEN];
};

}

#endif
