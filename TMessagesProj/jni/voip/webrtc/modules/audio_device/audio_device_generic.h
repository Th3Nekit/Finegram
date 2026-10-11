/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_DEVICE_AUDIO_DEVICE_GENERIC_H_
#define AUDIO_DEVICE_AUDIO_DEVICE_GENERIC_H_

#include <stdint.h>

#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/audio_device_defines.h"

namespace webrtc {

class AudioDeviceGeneric {
 public:

  enum class InitStatus {
    OK = 0,
    PLAYOUT_ERROR = 1,
    RECORDING_ERROR = 2,
    OTHER_ERROR = 3,
    NUM_STATUSES = 4
  };

  virtual int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const = 0;

  virtual InitStatus Init() = 0;
  virtual int32_t Terminate() = 0;
  virtual bool Initialized() const = 0;

  virtual int16_t PlayoutDevices() = 0;
  virtual int16_t RecordingDevices() = 0;
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]) = 0;
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) = 0;

  virtual int32_t SetPlayoutDevice(uint16_t index) = 0;
  virtual int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) = 0;
  virtual int32_t SetRecordingDevice(uint16_t index) = 0;
  virtual int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) = 0;

  virtual int32_t PlayoutIsAvailable(bool& available) = 0;
  virtual int32_t InitPlayout() = 0;
  virtual bool PlayoutIsInitialized() const = 0;
  virtual int32_t RecordingIsAvailable(bool& available) = 0;
  virtual int32_t InitRecording() = 0;
  virtual bool RecordingIsInitialized() const = 0;

  virtual int32_t StartPlayout() = 0;
  virtual int32_t StopPlayout() = 0;
  virtual bool Playing() const = 0;
  virtual int32_t StartRecording() = 0;
  virtual int32_t StopRecording() = 0;
  virtual bool Recording() const = 0;

  virtual int32_t InitSpeaker() = 0;
  virtual bool SpeakerIsInitialized() const = 0;
  virtual int32_t InitMicrophone() = 0;
  virtual bool MicrophoneIsInitialized() const = 0;

  virtual int32_t SpeakerVolumeIsAvailable(bool& available) = 0;
  virtual int32_t SetSpeakerVolume(uint32_t volume) = 0;
  virtual int32_t SpeakerVolume(uint32_t& volume) const = 0;
  virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const = 0;
  virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const = 0;

  virtual int32_t MicrophoneVolumeIsAvailable(bool& available) = 0;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) = 0;
  virtual int32_t MicrophoneVolume(uint32_t& volume) const = 0;
  virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const = 0;
  virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const = 0;

  virtual int32_t SpeakerMuteIsAvailable(bool& available) = 0;
  virtual int32_t SetSpeakerMute(bool enable) = 0;
  virtual int32_t SpeakerMute(bool& enabled) const = 0;

  virtual int32_t MicrophoneMuteIsAvailable(bool& available) = 0;
  virtual int32_t SetMicrophoneMute(bool enable) = 0;
  virtual int32_t MicrophoneMute(bool& enabled) const = 0;

  virtual int32_t StereoPlayoutIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoPlayout(bool enable) = 0;
  virtual int32_t StereoPlayout(bool& enabled) const = 0;
  virtual int32_t StereoRecordingIsAvailable(bool& available) = 0;
  virtual int32_t SetStereoRecording(bool enable) = 0;
  virtual int32_t StereoRecording(bool& enabled) const = 0;

  virtual int32_t PlayoutDelay(uint16_t& delayMS) const = 0;

  virtual bool BuiltInAECIsAvailable() const;
  virtual bool BuiltInAGCIsAvailable() const;
  virtual bool BuiltInNSIsAvailable() const;

  virtual int32_t EnableBuiltInAEC(bool enable);
  virtual int32_t EnableBuiltInAGC(bool enable);
  virtual int32_t EnableBuiltInNS(bool enable);

  virtual int32_t GetPlayoutUnderrunCount() const;

#if defined(WEBRTC_IOS)
  virtual int GetPlayoutAudioParameters(AudioParameters* params) const;
  virtual int GetRecordAudioParameters(AudioParameters* params) const;
#endif

  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) = 0;

  virtual ~AudioDeviceGeneric() {}
};

}

#endif
