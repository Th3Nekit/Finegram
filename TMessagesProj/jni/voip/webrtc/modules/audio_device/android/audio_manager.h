/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_
#define MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_

#include <SLES/OpenSLES.h>
#include <jni.h>

#include <memory>

#include "api/sequence_checker.h"
#include "modules/audio_device/android/audio_common.h"
#include "modules/audio_device/android/opensles_common.h"
#include "modules/audio_device/audio_device_config.h"
#include "modules/audio_device/audio_device_generic.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "modules/utility/include/helpers_android.h"
#include "modules/utility/include/jvm_android.h"

namespace webrtc {

class AudioManager {
 public:

  class JavaAudioManager {
   public:
    JavaAudioManager(NativeRegistration* native_registration,
                     std::unique_ptr<GlobalRef> audio_manager);
    ~JavaAudioManager();

    bool Init();
    void Close();
    bool IsCommunicationModeEnabled();
    bool IsDeviceBlacklistedForOpenSLESUsage();

   private:
    std::unique_ptr<GlobalRef> audio_manager_;
    jmethodID init_;
    jmethodID dispose_;
    jmethodID is_communication_mode_enabled_;
    jmethodID is_device_blacklisted_for_open_sles_usage_;
  };

  AudioManager();
  ~AudioManager();

  void SetActiveAudioLayer(AudioDeviceModule::AudioLayer audio_layer);

  SLObjectItf GetOpenSLEngine();

  bool Init();

  bool Close();

  bool IsCommunicationModeEnabled() const;

  const AudioParameters& GetPlayoutAudioParameters();
  const AudioParameters& GetRecordAudioParameters();

  bool IsAcousticEchoCancelerSupported() const;
  bool IsAutomaticGainControlSupported() const;
  bool IsNoiseSuppressorSupported() const;

  bool IsLowLatencyPlayoutSupported() const;
  bool IsLowLatencyRecordSupported() const;

  bool IsStereoPlayoutSupported() const;
  bool IsStereoRecordSupported() const;

  bool IsProAudioSupported() const;

  bool IsAAudioSupported() const;

  int GetDelayEstimateInMilliseconds() const;

 private:

  static void JNICALL CacheAudioParameters(JNIEnv* env,
                                           jobject obj,
                                           jint sample_rate,
                                           jint output_channels,
                                           jint input_channels,
                                           jboolean hardware_aec,
                                           jboolean hardware_agc,
                                           jboolean hardware_ns,
                                           jboolean low_latency_output,
                                           jboolean low_latency_input,
                                           jboolean pro_audio,
                                           jboolean a_audio,
                                           jint output_buffer_size,
                                           jint input_buffer_size,
                                           jlong native_audio_manager);
  void OnCacheAudioParameters(JNIEnv* env,
                              jint sample_rate,
                              jint output_channels,
                              jint input_channels,
                              jboolean hardware_aec,
                              jboolean hardware_agc,
                              jboolean hardware_ns,
                              jboolean low_latency_output,
                              jboolean low_latency_input,
                              jboolean pro_audio,
                              jboolean a_audio,
                              jint output_buffer_size,
                              jint input_buffer_size);

  SequenceChecker thread_checker_;

  JvmThreadConnector attach_thread_if_needed_;

  std::unique_ptr<JNIEnvironment> j_environment_;

  std::unique_ptr<NativeRegistration> j_native_registration_;

  std::unique_ptr<AudioManager::JavaAudioManager> j_audio_manager_;

  AudioDeviceModule::AudioLayer audio_layer_;

  webrtc::ScopedSLObjectItf engine_object_;

  bool initialized_;

  bool hardware_aec_;

  bool hardware_agc_;

  bool hardware_ns_;

  bool low_latency_playout_;

  bool low_latency_record_;

  bool pro_audio_;

  bool a_audio_;

  int delay_estimate_in_milliseconds_;

  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

}

#endif
