/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TGCALLS_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_H_
#define TGCALLS_AUDIO_VOICE_PROCESSING_AUDIO_UNIT_H_

#include <AudioUnit/AudioUnit.h>

namespace webrtc {
namespace tgcalls_ios_adm {

class VoiceProcessingAudioUnitObserver {
 public:

  virtual OSStatus OnDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                         const AudioTimeStamp* time_stamp,
                                         UInt32 bus_number,
                                         UInt32 num_frames,
                                         AudioBufferList* io_data) = 0;

  virtual OSStatus OnGetPlayoutData(AudioUnitRenderActionFlags* io_action_flags,
                                    const AudioTimeStamp* time_stamp,
                                    UInt32 bus_number,
                                    UInt32 num_frames,
                                    AudioBufferList* io_data) = 0;

  virtual void OnMutedSpeechStatusChanged(bool isDetectingSpeech) {}

 protected:
  ~VoiceProcessingAudioUnitObserver() {}
};

class VoiceProcessingAudioUnit {
 public:
  VoiceProcessingAudioUnit(bool bypass_voice_processing, bool disable_recording, bool enableSystemMute, int numChannels,
                           VoiceProcessingAudioUnitObserver* observer);
  ~VoiceProcessingAudioUnit();

  enum State : int32_t {

    kInitRequired,

    kUninitialized,

    kInitialized,

    kStarted,
  };

  static const UInt32 kBytesPerSample;

  bool Init();

  VoiceProcessingAudioUnit::State GetState() const;

  bool Initialize(Float64 sample_rate);

  OSStatus Start();

  bool Stop();

  bool Uninitialize();

  void setIsMicrophoneMuted(bool isMuted);

  OSStatus Render(AudioUnitRenderActionFlags* flags,
                  const AudioTimeStamp* time_stamp,
                  UInt32 output_bus_number,
                  UInt32 num_frames,
                  AudioBufferList* io_data);

 private:

  static OSStatus OnGetPlayoutData(void* in_ref_con,
                                   AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* time_stamp,
                                   UInt32 bus_number,
                                   UInt32 num_frames,
                                   AudioBufferList* io_data);
  static OSStatus OnDeliverRecordedData(void* in_ref_con,
                                        AudioUnitRenderActionFlags* flags,
                                        const AudioTimeStamp* time_stamp,
                                        UInt32 bus_number,
                                        UInt32 num_frames,
                                        AudioBufferList* io_data);

  OSStatus NotifyGetPlayoutData(AudioUnitRenderActionFlags* flags,
                                const AudioTimeStamp* time_stamp,
                                UInt32 bus_number,
                                UInt32 num_frames,
                                AudioBufferList* io_data);

  OSStatus NotifyDeliverRecordedData(AudioUnitRenderActionFlags* flags,
                                     const AudioTimeStamp* time_stamp,
                                     UInt32 bus_number,
                                     UInt32 num_frames,
                                     AudioBufferList* io_data);

  AudioStreamBasicDescription GetFormat(Float64 sample_rate, int numChannels) const;

  void DisposeAudioUnit();

  const bool bypass_voice_processing_;
  const bool disable_recording_;
  const int numChannels_;
  bool enableSystemMute_ = false;
  bool isMicrophoneMuted_ = true;
  VoiceProcessingAudioUnitObserver* observer_;
  AudioUnit vpio_unit_;
  VoiceProcessingAudioUnit::State state_;
};
}
}

#endif
