/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_TRACK_JNI_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AUDIO_TRACK_JNI_H_

#include <jni.h>

#include <memory>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "sdk/android/src/jni/audio_device/audio_common.h"
#include "sdk/android/src/jni/audio_device/audio_device_module.h"

namespace webrtc {

namespace jni {

class AudioTrackJni : public AudioOutput {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaWebRtcAudioTrack(
      JNIEnv* env,
      const JavaRef<jobject>& j_context,
      const JavaRef<jobject>& j_audio_manager);

  AudioTrackJni(JNIEnv* env,
                const AudioParameters& audio_parameters,
                const JavaRef<jobject>& j_webrtc_audio_track);
  ~AudioTrackJni() override;

  int32_t Init() override;
  int32_t Terminate() override;

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;

  bool SpeakerVolumeIsAvailable() override;
  int SetSpeakerVolume(uint32_t volume) override;
  absl::optional<uint32_t> SpeakerVolume() const override;
  absl::optional<uint32_t> MaxSpeakerVolume() const override;
  absl::optional<uint32_t> MinSpeakerVolume() const override;
  int GetPlayoutUnderrunCount() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  void CacheDirectBufferAddress(JNIEnv* env,
                                const JavaParamRef<jobject>& byte_buffer);

  void GetPlayoutData(JNIEnv* env, size_t length);

 private:

  SequenceChecker thread_checker_;

  SequenceChecker thread_checker_java_;

  JNIEnv* env_ = nullptr;
  ScopedJavaGlobalRef<jobject> j_audio_track_;

  const AudioParameters audio_parameters_;

  void* direct_buffer_address_;

  size_t direct_buffer_capacity_in_bytes_;

  size_t frames_per_buffer_;

  bool initialized_;

  bool playing_;

  AudioDeviceBuffer* audio_device_buffer_;
};

}

}

#endif
