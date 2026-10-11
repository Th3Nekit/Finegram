/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_

#include <jni.h>

#include <memory>

#include "api/sequence_checker.h"
#include "modules/audio_device/android/audio_common.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "modules/utility/include/helpers_android.h"
#include "modules/utility/include/jvm_android.h"

namespace webrtc {

class AudioTrackJni {
 public:

  class JavaAudioTrack {
   public:
    JavaAudioTrack(NativeRegistration* native_registration,
                   std::unique_ptr<GlobalRef> audio_track);
    ~JavaAudioTrack();

    bool InitPlayout(int sample_rate, int channels);
    bool StartPlayout();
    bool StopPlayout();
    bool SetStreamVolume(int volume);
    int GetStreamMaxVolume();
    int GetStreamVolume();

   private:
    std::unique_ptr<GlobalRef> audio_track_;
    jmethodID init_playout_;
    jmethodID start_playout_;
    jmethodID stop_playout_;
    jmethodID set_stream_volume_;
    jmethodID get_stream_max_volume_;
    jmethodID get_stream_volume_;
    jmethodID get_buffer_size_in_frames_;
  };

  explicit AudioTrackJni(AudioManager* audio_manager);
  ~AudioTrackJni();

  int32_t Init();
  int32_t Terminate();

  int32_t InitPlayout();
  bool PlayoutIsInitialized() const { return initialized_; }

  int32_t StartPlayout();
  int32_t StopPlayout();
  bool Playing() const { return playing_; }

  int SpeakerVolumeIsAvailable(bool& available);
  int SetSpeakerVolume(uint32_t volume);
  int SpeakerVolume(uint32_t& volume) const;
  int MaxSpeakerVolume(uint32_t& max_volume) const;
  int MinSpeakerVolume(uint32_t& min_volume) const;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 private:

  static void JNICALL CacheDirectBufferAddress(JNIEnv* env,
                                               jobject obj,
                                               jobject byte_buffer,
                                               jlong nativeAudioTrack);
  void OnCacheDirectBufferAddress(JNIEnv* env, jobject byte_buffer);

  static void JNICALL GetPlayoutData(JNIEnv* env,
                                     jobject obj,
                                     jint length,
                                     jlong nativeAudioTrack);
  void OnGetPlayoutData(size_t length);

  SequenceChecker thread_checker_;

  SequenceChecker thread_checker_java_;

  JvmThreadConnector attach_thread_if_needed_;

  std::unique_ptr<JNIEnvironment> j_environment_;

  std::unique_ptr<NativeRegistration> j_native_registration_;

  std::unique_ptr<AudioTrackJni::JavaAudioTrack> j_audio_track_;

  const AudioParameters audio_parameters_;

  void* direct_buffer_address_;

  size_t direct_buffer_capacity_in_bytes_;

  size_t frames_per_buffer_;

  bool initialized_;

  bool playing_;

  AudioDeviceBuffer* audio_device_buffer_;
};

}

#endif
