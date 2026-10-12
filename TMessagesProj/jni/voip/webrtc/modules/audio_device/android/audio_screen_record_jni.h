/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AUDIO_SCREEN_RECORD_JNI_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AUDIO_SCREEN_RECORD_JNI_H_

#include <jni.h>

#include <memory>

#include "api/sequence_checker.h"
#include "modules/audio_device/android/audio_manager.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "modules/utility/include/helpers_android.h"
#include "modules/utility/include/jvm_android.h"

namespace webrtc {

class AudioScreenRecordJni {
 public:

  class JavaAudioRecord {
   public:
    JavaAudioRecord(NativeRegistration* native_registration,
                    std::unique_ptr<GlobalRef> audio_track);

    int InitRecording(int sample_rate, size_t channels);
    bool StartRecording();
    bool StopRecording();
    bool EnableBuiltInAEC(bool enable);
    bool EnableBuiltInNS(bool enable);

   private:
    std::unique_ptr<GlobalRef> audio_record_;
    jmethodID init_recording_;
    jmethodID start_recording_;
    jmethodID stop_recording_;
    jmethodID enable_built_in_aec_;
    jmethodID enable_built_in_ns_;
  };

  explicit AudioScreenRecordJni(AudioManager* audio_manager);
  ~AudioScreenRecordJni();

  int32_t Init();
  int32_t Terminate();

  int32_t InitRecording();
  bool RecordingIsInitialized() const { return initialized_; }

  int32_t StartRecording();
  int32_t StopRecording();
  bool Recording() const { return recording_; }

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  int32_t EnableBuiltInAEC(bool enable);
  int32_t EnableBuiltInAGC(bool enable);
  int32_t EnableBuiltInNS(bool enable);

 private:

  static void JNICALL CacheDirectBufferAddress(JNIEnv* env,
                                               jobject obj,
                                               jobject byte_buffer,
                                               jlong nativeAudioRecord);
  void OnCacheDirectBufferAddress(JNIEnv* env, jobject byte_buffer);

  static void JNICALL DataIsRecorded(JNIEnv* env,
                                     jobject obj,
                                     jint length,
                                     jlong nativeAudioRecord);
  void OnDataIsRecorded(int length);

  SequenceChecker thread_checker_;

  SequenceChecker thread_checker_java_;

  JvmThreadConnector attach_thread_if_needed_;

  std::unique_ptr<JNIEnvironment> j_environment_;

  std::unique_ptr<NativeRegistration> j_native_registration_;

  std::unique_ptr<AudioScreenRecordJni::JavaAudioRecord> j_audio_record_;

  const AudioManager* audio_manager_;

  const AudioParameters audio_parameters_;

  int total_delay_in_milliseconds_;

  void* direct_buffer_address_;

  size_t direct_buffer_capacity_in_bytes_;

  size_t frames_per_buffer_;

  bool initialized_;

  bool recording_;

  AudioDeviceBuffer* audio_device_buffer_;
};

}

#endif
