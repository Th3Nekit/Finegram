/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/linux/audio_mixer_manager_pulse_linux.h"

#include <stddef.h>

#include "modules/audio_device/linux/audio_device_pulse_linux.h"
#include "modules/audio_device/linux/latebindingsymboltable_linux.h"
#include "modules/audio_device/linux/pulseaudiosymboltable_linux.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

#define LATE(sym)                                             \
  LATESYM_GET(webrtc::adm_linux_pulse::PulseAudioSymbolTable, \
              GetPulseSymbolTable(), sym)

namespace webrtc {

class AutoPulseLock {
 public:
  explicit AutoPulseLock(pa_threaded_mainloop* pa_mainloop)
      : pa_mainloop_(pa_mainloop) {
    LATE(pa_threaded_mainloop_lock)(pa_mainloop_);
  }

  ~AutoPulseLock() { LATE(pa_threaded_mainloop_unlock)(pa_mainloop_); }

 private:
  pa_threaded_mainloop* const pa_mainloop_;
};

AudioMixerManagerLinuxPulse::AudioMixerManagerLinuxPulse()
    : _paOutputDeviceIndex(-1),
      _paInputDeviceIndex(-1),
      _paPlayStream(NULL),
      _paRecStream(NULL),
      _paMainloop(NULL),
      _paContext(NULL),
      _paVolume(0),
      _paMute(0),
      _paVolSteps(0),
      _paSpeakerMute(false),
      _paSpeakerVolume(PA_VOLUME_NORM),
      _paChannels(0),
      _paObjectsSet(false) {
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " created";
}

AudioMixerManagerLinuxPulse::~AudioMixerManagerLinuxPulse() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_INFO) << __FUNCTION__ << " destroyed";

  Close();
}

int32_t AudioMixerManagerLinuxPulse::SetPulseAudioObjects(
    pa_threaded_mainloop* mainloop,
    pa_context* context) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  if (!mainloop || !context) {
    RTC_LOG(LS_ERROR) << "could not set PulseAudio objects for mixer";
    return -1;
  }

  _paMainloop = mainloop;
  _paContext = context;
  _paObjectsSet = true;

  RTC_LOG(LS_VERBOSE) << "the PulseAudio objects for the mixer has been set";

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::Close() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  CloseSpeaker();
  CloseMicrophone();

  _paMainloop = NULL;
  _paContext = NULL;
  _paObjectsSet = false;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::CloseSpeaker() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  _paOutputDeviceIndex = -1;
  _paPlayStream = NULL;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::CloseMicrophone() {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_VERBOSE) << __FUNCTION__;

  _paInputDeviceIndex = -1;
  _paRecStream = NULL;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SetPlayStream(pa_stream* playStream) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::SetPlayStream(playStream)";

  _paPlayStream = playStream;
  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SetRecStream(pa_stream* recStream) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerLinuxPulse::SetRecStream(recStream)";

  _paRecStream = recStream;
  return 0;
}

int32_t AudioMixerManagerLinuxPulse::OpenSpeaker(uint16_t deviceIndex) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerLinuxPulse::OpenSpeaker(deviceIndex="
                      << deviceIndex << ")";

  if (!_paObjectsSet) {
    RTC_LOG(LS_ERROR) << "PulseAudio objects has not been set";
    return -1;
  }

  _paOutputDeviceIndex = deviceIndex;

  RTC_LOG(LS_VERBOSE) << "the output mixer device is now open";

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::OpenMicrophone(uint16_t deviceIndex) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::OpenMicrophone(deviceIndex="
      << deviceIndex << ")";

  if (!_paObjectsSet) {
    RTC_LOG(LS_ERROR) << "PulseAudio objects have not been set";
    return -1;
  }

  _paInputDeviceIndex = deviceIndex;

  RTC_LOG(LS_VERBOSE) << "the input mixer device is now open";

  return 0;
}

bool AudioMixerManagerLinuxPulse::SpeakerIsInitialized() const {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  return (_paOutputDeviceIndex != -1);
}

bool AudioMixerManagerLinuxPulse::MicrophoneIsInitialized() const {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  return (_paInputDeviceIndex != -1);
}

int32_t AudioMixerManagerLinuxPulse::SetSpeakerVolume(uint32_t volume) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerLinuxPulse::SetSpeakerVolume(volume="
                      << volume << ")";

  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  bool setFailed(false);

  if (_paPlayStream &&
      (LATE(pa_stream_get_state)(_paPlayStream) != PA_STREAM_UNCONNECTED)) {

    AutoPulseLock auto_lock(_paMainloop);

    const pa_sample_spec* spec = LATE(pa_stream_get_sample_spec)(_paPlayStream);
    if (!spec) {
      RTC_LOG(LS_ERROR) << "could not get sample specification";
      return -1;
    }

    pa_cvolume cVolumes;
    LATE(pa_cvolume_set)(&cVolumes, spec->channels, volume);

    pa_operation* paOperation = NULL;
    paOperation = LATE(pa_context_set_sink_input_volume)(
        _paContext, LATE(pa_stream_get_index)(_paPlayStream), &cVolumes,
        PaSetVolumeCallback, NULL);
    if (!paOperation) {
      setFailed = true;
    }

    LATE(pa_operation_unref)(paOperation);
  } else {

    _paSpeakerVolume = volume;
  }

  if (setFailed) {
    RTC_LOG(LS_WARNING) << "could not set speaker volume, error="
                        << LATE(pa_context_errno)(_paContext);

    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SpeakerVolume(uint32_t& volume) const {
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  if (_paPlayStream &&
      (LATE(pa_stream_get_state)(_paPlayStream) != PA_STREAM_UNCONNECTED)) {

    if (!GetSinkInputInfo())
      return -1;

    AutoPulseLock auto_lock(_paMainloop);
    volume = static_cast<uint32_t>(_paVolume);
  } else {
    AutoPulseLock auto_lock(_paMainloop);
    volume = _paSpeakerVolume;
  }

  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerLinuxPulse::SpeakerVolume() => vol="
                      << volume;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MaxSpeakerVolume(
    uint32_t& maxVolume) const {
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  maxVolume = static_cast<uint32_t>(PA_VOLUME_NORM);

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MinSpeakerVolume(
    uint32_t& minVolume) const {
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  minVolume = static_cast<uint32_t>(PA_VOLUME_MUTED);

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SpeakerVolumeIsAvailable(bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  available = true;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SpeakerMuteIsAvailable(bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  available = true;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SetSpeakerMute(bool enable) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE) << "AudioMixerManagerLinuxPulse::SetSpeakerMute(enable="
                      << enable << ")";

  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  bool setFailed(false);

  if (_paPlayStream &&
      (LATE(pa_stream_get_state)(_paPlayStream) != PA_STREAM_UNCONNECTED)) {

    AutoPulseLock auto_lock(_paMainloop);

    pa_operation* paOperation = NULL;
    paOperation = LATE(pa_context_set_sink_input_mute)(
        _paContext, LATE(pa_stream_get_index)(_paPlayStream), (int)enable,
        PaSetVolumeCallback, NULL);
    if (!paOperation) {
      setFailed = true;
    }

    LATE(pa_operation_unref)(paOperation);
  } else {

    _paSpeakerMute = enable;
  }

  if (setFailed) {
    RTC_LOG(LS_WARNING) << "could not mute speaker, error="
                        << LATE(pa_context_errno)(_paContext);
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SpeakerMute(bool& enabled) const {
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  if (_paPlayStream &&
      (LATE(pa_stream_get_state)(_paPlayStream) != PA_STREAM_UNCONNECTED)) {

    if (!GetSinkInputInfo())
      return -1;

    enabled = static_cast<bool>(_paMute);
  } else {
    enabled = _paSpeakerMute;
  }
  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::SpeakerMute() => enabled=" << enabled;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::StereoPlayoutIsAvailable(bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paOutputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "output device index has not been set";
    return -1;
  }

  uint32_t deviceIndex = (uint32_t)_paOutputDeviceIndex;

  {
    AutoPulseLock auto_lock(_paMainloop);

    if (_paPlayStream &&
        (LATE(pa_stream_get_state)(_paPlayStream) != PA_STREAM_UNCONNECTED)) {
      deviceIndex = LATE(pa_stream_get_device_index)(_paPlayStream);
    }
  }

  if (!GetSinkInfoByIndex(deviceIndex))
    return -1;

  available = static_cast<bool>(_paChannels == 2);

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::StereoRecordingIsAvailable(
    bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  uint32_t deviceIndex = (uint32_t)_paInputDeviceIndex;

  AutoPulseLock auto_lock(_paMainloop);

  if (_paRecStream &&
      (LATE(pa_stream_get_state)(_paRecStream) != PA_STREAM_UNCONNECTED)) {
    deviceIndex = LATE(pa_stream_get_device_index)(_paRecStream);
  }

  pa_operation* paOperation = NULL;

  paOperation = LATE(pa_context_get_source_info_by_index)(
      _paContext, deviceIndex, PaSourceInfoCallback, (void*)this);

  WaitForOperationCompletion(paOperation);

  available = static_cast<bool>(_paChannels == 2);

  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::StereoRecordingIsAvailable()"
         " => available="
      << available;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MicrophoneMuteIsAvailable(
    bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  available = true;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SetMicrophoneMute(bool enable) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::SetMicrophoneMute(enable=" << enable
      << ")";

  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  bool setFailed(false);
  pa_operation* paOperation = NULL;

  uint32_t deviceIndex = (uint32_t)_paInputDeviceIndex;

  AutoPulseLock auto_lock(_paMainloop);

  if (_paRecStream &&
      (LATE(pa_stream_get_state)(_paRecStream) != PA_STREAM_UNCONNECTED)) {
    deviceIndex = LATE(pa_stream_get_device_index)(_paRecStream);
  }

  paOperation = LATE(pa_context_set_source_mute_by_index)(
      _paContext, deviceIndex, enable, PaSetVolumeCallback, NULL);

  if (!paOperation) {
    setFailed = true;
  }

  LATE(pa_operation_unref)(paOperation);

  if (setFailed) {
    RTC_LOG(LS_WARNING) << "could not mute microphone, error="
                        << LATE(pa_context_errno)(_paContext);
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MicrophoneMute(bool& enabled) const {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  uint32_t deviceIndex = (uint32_t)_paInputDeviceIndex;

  {
    AutoPulseLock auto_lock(_paMainloop);

    if (_paRecStream &&
        (LATE(pa_stream_get_state)(_paRecStream) != PA_STREAM_UNCONNECTED)) {
      deviceIndex = LATE(pa_stream_get_device_index)(_paRecStream);
    }
  }

  if (!GetSourceInfoByIndex(deviceIndex))
    return -1;

  enabled = static_cast<bool>(_paMute);

  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::MicrophoneMute() => enabled=" << enabled;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MicrophoneVolumeIsAvailable(
    bool& available) {
  RTC_DCHECK(thread_checker_.IsCurrent());
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  available = true;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::SetMicrophoneVolume(uint32_t volume) {
  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::SetMicrophoneVolume(volume=" << volume
      << ")";

  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  AutoPulseLock auto_lock(_paMainloop);
  uint32_t deviceIndex = (uint32_t)_paInputDeviceIndex;

  if (_paRecStream &&
      (LATE(pa_stream_get_state)(_paRecStream) != PA_STREAM_UNCONNECTED)) {
    deviceIndex = LATE(pa_stream_get_device_index)(_paRecStream);
  }

  bool setFailed(false);
  pa_operation* paOperation = NULL;

  paOperation = LATE(pa_context_get_source_info_by_index)(
      _paContext, deviceIndex, PaSourceInfoCallback, (void*)this);

  WaitForOperationCompletion(paOperation);

  uint8_t channels = _paChannels;
  pa_cvolume cVolumes;
  LATE(pa_cvolume_set)(&cVolumes, channels, volume);

  paOperation = LATE(pa_context_set_source_volume_by_index)(
      _paContext, deviceIndex, &cVolumes, PaSetVolumeCallback, NULL);

  if (!paOperation) {
    setFailed = true;
  }

  LATE(pa_operation_unref)(paOperation);

  if (setFailed) {
    RTC_LOG(LS_WARNING) << "could not set microphone volume, error="
                        << LATE(pa_context_errno)(_paContext);
    return -1;
  }

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MicrophoneVolume(uint32_t& volume) const {
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  uint32_t deviceIndex = (uint32_t)_paInputDeviceIndex;

  {
    AutoPulseLock auto_lock(_paMainloop);

    if (_paRecStream &&
        (LATE(pa_stream_get_state)(_paRecStream) != PA_STREAM_UNCONNECTED)) {
      deviceIndex = LATE(pa_stream_get_device_index)(_paRecStream);
    }
  }

  if (!GetSourceInfoByIndex(deviceIndex))
    return -1;

  {
    AutoPulseLock auto_lock(_paMainloop);
    volume = static_cast<uint32_t>(_paVolume);
  }

  RTC_LOG(LS_VERBOSE)
      << "AudioMixerManagerLinuxPulse::MicrophoneVolume() => vol=" << volume;

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MaxMicrophoneVolume(
    uint32_t& maxVolume) const {
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  maxVolume = static_cast<uint32_t>(PA_VOLUME_NORM);

  return 0;
}

int32_t AudioMixerManagerLinuxPulse::MinMicrophoneVolume(
    uint32_t& minVolume) const {
  if (_paInputDeviceIndex == -1) {
    RTC_LOG(LS_WARNING) << "input device index has not been set";
    return -1;
  }

  minVolume = static_cast<uint32_t>(PA_VOLUME_MUTED);

  return 0;
}

void AudioMixerManagerLinuxPulse::PaSinkInfoCallback(pa_context*      ,
                                                     const pa_sink_info* i,
                                                     int eol,
                                                     void* pThis) {
  static_cast<AudioMixerManagerLinuxPulse*>(pThis)->PaSinkInfoCallbackHandler(
      i, eol);
}

void AudioMixerManagerLinuxPulse::PaSinkInputInfoCallback(
    pa_context*      ,
    const pa_sink_input_info* i,
    int eol,
    void* pThis) {
  static_cast<AudioMixerManagerLinuxPulse*>(pThis)
      ->PaSinkInputInfoCallbackHandler(i, eol);
}

void AudioMixerManagerLinuxPulse::PaSourceInfoCallback(pa_context*      ,
                                                       const pa_source_info* i,
                                                       int eol,
                                                       void* pThis) {
  static_cast<AudioMixerManagerLinuxPulse*>(pThis)->PaSourceInfoCallbackHandler(
      i, eol);
}

void AudioMixerManagerLinuxPulse::PaSetVolumeCallback(pa_context* c,
                                                      int success,
                                                      void*          ) {
  if (!success) {
    RTC_LOG(LS_ERROR) << "failed to set volume";
  }
}

void AudioMixerManagerLinuxPulse::PaSinkInfoCallbackHandler(
    const pa_sink_info* i,
    int eol) {
  if (eol) {

    LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
    return;
  }

  _paChannels = i->channel_map.channels;
  pa_volume_t paVolume = PA_VOLUME_MUTED;
  for (int j = 0; j < _paChannels; ++j) {
    if (paVolume < i->volume.values[j]) {
      paVolume = i->volume.values[j];
    }
  }
  _paVolume = paVolume;
  _paMute = i->mute;

  _paVolSteps = PA_VOLUME_NORM + 1;
}

void AudioMixerManagerLinuxPulse::PaSinkInputInfoCallbackHandler(
    const pa_sink_input_info* i,
    int eol) {
  if (eol) {

    LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
    return;
  }

  _paChannels = i->channel_map.channels;
  pa_volume_t paVolume = PA_VOLUME_MUTED;
  for (int j = 0; j < _paChannels; ++j) {
    if (paVolume < i->volume.values[j]) {
      paVolume = i->volume.values[j];
    }
  }
  _paVolume = paVolume;
  _paMute = i->mute;
}

void AudioMixerManagerLinuxPulse::PaSourceInfoCallbackHandler(
    const pa_source_info* i,
    int eol) {
  if (eol) {

    LATE(pa_threaded_mainloop_signal)(_paMainloop, 0);
    return;
  }

  _paChannels = i->channel_map.channels;
  pa_volume_t paVolume = PA_VOLUME_MUTED;
  for (int j = 0; j < _paChannels; ++j) {
    if (paVolume < i->volume.values[j]) {
      paVolume = i->volume.values[j];
    }
  }
  _paVolume = paVolume;
  _paMute = i->mute;

  _paVolSteps = PA_VOLUME_NORM + 1;
}

void AudioMixerManagerLinuxPulse::WaitForOperationCompletion(
    pa_operation* paOperation) const {
  while (LATE(pa_operation_get_state)(paOperation) == PA_OPERATION_RUNNING) {
    LATE(pa_threaded_mainloop_wait)(_paMainloop);
  }

  LATE(pa_operation_unref)(paOperation);
}

bool AudioMixerManagerLinuxPulse::GetSinkInputInfo() const {
  pa_operation* paOperation = NULL;

  AutoPulseLock auto_lock(_paMainloop);

  paOperation = LATE(pa_context_get_sink_input_info)(
      _paContext, LATE(pa_stream_get_index)(_paPlayStream),
      PaSinkInputInfoCallback, (void*)this);

  WaitForOperationCompletion(paOperation);
  return true;
}

bool AudioMixerManagerLinuxPulse::GetSinkInfoByIndex(int device_index) const {
  pa_operation* paOperation = NULL;

  AutoPulseLock auto_lock(_paMainloop);
  paOperation = LATE(pa_context_get_sink_info_by_index)(
      _paContext, device_index, PaSinkInfoCallback, (void*)this);

  WaitForOperationCompletion(paOperation);
  return true;
}

bool AudioMixerManagerLinuxPulse::GetSourceInfoByIndex(int device_index) const {
  pa_operation* paOperation = NULL;

  AutoPulseLock auto_lock(_paMainloop);
  paOperation = LATE(pa_context_get_source_info_by_index)(
      _paContext, device_index, PaSourceInfoCallback, (void*)this);

  WaitForOperationCompletion(paOperation);
  return true;
}

}
