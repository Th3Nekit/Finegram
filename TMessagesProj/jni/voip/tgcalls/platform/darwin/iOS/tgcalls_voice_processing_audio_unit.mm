/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "tgcalls_voice_processing_audio_unit.h"

#include "rtc_base/checks.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"
#import "platform/darwin/iOS/RTCAudioSessionConfiguration.h"

#if !defined(NDEBUG)
static void LogStreamDescription(AudioStreamBasicDescription description) {
  char formatIdString[5];
  UInt32 formatId = CFSwapInt32HostToBig(description.mFormatID);
  bcopy(&formatId, formatIdString, 4);
  formatIdString[4] = '\0';
  RTCLog(@"AudioStreamBasicDescription: {\n"
          "  mSampleRate: %.2f\n"
          "  formatIDString: %s\n"
          "  mFormatFlags: 0x%X\n"
          "  mBytesPerPacket: %u\n"
          "  mFramesPerPacket: %u\n"
          "  mBytesPerFrame: %u\n"
          "  mChannelsPerFrame: %u\n"
          "  mBitsPerChannel: %u\n"
          "  mReserved: %u\n}",
         description.mSampleRate, formatIdString,
         static_cast<unsigned int>(description.mFormatFlags),
         static_cast<unsigned int>(description.mBytesPerPacket),
         static_cast<unsigned int>(description.mFramesPerPacket),
         static_cast<unsigned int>(description.mBytesPerFrame),
         static_cast<unsigned int>(description.mChannelsPerFrame),
         static_cast<unsigned int>(description.mBitsPerChannel),
         static_cast<unsigned int>(description.mReserved));
}
#endif

namespace webrtc {
namespace tgcalls_ios_adm {

static const int kMaxNumberOfAudioUnitInitializeAttempts = 5;

static const AudioUnitElement kInputBus = 1;

static const AudioUnitElement kOutputBus = 0;

static OSStatus GetAGCState(AudioUnit audio_unit, UInt32* enabled) {
  RTC_DCHECK(audio_unit);
  UInt32 size = sizeof(*enabled);
  OSStatus result = AudioUnitGetProperty(audio_unit,
                                         kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                                         kAudioUnitScope_Global,
                                         kInputBus,
                                         enabled,
                                         &size);
  RTCLog(@"VPIO unit AGC: %u", static_cast<unsigned int>(*enabled));
  return result;
}

VoiceProcessingAudioUnit::VoiceProcessingAudioUnit(bool bypass_voice_processing, bool disable_recording, bool enableSystemMute, int numChannels,
                                                   VoiceProcessingAudioUnitObserver* observer)
    : bypass_voice_processing_(bypass_voice_processing),
      disable_recording_(disable_recording),
      numChannels_(numChannels),
      enableSystemMute_(enableSystemMute),
      observer_(observer),
      vpio_unit_(nullptr),
      state_(kInitRequired) {
  RTC_DCHECK(observer);
}

VoiceProcessingAudioUnit::~VoiceProcessingAudioUnit() {
  DisposeAudioUnit();
}

const UInt32 VoiceProcessingAudioUnit::kBytesPerSample = 2;

bool VoiceProcessingAudioUnit::Init() {
  RTC_DCHECK_EQ(state_, kInitRequired);

  AudioComponentDescription vpio_unit_description;
  vpio_unit_description.componentType = kAudioUnitType_Output;

  if (disable_recording_) {
    vpio_unit_description.componentSubType = kAudioUnitSubType_RemoteIO;
  } else {
    vpio_unit_description.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
  }
  vpio_unit_description.componentManufacturer = kAudioUnitManufacturer_Apple;
  vpio_unit_description.componentFlags = 0;
  vpio_unit_description.componentFlagsMask = 0;

  AudioComponent found_vpio_unit_ref =
      AudioComponentFindNext(nullptr, &vpio_unit_description);

  OSStatus result = noErr;
  result = AudioComponentInstanceNew(found_vpio_unit_ref, &vpio_unit_);
  if (result != noErr) {
    vpio_unit_ = nullptr;
    RTCLogError(@"AudioComponentInstanceNew failed. Error=%ld.", (long)result);
    return false;
  }

    if (!disable_recording_) {

        UInt32 enable_input = 1;
        result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Input, kInputBus, &enable_input,
                                      sizeof(enable_input));
        if (result != noErr) {
            DisposeAudioUnit();
            RTCLogError(@"Failed to enable input on input scope of input element. "
                        "Error=%ld.",
                        (long)result);
            return false;
        }
    }

  UInt32 enable_output = 1;
  result = AudioUnitSetProperty(vpio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, kOutputBus,
                                &enable_output, sizeof(enable_output));
  if (result != noErr) {
    DisposeAudioUnit();
    RTCLogError(@"Failed to enable output on output scope of output element. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  AURenderCallbackStruct render_callback;
  render_callback.inputProc = OnGetPlayoutData;
  render_callback.inputProcRefCon = this;
  result = AudioUnitSetProperty(
      vpio_unit_, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
      kOutputBus, &render_callback, sizeof(render_callback));
  if (result != noErr) {
    DisposeAudioUnit();
    RTCLogError(@"Failed to specify the render callback on the output bus. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  if (!disable_recording_) {

      UInt32 flag = 0;
      result = AudioUnitSetProperty(
                                    vpio_unit_, kAudioUnitProperty_ShouldAllocateBuffer,
                                    kAudioUnitScope_Output, kInputBus, &flag, sizeof(flag));
      if (result != noErr) {
          DisposeAudioUnit();
          RTCLogError(@"Failed to disable buffer allocation on the input bus. "
                      "Error=%ld.",
                      (long)result);
          return false;
      }

      AURenderCallbackStruct input_callback;
      input_callback.inputProc = OnDeliverRecordedData;
      input_callback.inputProcRefCon = this;
      result = AudioUnitSetProperty(vpio_unit_,
                                    kAudioOutputUnitProperty_SetInputCallback,
                                    kAudioUnitScope_Global, kInputBus,
                                    &input_callback, sizeof(input_callback));
      if (result != noErr) {
          DisposeAudioUnit();
          RTCLogError(@"Failed to specify the input callback on the input bus. "
                      "Error=%ld.",
                      (long)result);
          return false;
      }

      if (@available(iOS 15.0, macOS 14.0, *)) {
          VoiceProcessingAudioUnit *audio_unit = this;
          AUVoiceIOMutedSpeechActivityEventListener listener = ^(AUVoiceIOSpeechActivityEvent event) {
              if (event == kAUVoiceIOSpeechActivityHasStarted) {
                  if (audio_unit->observer_) {
                      audio_unit->observer_->OnMutedSpeechStatusChanged(true);
                  }
              } else if (event == kAUVoiceIOSpeechActivityHasEnded) {
                  if (audio_unit->observer_) {
                      audio_unit->observer_->OnMutedSpeechStatusChanged(false);
                  }
              }
          };
          result = AudioUnitSetProperty(vpio_unit_, kAUVoiceIOProperty_MutedSpeechActivityEventListener, kAudioUnitScope_Global, kInputBus, &listener, sizeof(AUVoiceIOMutedSpeechActivityEventListener));
      }
  }

    if (enableSystemMute_) {
        UInt32 muteUplinkOutput = isMicrophoneMuted_ ? 1 : 0;
        OSStatus result = AudioUnitSetProperty(vpio_unit_, kAUVoiceIOProperty_MuteOutput, kAudioUnitScope_Global, kInputBus, &muteUplinkOutput, sizeof(muteUplinkOutput));
        if (result != noErr) {
            RTCLogError(@"Failed to set kAUVoiceIOProperty_MuteOutput. Error=%ld", (long)result);
        }
    }

  state_ = kUninitialized;
  return true;
}

VoiceProcessingAudioUnit::State VoiceProcessingAudioUnit::GetState() const {
  return state_;
}

bool VoiceProcessingAudioUnit::Initialize(Float64 sample_rate) {
  RTC_DCHECK_GE(state_, kUninitialized);
  RTCLog(@"Initializing audio unit with sample rate: %f", sample_rate);

  OSStatus result = noErr;
  AudioStreamBasicDescription outputFormat = GetFormat(sample_rate, numChannels_);
  AudioStreamBasicDescription inputFormat = GetFormat(sample_rate, 1);
  UInt32 size = sizeof(outputFormat);
#if !defined(NDEBUG)
  LogStreamDescription(outputFormat);
#endif

    if (!disable_recording_) {

        result =
        AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output, kInputBus, &inputFormat, size);
        if (result != noErr) {
            RTCLogError(@"Failed to set format on output scope of input bus. "
                        "Error=%ld.",
                        (long)result);
            return false;
        }
    }

  result =
      AudioUnitSetProperty(vpio_unit_, kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input, kOutputBus, &outputFormat, size);
  if (result != noErr) {
    RTCLogError(@"Failed to set format on input scope of output bus. "
                 "Error=%ld.",
                (long)result);
    return false;
  }

  int failed_initalize_attempts = 0;
  result = AudioUnitInitialize(vpio_unit_);
  while (result != noErr) {
    RTCLogError(@"Failed to initialize the Voice Processing I/O unit. "
                 "Error=%ld.",
                (long)result);
    ++failed_initalize_attempts;
    if (failed_initalize_attempts == kMaxNumberOfAudioUnitInitializeAttempts) {

      RTCLogError(@"Too many initialization attempts.");
      return false;
    }
    RTCLog(@"Pause 100ms and try audio unit initialization again...");
    [NSThread sleepForTimeInterval:0.1f];
    result = AudioUnitInitialize(vpio_unit_);
  }
  if (result == noErr) {
    RTCLog(@"Voice Processing I/O unit is now initialized.");
  }

  if (bypass_voice_processing_) {

    UInt32 toggle = 1;
    result = AudioUnitSetProperty(vpio_unit_,
                                  kAUVoiceIOProperty_BypassVoiceProcessing,
                                  kAudioUnitScope_Global,
                                  kInputBus,
                                  &toggle,
                                  sizeof(toggle));
    if (result == noErr) {
      RTCLog(@"Successfully bypassed voice processing.");
    } else {
      RTCLogError(@"Failed to bypass voice processing. Error=%ld.", (long)result);
    }
    state_ = kInitialized;
    return true;
  }

  if (!disable_recording_) {

  int agc_was_enabled_by_default = 0;
  UInt32 agc_is_enabled = 0;
  result = GetAGCState(vpio_unit_, &agc_is_enabled);
  if (result != noErr) {
    RTCLogError(@"Failed to get AGC state (1st attempt). "
                 "Error=%ld.",
                (long)result);

    RTC_HISTOGRAM_COUNTS_SPARSE_100000(
        "WebRTC.Audio.GetAGCStateErrorCode1", (-1) * result);
  }

  if (agc_is_enabled) {

    agc_was_enabled_by_default = 1;
  } else {

    UInt32 enable_agc = 1;
    result =
        AudioUnitSetProperty(vpio_unit_,
                             kAUVoiceIOProperty_VoiceProcessingEnableAGC,
                             kAudioUnitScope_Global, kInputBus, &enable_agc,
                             sizeof(enable_agc));
    if (result != noErr) {
      RTCLogError(@"Failed to enable the built-in AGC. "
                   "Error=%ld.",
                  (long)result);
      RTC_HISTOGRAM_COUNTS_SPARSE_100000(
          "WebRTC.Audio.SetAGCStateErrorCode", (-1) * result);
    }
    result = GetAGCState(vpio_unit_, &agc_is_enabled);
    if (result != noErr) {
      RTCLogError(@"Failed to get AGC state (2nd attempt). "
                   "Error=%ld.",
                  (long)result);
      RTC_HISTOGRAM_COUNTS_SPARSE_100000(
          "WebRTC.Audio.GetAGCStateErrorCode2", (-1) * result);
    }
  }

  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.BuiltInAGCWasEnabledByDefault",
                        agc_was_enabled_by_default);
  RTCLog(@"WebRTC.Audio.BuiltInAGCWasEnabledByDefault: %d",
         agc_was_enabled_by_default);

  RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.BuiltInAGCIsEnabled", agc_is_enabled);
  RTCLog(@"WebRTC.Audio.BuiltInAGCIsEnabled: %u",
         static_cast<unsigned int>(agc_is_enabled));
  }

  state_ = kInitialized;
  return true;
}

OSStatus VoiceProcessingAudioUnit::Start() {
  RTC_DCHECK_GE(state_, kUninitialized);
  RTCLog(@"Starting audio unit.");

  OSStatus result = AudioOutputUnitStart(vpio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to start audio unit. Error=%ld", (long)result);
    return result;
  } else {
    RTCLog(@"Started audio unit");
  }
  state_ = kStarted;
  return noErr;
}

bool VoiceProcessingAudioUnit::Stop() {
  RTC_DCHECK_GE(state_, kUninitialized);
  RTCLog(@"Stopping audio unit.");

  OSStatus result = AudioOutputUnitStop(vpio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to stop audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Stopped audio unit");
  }

  state_ = kInitialized;
  return true;
}

bool VoiceProcessingAudioUnit::Uninitialize() {
  RTC_DCHECK_GE(state_, kUninitialized);
  RTCLog(@"Unintializing audio unit.");

  OSStatus result = AudioUnitUninitialize(vpio_unit_);
  if (result != noErr) {
    RTCLogError(@"Failed to uninitialize audio unit. Error=%ld", (long)result);
    return false;
  } else {
    RTCLog(@"Uninitialized audio unit.");
  }

  state_ = kUninitialized;
  return true;
}

void VoiceProcessingAudioUnit::setIsMicrophoneMuted(bool isMuted) {
    isMicrophoneMuted_ = isMuted;
    if (enableSystemMute_) {
        if (vpio_unit_ && state_ == kStarted) {
            UInt32 muteUplinkOutput = isMuted ? 1 : 0;
            OSStatus result = AudioUnitSetProperty(vpio_unit_, kAUVoiceIOProperty_MuteOutput, kAudioUnitScope_Global, kInputBus, &muteUplinkOutput, sizeof(muteUplinkOutput));
            if (result != noErr) {
                RTCLogError(@"Failed to set kAUVoiceIOProperty_MuteOutput. Error=%ld", (long)result);
            }
        }
    }
}

OSStatus VoiceProcessingAudioUnit::Render(AudioUnitRenderActionFlags* flags,
                                          const AudioTimeStamp* time_stamp,
                                          UInt32 output_bus_number,
                                          UInt32 num_frames,
                                          AudioBufferList* io_data) {
  RTC_DCHECK(vpio_unit_) << "Init() not called.";

  OSStatus result = AudioUnitRender(vpio_unit_, flags, time_stamp,
                                    output_bus_number, num_frames, io_data);
  if (result != noErr) {
    RTCLogError(@"Failed to render audio unit. Error=%ld", (long)result);
  }
  return result;
}

OSStatus VoiceProcessingAudioUnit::OnGetPlayoutData(
    void* in_ref_con,
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList* io_data) {
  VoiceProcessingAudioUnit* audio_unit =
      static_cast<VoiceProcessingAudioUnit*>(in_ref_con);
  return audio_unit->NotifyGetPlayoutData(flags, time_stamp, bus_number,
                                          num_frames, io_data);
}

OSStatus VoiceProcessingAudioUnit::OnDeliverRecordedData(
    void* in_ref_con,
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList* io_data) {
  VoiceProcessingAudioUnit* audio_unit =
      static_cast<VoiceProcessingAudioUnit*>(in_ref_con);
  return audio_unit->NotifyDeliverRecordedData(flags, time_stamp, bus_number,
                                               num_frames, io_data);
}

OSStatus VoiceProcessingAudioUnit::NotifyGetPlayoutData(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList* io_data) {
  return observer_->OnGetPlayoutData(flags, time_stamp, bus_number, num_frames,
                                     io_data);
}

OSStatus VoiceProcessingAudioUnit::NotifyDeliverRecordedData(
    AudioUnitRenderActionFlags* flags,
    const AudioTimeStamp* time_stamp,
    UInt32 bus_number,
    UInt32 num_frames,
    AudioBufferList* io_data) {
  return observer_->OnDeliverRecordedData(flags, time_stamp, bus_number,
                                          num_frames, io_data);
}

AudioStreamBasicDescription VoiceProcessingAudioUnit::GetFormat(
    Float64 sample_rate, int numChannels) const {

  AudioStreamBasicDescription format;
  format.mSampleRate = sample_rate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
  format.mBytesPerPacket = kBytesPerSample * numChannels;
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = kBytesPerSample * numChannels;
  format.mChannelsPerFrame = numChannels;
  format.mBitsPerChannel = 8 * kBytesPerSample;
  return format;
}

void VoiceProcessingAudioUnit::DisposeAudioUnit() {
  if (vpio_unit_) {
    switch (state_) {
      case kStarted:
        Stop();
        [[fallthrough]];
      case kInitialized:
        Uninitialize();
        break;
      case kUninitialized:
      case kInitRequired:
        break;
    }

    RTCLog(@"Disposing audio unit.");
    OSStatus result = AudioComponentInstanceDispose(vpio_unit_);
    if (result != noErr) {
      RTCLogError(@"AudioComponentInstanceDispose failed. Error=%ld.",
                  (long)result);
    }
    vpio_unit_ = nullptr;
  }
}

}
}
