/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "tgcalls_audio_device_ios.h"

#include <cmath>

#include "api/array_view.h"
#include "sdk/objc/native/src/audio/helpers.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "RTCAudioSession+Private.h"
#import "RTCAudioSession.h"
#import "RTCAudioSessionConfiguration.h"
#import "RTCNativeAudioSessionDelegateAdapter.h"

namespace webrtc {
namespace tgcalls_ios_adm {

#define LOGI() RTC_LOG(LS_INFO) << "AudioDeviceIOS::"

#define LOG_AND_RETURN_IF_ERROR(error, message)    \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
      return false;                                \
    }                                              \
  } while (0)

#define LOG_IF_ERROR(error, message)               \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
    }                                              \
  } while (0)

const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

using ios::CheckAndLogError;

#if !defined(NDEBUG)

static bool DeviceIsSimulator() {
  return ios::GetDeviceName() == "x86_64";
}

static void LogDeviceInfo() {
  RTC_LOG(LS_INFO) << "LogDeviceInfo";
  @autoreleasepool {
    RTC_LOG(LS_INFO) << " system name: " << ios::GetSystemName();
    RTC_LOG(LS_INFO) << " system version: " << ios::GetSystemVersionAsString();
    RTC_LOG(LS_INFO) << " device type: " << ios::GetDeviceType();
    RTC_LOG(LS_INFO) << " device name: " << ios::GetDeviceName();
    RTC_LOG(LS_INFO) << " process name: " << ios::GetProcessName();
    RTC_LOG(LS_INFO) << " process ID: " << ios::GetProcessID();
    RTC_LOG(LS_INFO) << " OS version: " << ios::GetOSVersionString();
    RTC_LOG(LS_INFO) << " processing cores: " << ios::GetProcessorCount();
    RTC_LOG(LS_INFO) << " low power mode: " << ios::GetLowPowerModeEnabled();
#if TARGET_IPHONE_SIMULATOR
    RTC_LOG(LS_INFO) << " TARGET_IPHONE_SIMULATOR is defined";
#endif
    RTC_LOG(LS_INFO) << " DeviceIsSimulator: " << DeviceIsSimulator();
  }
}
#endif

AudioDeviceIOS::AudioDeviceIOS(bool bypass_voice_processing, bool disable_recording, bool enableSystemMute, int numChannels)
    : bypass_voice_processing_(bypass_voice_processing),
      disable_recording_(disable_recording),
      enableSystemMute_(enableSystemMute),
      numChannels_(numChannels),
      audio_device_buffer_(nullptr),
      audio_unit_(nullptr),
      recording_(0),
      playing_(0),
      initialized_(false),
      audio_is_initialized_(false),
      is_interrupted_(false),
      has_configured_session_(false),
      num_detected_playout_glitches_(0),
      last_playout_time_(0),
      num_playout_callbacks_(0),
      last_output_volume_change_time_(0) {
  LOGI() << "ctor" << ios::GetCurrentThreadDescription()
         << ",bypass_voice_processing=" << bypass_voice_processing_;
  io_thread_checker_.Detach();
  thread_ = rtc::Thread::Current();

  audio_session_observer_ = [[RTCNativeAudioSessionDelegateAdapter alloc] initWithObserver:this];
}

AudioDeviceIOS::~AudioDeviceIOS() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "~dtor" << ios::GetCurrentThreadDescription();
  safety_->SetNotAlive();
  Terminate();
  audio_session_observer_ = nil;
}

void AudioDeviceIOS::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK_RUN_ON(thread_);
  audio_device_buffer_ = audioBuffer;
}

AudioDeviceGeneric::InitStatus AudioDeviceIOS::Init() {
  LOGI() << "Init";
  io_thread_checker_.Detach();

  RTC_DCHECK_RUN_ON(thread_);
  if (initialized_) {
    return InitStatus::OK;
  }
#if !defined(NDEBUG)
  LogDeviceInfo();
#endif

  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* config =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate, config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate, config.inputNumberOfChannels);

  UpdateAudioDeviceBuffer();
  initialized_ = true;
  return InitStatus::OK;
}

int32_t AudioDeviceIOS::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK_RUN_ON(thread_);
  if (!initialized_) {
    return 0;
  }
  StopPlayout();
  StopRecording();
  initialized_ = false;
  return 0;
}

bool AudioDeviceIOS::Initialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return initialized_;
}

int32_t AudioDeviceIOS::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!audio_is_initialized_);
  RTC_DCHECK(!playing_.load());
  if (!audio_is_initialized_) {
    if (!InitPlayOrRecord()) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitPlayout!";
      return -1;
    }
  }
  audio_is_initialized_ = true;
  return 0;
}

bool AudioDeviceIOS::PlayoutIsInitialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return audio_is_initialized_;
}

bool AudioDeviceIOS::RecordingIsInitialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return audio_is_initialized_;
}

int32_t AudioDeviceIOS::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!audio_is_initialized_);
  RTC_DCHECK(!recording_.load());
  if (!audio_is_initialized_) {
    if (!InitPlayOrRecord()) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitRecording!";
      return -1;
    }
  }
  audio_is_initialized_ = true;
  return 0;
}

int32_t AudioDeviceIOS::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(audio_is_initialized_);
  RTC_DCHECK(!playing_.load());
  RTC_DCHECK(audio_unit_);
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }
  if (!recording_.load() && audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
    OSStatus result = audio_unit_->Start();
    if (result != noErr) {
      RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
      [session notifyAudioUnitStartFailedWithError:result];
      RTCLogError(@"StartPlayout failed to start audio unit, reason %d", result);
      return -1;
    }
    RTC_LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  playing_.store(1, std::memory_order_release);
  num_playout_callbacks_ = 0;
  num_detected_playout_glitches_ = 0;
  return 0;
}

int32_t AudioDeviceIOS::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  if (!audio_is_initialized_ || !playing_.load()) {
    return 0;
  }
  if (!recording_.load()) {
    ShutdownPlayOrRecord();
    audio_is_initialized_ = false;
  }
  playing_.store(0, std::memory_order_release);

  int average_number_of_playout_callbacks_between_glitches = 100000;
  RTC_DCHECK_GE(num_playout_callbacks_, num_detected_playout_glitches_);
  if (num_detected_playout_glitches_ > 0) {
    average_number_of_playout_callbacks_between_glitches = (int32_t)(
    num_playout_callbacks_ / num_detected_playout_glitches_);
  }
  RTC_HISTOGRAM_COUNTS_100000("WebRTC.Audio.AveragePlayoutCallbacksBetweenGlitches",
                              average_number_of_playout_callbacks_between_glitches);
  RTCLog(@"Average number of playout callbacks between glitches: %d",
         average_number_of_playout_callbacks_between_glitches);
  return 0;
}

bool AudioDeviceIOS::Playing() const {
  return playing_.load();
}

int32_t AudioDeviceIOS::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(audio_is_initialized_);
  RTC_DCHECK(!recording_.load());
  RTC_DCHECK(audio_unit_);
  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetRecord();
  }
  if (!playing_.load() && audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
    OSStatus result = audio_unit_->Start();
    if (result != noErr) {
      RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
      [session notifyAudioUnitStartFailedWithError:result];
      RTCLogError(@"StartRecording failed to start audio unit, reason %d", result);
      return -1;
    }
    RTC_LOG(LS_INFO) << "Voice-Processing I/O audio unit is now started";
  }
  recording_.store(1, std::memory_order_release);
  return 0;
}

int32_t AudioDeviceIOS::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK_RUN_ON(thread_);
  if (!audio_is_initialized_ || !recording_.load()) {
    return 0;
  }
  if (!playing_.load()) {
    ShutdownPlayOrRecord();
    audio_is_initialized_ = false;
  }
  recording_.store(0, std::memory_order_release);
  return 0;
}

bool AudioDeviceIOS::Recording() const {
  return recording_.load();
}

void AudioDeviceIOS::setIsBufferPlaying(bool isBufferPlaying) {
  isBufferPlaying_ = isBufferPlaying;
}

void AudioDeviceIOS::setIsBufferRecording(bool isBufferRecording) {
  isBufferRecording_ = isBufferRecording;
}

int32_t AudioDeviceIOS::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int AudioDeviceIOS::GetPlayoutAudioParameters(AudioParameters* params) const {
  LOGI() << "GetPlayoutAudioParameters";
  RTC_DCHECK(playout_parameters_.is_valid());
  RTC_DCHECK_RUN_ON(thread_);
  *params = playout_parameters_;
  return 0;
}

int AudioDeviceIOS::GetRecordAudioParameters(AudioParameters* params) const {
  LOGI() << "GetRecordAudioParameters";
  RTC_DCHECK(record_parameters_.is_valid());
  RTC_DCHECK_RUN_ON(thread_);
  *params = record_parameters_;
  return 0;
}

void AudioDeviceIOS::OnInterruptionBegin() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionBegin";
  thread_->PostTask(SafeTask(safety_, [this] { HandleInterruptionBegin(); }));
}

void AudioDeviceIOS::OnInterruptionEnd() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionEnd";
  thread_->PostTask(SafeTask(safety_, [this] { HandleInterruptionEnd(); }));
}

void AudioDeviceIOS::OnValidRouteChange() {
  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] { HandleValidRouteChange(); }));
}

void AudioDeviceIOS::OnCanPlayOrRecordChange(bool can_play_or_record) {
  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(
      safety_, [this, can_play_or_record] { HandleCanPlayOrRecordChange(can_play_or_record); }));
}

void AudioDeviceIOS::OnChangedOutputVolume() {
  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] { HandleOutputVolumeChange(); }));
}

void AudioDeviceIOS::setTone(std::shared_ptr<tgcalls::CallAudioTone> tone) {
    RTC_DCHECK(thread_);
    thread_->PostTask(SafeTask(safety_, [this, tone] {
        _tone = tone;
        _hasTone.store(true);
    }));
}

OSStatus AudioDeviceIOS::OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                               const AudioTimeStamp* time_stamp,
                                               UInt32 bus_number,
                                               UInt32 num_frames,
                                               AudioBufferList*              ) {
  RTC_DCHECK_RUN_ON(&io_thread_checker_);
  OSStatus result = noErr;

  if (!recording_.load(std::memory_order_acquire)) return result;

  record_audio_buffer_.Clear();
  record_audio_buffer_.SetSize(num_frames);

  AudioBufferList audio_buffer_list;
  audio_buffer_list.mNumberBuffers = 1;
  AudioBuffer* audio_buffer = &audio_buffer_list.mBuffers[0];
  audio_buffer->mNumberChannels = (uint32_t)record_parameters_.channels();
  audio_buffer->mDataByteSize =
      (uint32_t)(record_audio_buffer_.size() * VoiceProcessingAudioUnit::kBytesPerSample);
  audio_buffer->mData = reinterpret_cast<int8_t*>(record_audio_buffer_.data());

  result = audio_unit_->Render(flags, time_stamp, bus_number, num_frames, &audio_buffer_list);
  if (result != noErr) {
    RTCLogError(@"Failed to render audio.");
    return result;
  }

  if (isBufferRecording_) {
    fine_audio_buffer_->DeliverRecordedData(record_audio_buffer_, kFixedRecordDelayEstimate);
  }
  return noErr;
}

OSStatus AudioDeviceIOS::OnGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                          const AudioTimeStamp* time_stamp,
                                          UInt32 bus_number,
                                          UInt32 num_frames,
                                          AudioBufferList* io_data) {
  RTC_DCHECK_RUN_ON(&io_thread_checker_);

  RTC_DCHECK_EQ(1, io_data->mNumberBuffers);
  AudioBuffer* audio_buffer = &io_data->mBuffers[0];

  if (!playing_.load(std::memory_order_acquire)) {
    const size_t size_in_bytes = audio_buffer->mDataByteSize;
    RTC_CHECK_EQ((size_in_bytes / audio_buffer->mNumberChannels) / VoiceProcessingAudioUnit::kBytesPerSample, num_frames);
    *flags |= kAudioUnitRenderAction_OutputIsSilence;
    memset(static_cast<int8_t*>(audio_buffer->mData), 0, size_in_bytes);
    return noErr;
  }

  ++num_playout_callbacks_;
  const int64_t now_time = rtc::TimeMillis();
  if (time_stamp->mSampleTime != num_frames) {
    const int64_t delta_time = now_time - last_playout_time_;
    const int glitch_threshold = 1.6 * playout_parameters_.GetBufferSizeInMilliseconds();
    if (delta_time > glitch_threshold) {
      RTCLogWarning(@"Possible playout audio glitch detected.\n"
                     "  Time since last OnGetPlayoutData was %lld ms.\n",
                    delta_time);

      if (glitch_threshold < 120 && delta_time > 120) {
        RTCLog(@"Glitch warning is ignored. Probably caused by device switch.");
      } else {
        thread_->PostTask(SafeTask(safety_, [this] { HandlePlayoutGlitchDetected(); }));
      }
    }
  }
  last_playout_time_ = now_time;

  if (isBufferPlaying_) {
    fine_audio_buffer_->GetPlayoutData(
      rtc::ArrayView<int16_t>(static_cast<int16_t*>(audio_buffer->mData), num_frames * audio_buffer->mNumberChannels), kFixedPlayoutDelayEstimate);
  } else {
    memset(audio_buffer->mData, 0, num_frames * audio_buffer->mNumberChannels * sizeof(int16_t));
  }

  if (_hasTone.load()) {
      int16_t *mixDestination = static_cast<int16_t*>(audio_buffer->mData);

      auto tone = _tone;
      if (tone) {
          size_t destinationOffset = 0;
          size_t destinationMax = num_frames * audio_buffer->mNumberChannels;
          std::vector<int16_t> const &samples = tone->samples();
          size_t toneOffset = tone->offset();

          while (destinationOffset < destinationMax) {
              while (toneOffset < samples.size() && destinationOffset < destinationMax) {
                  for (int i = 0; i < audio_buffer->mNumberChannels; i++) {
                      int32_t current = mixDestination[destinationOffset];
                      current = current + samples[toneOffset];
                      if (current > INT16_MAX) {
                          current = INT16_MAX;
                      }
                      if (current < INT16_MIN) {
                          current = INT16_MIN;
                      }
                      mixDestination[destinationOffset] = current;
                      destinationOffset++;
                  }
                  toneOffset++;
              }
              if (toneOffset >= samples.size()) {
                  if (tone->loopCount() <= 1) {
                      _tone.reset();
                      tone.reset();
                      _hasTone.store(false);
                      break;
                  } else {
                      tone->setLoopCount(tone->loopCount() - 1);
                      toneOffset = 0;
                  }
              }
          }
          if (tone) {
              tone->setOffset(toneOffset);
          }
      } else {
          _hasTone.store(false);
      }
  }

  return noErr;
}

void AudioDeviceIOS::OnMutedSpeechStatusChanged(bool isDetectingSpeech) {
    if (mutedSpeechDetectionChanged) {
        mutedSpeechDetectionChanged(isDetectingSpeech);
    }
}

void AudioDeviceIOS::HandleInterruptionBegin() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Interruption begin. IsInterrupted changed from %d to 1.", is_interrupted_);
  if (audio_unit_ && audio_unit_->GetState() == VoiceProcessingAudioUnit::kStarted) {
    RTCLog(@"Stopping the audio unit due to interruption begin.");
    if (!audio_unit_->Stop()) {
      RTCLogError(@"Failed to stop the audio unit for interruption begin.");
    }
    PrepareForNewStart();
  }
  is_interrupted_ = true;
}

void AudioDeviceIOS::HandleInterruptionEnd() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Interruption ended. IsInterrupted changed from %d to 0. "
          "Updating audio unit state.",
         is_interrupted_);
  is_interrupted_ = false;
  if (!audio_unit_) return;
  if (webrtc::field_trial::IsEnabled("WebRTC-Audio-iOS-Holding")) {

    if (audio_unit_->GetState() == VoiceProcessingAudioUnit::kStarted) {
      audio_unit_->Stop();
      PrepareForNewStart();
    }
    if (audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
      audio_unit_->Uninitialize();
    }

    SetupAudioBuffersForActiveAudioSession();
  }
  UpdateAudioUnit([RTC_OBJC_TYPE(RTCAudioSession) sharedInstance].canPlayOrRecord);
}

void AudioDeviceIOS::HandleValidRouteChange() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  RTCLog(@"%@", session);
  HandleSampleRateChange();
}

void AudioDeviceIOS::HandleCanPlayOrRecordChange(bool can_play_or_record) {
  RTCLog(@"Handling CanPlayOrRecord change to: %d", can_play_or_record);
  UpdateAudioUnit(can_play_or_record);
}

void AudioDeviceIOS::HandleSampleRateChange() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Handling sample rate change.");

  if (is_interrupted_) {
    RTCLog(@"Ignoring sample rate change due to interruption.");
    return;
  }

  if (!audio_unit_ || audio_unit_->GetState() < VoiceProcessingAudioUnit::kInitialized) {
    return;
  }

  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  const double new_sample_rate = session.sampleRate;
  const NSTimeInterval session_buffer_duration = session.IOBufferDuration;
  const size_t new_frames_per_buffer =
      static_cast<size_t>(new_sample_rate * session_buffer_duration + .5);
  const double current_sample_rate = playout_parameters_.sample_rate();
  const size_t current_frames_per_buffer = playout_parameters_.frames_per_buffer();
  RTCLog(@"Handling playout sample rate change:\n"
          "  Session sample rate: %f frames_per_buffer: %lu\n"
          "  ADM sample rate: %f frames_per_buffer: %lu",
         new_sample_rate,
         (unsigned long)new_frames_per_buffer,
         current_sample_rate,
         (unsigned long)current_frames_per_buffer);

  if (std::abs(current_sample_rate - new_sample_rate) <= DBL_EPSILON &&
      current_frames_per_buffer == new_frames_per_buffer) {
    RTCLog(@"Ignoring sample rate change since audio parameters are intact.");
    return;
  }

  if (new_sample_rate <= 0.0) {
    RTCLogError(@"Sample rate is invalid: %f", new_sample_rate);
    return;
  }

  RTCLog(@"Stopping and uninitializing audio unit to adjust buffers.");
  bool restart_audio_unit = false;
  if (audio_unit_->GetState() == VoiceProcessingAudioUnit::kStarted) {
    audio_unit_->Stop();
    restart_audio_unit = true;
    PrepareForNewStart();
  }
  if (audio_unit_->GetState() == VoiceProcessingAudioUnit::kInitialized) {
    audio_unit_->Uninitialize();
  }

  SetupAudioBuffersForActiveAudioSession();

  if (!audio_unit_->Initialize(playout_parameters_.sample_rate())) {
    RTCLogError(@"Failed to initialize the audio unit with sample rate: %d",
                playout_parameters_.sample_rate());
    return;
  }

  if (restart_audio_unit) {
    OSStatus result = audio_unit_->Start();
    if (result != noErr) {
      RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
      [session notifyAudioUnitStartFailedWithError:result];
      RTCLogError(@"Failed to start audio unit with sample rate: %d, reason %d",
                  playout_parameters_.sample_rate(),
                  result);
      return;
    }
  }
  RTCLog(@"Successfully handled sample rate change.");
}

void AudioDeviceIOS::HandlePlayoutGlitchDetected() {
  RTC_DCHECK_RUN_ON(thread_);

  if (is_interrupted_) {
    RTCLog(@"Ignoring audio glitch due to interruption.");
    return;
  }

  if (last_output_volume_change_time_ > 0 &&
      rtc::TimeSince(last_output_volume_change_time_) < 2000) {
    RTCLog(@"Ignoring audio glitch due to recent output volume change.");
    return;
  }
  num_detected_playout_glitches_++;
  RTCLog(@"Number of detected playout glitches: %lld", num_detected_playout_glitches_);

  int64_t glitch_count = num_detected_playout_glitches_;
  dispatch_async(dispatch_get_main_queue(), ^{
    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    [session notifyDidDetectPlayoutGlitch:glitch_count];
  });
}

void AudioDeviceIOS::HandleOutputVolumeChange() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Output volume change detected.");

  last_output_volume_change_time_ = rtc::TimeMillis();
}

void AudioDeviceIOS::UpdateAudioDeviceBuffer() {
  LOGI() << "UpdateAudioDevicebuffer";

  RTC_DCHECK(audio_device_buffer_) << "AttachAudioBuffer must be called first";
  RTC_DCHECK_GT(playout_parameters_.sample_rate(), 0);
  RTC_DCHECK_GT(record_parameters_.sample_rate(), 0);

  RTC_DCHECK_EQ(record_parameters_.channels(), 1);

  audio_device_buffer_->SetPlayoutSampleRate(playout_parameters_.sample_rate());
  audio_device_buffer_->SetPlayoutChannels(playout_parameters_.channels());
  audio_device_buffer_->SetRecordingSampleRate(record_parameters_.sample_rate());
  audio_device_buffer_->SetRecordingChannels(record_parameters_.channels());
}

void AudioDeviceIOS::SetupAudioBuffersForActiveAudioSession() {
  LOGI() << "SetupAudioBuffersForActiveAudioSession";

  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  double sample_rate = session.sampleRate;
  NSTimeInterval io_buffer_duration = session.IOBufferDuration;
  RTCLog(@"%@", session);

  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* webRTCConfig =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  if (sample_rate != webRTCConfig.sampleRate) {
    RTC_LOG(LS_WARNING) << "Unable to set the preferred sample rate";
  }

  if (sample_rate <= DBL_EPSILON && playout_parameters_.sample_rate() > 0) {
    RTCLogError(@"Reported rate is invalid: %f. "
                 "Using %d as sample rate instead.",
                sample_rate, playout_parameters_.sample_rate());
    sample_rate = playout_parameters_.sample_rate();
  }

  playout_parameters_.reset(sample_rate, playout_parameters_.channels(), io_buffer_duration);
  RTC_DCHECK(playout_parameters_.is_complete());
  record_parameters_.reset(sample_rate, record_parameters_.channels(), io_buffer_duration);
  RTC_DCHECK(record_parameters_.is_complete());
  RTC_LOG(LS_INFO) << " frames per I/O buffer: " << playout_parameters_.frames_per_buffer();
  RTC_LOG(LS_INFO) << " bytes per I/O buffer: " << playout_parameters_.GetBytesPerBuffer();

  UpdateAudioDeviceBuffer();

  RTC_DCHECK(audio_device_buffer_);
  fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
}

bool AudioDeviceIOS::CreateAudioUnit() {
  RTC_DCHECK(!audio_unit_);

  audio_unit_.reset(new VoiceProcessingAudioUnit(bypass_voice_processing_, disable_recording_, enableSystemMute_, numChannels_, this));
  if (!audio_unit_->Init()) {
    audio_unit_.reset();
    return false;
  }

  return true;
}

void AudioDeviceIOS::UpdateAudioUnit(bool can_play_or_record) {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Updating audio unit state. CanPlayOrRecord=%d IsInterrupted=%d",
         can_play_or_record,
         is_interrupted_);

  if (is_interrupted_) {
    RTCLog(@"Ignoring audio unit update due to interruption.");
    return;
  }

  if (!audio_is_initialized_) return;

  RTC_DCHECK(audio_unit_);

  bool should_initialize_audio_unit = false;
  bool should_uninitialize_audio_unit = false;
  bool should_start_audio_unit = false;
  bool should_stop_audio_unit = false;

  switch (audio_unit_->GetState()) {
    case VoiceProcessingAudioUnit::kInitRequired:
      RTCLog(@"VPAU state: InitRequired");
      RTC_DCHECK_NOTREACHED();
      break;
    case VoiceProcessingAudioUnit::kUninitialized:
      RTCLog(@"VPAU state: Uninitialized");
      should_initialize_audio_unit = can_play_or_record;
      should_start_audio_unit =
          should_initialize_audio_unit && (playing_.load() || recording_.load());
      break;
    case VoiceProcessingAudioUnit::kInitialized:
      RTCLog(@"VPAU state: Initialized");
      should_start_audio_unit = can_play_or_record && (playing_.load() || recording_.load());
      should_uninitialize_audio_unit = !can_play_or_record;
      break;
    case VoiceProcessingAudioUnit::kStarted:
      RTCLog(@"VPAU state: Started");
      RTC_DCHECK(playing_.load() || recording_.load());
      should_stop_audio_unit = !can_play_or_record;
      should_uninitialize_audio_unit = should_stop_audio_unit;
      break;
  }

  if (should_initialize_audio_unit) {
    RTCLog(@"Initializing audio unit for UpdateAudioUnit");
    ConfigureAudioSession();
    SetupAudioBuffersForActiveAudioSession();
    if (!audio_unit_->Initialize(playout_parameters_.sample_rate())) {
      RTCLogError(@"Failed to initialize audio unit.");
      return;
    }
  }

  if (should_start_audio_unit) {
    RTCLog(@"Starting audio unit for UpdateAudioUnit");

    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    RTCLog(@"%@", session);
    OSStatus result = audio_unit_->Start();
    if (result != noErr) {
      [session notifyAudioUnitStartFailedWithError:result];
      RTCLogError(@"Failed to start audio unit, reason %d", result);
      return;
    }
  }

  if (should_stop_audio_unit) {
    RTCLog(@"Stopping audio unit for UpdateAudioUnit");
    if (!audio_unit_->Stop()) {
      RTCLogError(@"Failed to stop audio unit.");
      PrepareForNewStart();
      return;
    }
    PrepareForNewStart();
  }

  if (should_uninitialize_audio_unit) {
    RTCLog(@"Uninitializing audio unit for UpdateAudioUnit");
    audio_unit_->Uninitialize();
    UnconfigureAudioSession();
  }
}

bool AudioDeviceIOS::ConfigureAudioSession() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  bool success = [session configureWebRTCSession:nil disableRecording:disable_recording_];
  [session unlockForConfiguration];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

bool AudioDeviceIOS::ConfigureAudioSessionLocked() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  bool success = [session configureWebRTCSession:nil disableRecording:disable_recording_];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

void AudioDeviceIOS::UnconfigureAudioSession() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Unconfiguring audio session.");
  if (!has_configured_session_) {
    RTCLogWarning(@"Audio session already unconfigured.");
    return;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  [session unconfigureWebRTCSession:nil];
  if (!disable_recording_) {
    [session endWebRTCSession:nil];
  }
  [session unlockForConfiguration];
  has_configured_session_ = false;
  RTCLog(@"Unconfigured audio session.");
}

bool AudioDeviceIOS::InitPlayOrRecord() {
  LOGI() << "InitPlayOrRecord";
  RTC_DCHECK_RUN_ON(thread_);

  if (!CreateAudioUnit()) {
    return false;
  }

  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];

  [session pushDelegate:audio_session_observer_];
  is_interrupted_ = session.isInterrupted ? true : false;

  [session lockForConfiguration];
  NSError* error = nil;
  if (![session beginWebRTCSession:&error]) {
    [session unlockForConfiguration];
    RTCLogError(@"Failed to begin WebRTC session: %@", error.localizedDescription);
    audio_unit_.reset();
    return false;
  }

  if (session.canPlayOrRecord) {
    if (!ConfigureAudioSessionLocked()) {

      [session unlockForConfiguration];
      audio_unit_.reset();
      return false;
    }
    SetupAudioBuffersForActiveAudioSession();
    audio_unit_->Initialize(playout_parameters_.sample_rate());
  }

  [session unlockForConfiguration];
  return true;
}

void AudioDeviceIOS::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
  RTC_DCHECK_RUN_ON(thread_);

  audio_unit_->Stop();

  audio_unit_.reset();

  io_thread_checker_.Detach();

  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session removeDelegate:audio_session_observer_];

  UnconfigureAudioSession();
}

void AudioDeviceIOS::PrepareForNewStart() {
  LOGI() << "PrepareForNewStart";

  io_thread_checker_.Detach();
}

bool AudioDeviceIOS::IsInterrupted() {
  return is_interrupted_;
}

#pragma mark - Not Implemented

int32_t AudioDeviceIOS::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int16_t AudioDeviceIOS::PlayoutDevices() {

  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int16_t AudioDeviceIOS::RecordingDevices() {

  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

int32_t AudioDeviceIOS::InitSpeaker() {
  return 0;
}

bool AudioDeviceIOS::SpeakerIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetSpeakerMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SpeakerMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::InitMicrophone() {
  return 0;
}

bool AudioDeviceIOS::MicrophoneIsInitialized() const {
  return true;
}

int32_t AudioDeviceIOS::MicrophoneMuteIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneMute(bool enable) {
  isMicrophoneMuted_ = enable;
  if (audio_unit_) {
    audio_unit_->setIsMicrophoneMuted(enable);
  }
  return 0;
}

int32_t AudioDeviceIOS::MicrophoneMute(bool& enabled) const {
  enabled = isMicrophoneMuted_;
  return 0;
}

int32_t AudioDeviceIOS::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetStereoPlayout(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

int32_t AudioDeviceIOS::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceIOS::SetMicrophoneVolume(uint32_t volume) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MicrophoneVolume(uint32_t& volume) const {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::PlayoutDeviceName(uint16_t index,
                                          char name[kAdmMaxDeviceNameSize],
                                          char guid[kAdmMaxGuidSize]) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceIOS::SetRecordingDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceIOS::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceIOS::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

}
}
