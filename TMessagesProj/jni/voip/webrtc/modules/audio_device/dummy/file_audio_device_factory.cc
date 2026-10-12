/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/dummy/file_audio_device_factory.h"

#include <stdio.h>

#include <cstdlib>

#include "absl/strings/string_view.h"
#include "modules/audio_device/dummy/file_audio_device.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_utils.h"

namespace webrtc {

bool FileAudioDeviceFactory::_isConfigured = false;
char FileAudioDeviceFactory::_inputAudioFilename[MAX_FILENAME_LEN] = "";
char FileAudioDeviceFactory::_outputAudioFilename[MAX_FILENAME_LEN] = "";

FileAudioDevice* FileAudioDeviceFactory::CreateFileAudioDevice() {

  if (!_isConfigured) {
    RTC_LOG(LS_WARNING)
        << "WebRTC configured with WEBRTC_DUMMY_FILE_DEVICES but "
           "no device files supplied. Will fall back to dummy "
           "audio.";

    return nullptr;
  }
  return new FileAudioDevice(_inputAudioFilename, _outputAudioFilename);
}

void FileAudioDeviceFactory::SetFilenamesToUse(
    absl::string_view inputAudioFilename,
    absl::string_view outputAudioFilename) {
#ifdef WEBRTC_DUMMY_FILE_DEVICES
  RTC_DCHECK_LT(inputAudioFilename.size(), MAX_FILENAME_LEN);
  RTC_DCHECK_LT(outputAudioFilename.size(), MAX_FILENAME_LEN);

  rtc::strcpyn(_inputAudioFilename, MAX_FILENAME_LEN, inputAudioFilename);
  rtc::strcpyn(_outputAudioFilename, MAX_FILENAME_LEN, outputAudioFilename);
  _isConfigured = true;
#else

  printf(
      "Trying to use dummy file devices, but is not compiled "
      "with WEBRTC_DUMMY_FILE_DEVICES. Bailing out.\n");
  std::exit(1);
#endif
}

}
