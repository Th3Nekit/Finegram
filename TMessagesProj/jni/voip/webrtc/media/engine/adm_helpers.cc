/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/adm_helpers.h"

#include "modules/audio_device/include/audio_device.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace adm_helpers {

#if defined(WEBRTC_WIN)
#define AUDIO_DEVICE_ID \
  (AudioDeviceModule::WindowsDeviceType::kDefaultCommunicationDevice)
#else
#define AUDIO_DEVICE_ID (0u)
#endif

void Init(AudioDeviceModule* adm) {
  RTC_DCHECK(adm);

  RTC_CHECK_EQ(0, adm->Init()) << "Failed to initialize the ADM.";

  {
    if (adm->SetPlayoutDevice(AUDIO_DEVICE_ID) != 0) {
      RTC_LOG(LS_ERROR) << "Unable to set playout device.";
      return;
    }
    if (adm->InitSpeaker() != 0) {
      RTC_LOG(LS_ERROR) << "Unable to access speaker.";
    }

    bool available = false;
    if (adm->StereoPlayoutIsAvailable(&available) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to query stereo playout.";
    }
    if (adm->SetStereoPlayout(available) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to set stereo playout mode.";
    }
  }

  {
    if (adm->SetRecordingDevice(AUDIO_DEVICE_ID) != 0) {
      RTC_LOG(LS_ERROR) << "Unable to set recording device.";
      return;
    }
    if (adm->InitMicrophone() != 0) {
      RTC_LOG(LS_ERROR) << "Unable to access microphone.";
    }

    bool available = false;
    if (adm->StereoRecordingIsAvailable(&available) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to query stereo recording.";
    }
    if (adm->SetStereoRecording(available) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to set stereo recording mode.";
    }
  }
}
}
}
