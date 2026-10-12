/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TGCALLS_AUDIO_AUDIO_DEVICE_IOS_H_
#define TGCALLS_AUDIO_AUDIO_DEVICE_IOS_H_

#include <atomic>
#include <memory>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "sdk/objc/native/src/audio/audio_session_observer.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/buffer.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "sdk/objc/base/RTCMacros.h"
#include "tgcalls_voice_processing_audio_unit.h"

#include "platform/darwin/iOS/CallAudioTone.h"

RTC_FWD_DECL_OBJC_CLASS(RTCNativeAudioSessionDelegateAdapter);

namespace webrtc {

class FineAudioBuffer;

namespace tgcalls_ios_adm {

class AudioDeviceIOS : public AudioDeviceGeneric,
                       public AudioSessionObserver,
                       public VoiceProcessingAudioUnitObserver {
 public:
  explicit AudioDeviceIOS(bool bypass_voice_processing, bool disable_recording, bool enableSystemMute, int numChannels);
  ~AudioDeviceIOS() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  InitStatus Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;

  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;

  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  void setIsBufferPlaying(bool isBufferPlaying);
  void setIsBufferRecording(bool isBufferRecording);

  int32_t PlayoutDelay(uint16_t& delayMS) const override;

  int32_t GetPlayoutUnderrunCount() const override { return -1; }

  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const override;
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t RecordingIsAvailable(bool& available) override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

  void OnInterruptionBegin() override;
  void OnInterruptionEnd() override;
  void OnValidRouteChange() override;
  void OnCanPlayOrRecordChange(bool can_play_or_record) override;
  void OnChangedOutputVolume() override;

  void setTone(std::shared_ptr<tgcalls::CallAudioTone> tone);

  OSStatus OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                 const AudioTimeStamp* time_stamp,
                                 UInt32 bus_number,
                                 UInt32 num_frames,
                                 AudioBufferList* io_data) override;
  OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 num_frames,
                            AudioBufferList* io_data) override;
  void OnMutedSpeechStatusChanged(bool isDetectingSpeech) override;

  bool IsInterrupted();

 public:
  void (^mutedSpeechDetectionChanged)(bool);

 private:

  void HandleInterruptionBegin();
  void HandleInterruptionEnd();
  void HandleValidRouteChange();
  void HandleCanPlayOrRecordChange(bool can_play_or_record);
  void HandleSampleRateChange();
  void HandlePlayoutGlitchDetected();
  void HandleOutputVolumeChange();

  void UpdateAudioDeviceBuffer();

  void SetupAudioBuffersForActiveAudioSession();

  bool CreateAudioUnit();

  void UpdateAudioUnit(bool can_play_or_record);

  bool ConfigureAudioSession();

  bool ConfigureAudioSessionLocked();

  void UnconfigureAudioSession();

  bool InitPlayOrRecord();

  void ShutdownPlayOrRecord();

  void PrepareForNewStart();

  const bool bypass_voice_processing_;

  const bool disable_recording_;
  const bool enableSystemMute_ = false;
  const int numChannels_;

  SequenceChecker io_thread_checker_;

  rtc::Thread* thread_;

  AudioDeviceBuffer* audio_device_buffer_;

  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  std::unique_ptr<VoiceProcessingAudioUnit> audio_unit_;

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  rtc::BufferT<int16_t> record_audio_buffer_;

  std::atomic<int> recording_;

  std::atomic<int> playing_;

  bool initialized_ RTC_GUARDED_BY(thread_);

  bool audio_is_initialized_;

  bool is_interrupted_;

  RTCNativeAudioSessionDelegateAdapter* audio_session_observer_
      RTC_GUARDED_BY(thread_);

  bool has_configured_session_ RTC_GUARDED_BY(thread_);

  int64_t num_detected_playout_glitches_ RTC_GUARDED_BY(thread_);
  int64_t last_playout_time_ RTC_GUARDED_BY(io_thread_checker_);

  int64_t num_playout_callbacks_;

  int64_t last_output_volume_change_time_ RTC_GUARDED_BY(thread_);

  webrtc::scoped_refptr<PendingTaskSafetyFlag> safety_ =
      PendingTaskSafetyFlag::Create();

  std::atomic<bool> _hasTone;
  std::shared_ptr<tgcalls::CallAudioTone> _tone;

  bool isBufferPlaying_ = false;
  bool isBufferRecording_ = false;

  bool isMicrophoneMuted_ = false;
};
}
}

#endif
