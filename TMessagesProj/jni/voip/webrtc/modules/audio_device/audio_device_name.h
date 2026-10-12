/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_NAME_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_NAME_H_

#include <deque>
#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

struct AudioDeviceName {

  static const char kDefaultDeviceId[];

  AudioDeviceName() = default;
  AudioDeviceName(absl::string_view device_name, absl::string_view unique_id);

  ~AudioDeviceName() = default;

  AudioDeviceName(const AudioDeviceName& other) = default;
  AudioDeviceName(AudioDeviceName&&) = default;
  AudioDeviceName& operator=(const AudioDeviceName&) = default;
  AudioDeviceName& operator=(AudioDeviceName&&) = default;

  bool IsValid();

  std::string device_name;
  std::string unique_id;
};

typedef std::deque<AudioDeviceName> AudioDeviceNames;

}

#endif
