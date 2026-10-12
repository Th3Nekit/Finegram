/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_OPENSLES_RECORDER_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_OPENSLES_RECORDER_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include <memory>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "sdk/android/src/jni/audio_device/audio_common.h"
#include "sdk/android/src/jni/audio_device/audio_device_module.h"
#include "sdk/android/src/jni/audio_device/opensles_common.h"

namespace webrtc {

class FineAudioBuffer;

namespace jni {

class OpenSLESRecorder : public AudioInput {
 public:

  static const int kNumOfOpenSLESBuffers = 2;

  OpenSLESRecorder(const AudioParameters& audio_parameters,
                   rtc::scoped_refptr<OpenSLEngineManager> engine_manager);
  ~OpenSLESRecorder() override;

  int Init() override;
  int Terminate() override;

  int InitRecording() override;
  bool RecordingIsInitialized() const override;

  int StartRecording() override;
  int StopRecording() override;
  bool Recording() const override;

  void AttachAudioBuffer(AudioDeviceBuffer* audio_buffer) override;

  bool IsAcousticEchoCancelerSupported() const override;
  bool IsNoiseSuppressorSupported() const override;
  int EnableBuiltInAEC(bool enable) override;
  int EnableBuiltInNS(bool enable) override;

 private:

  bool ObtainEngineInterface();

  bool CreateAudioRecorder();
  void DestroyAudioRecorder();

  void AllocateDataBuffers();

  static void SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller,
                                        void* context);
  void ReadBufferQueue();

  bool EnqueueAudioBuffer();

  SLuint32 GetRecordState() const;

  SLAndroidSimpleBufferQueueState GetBufferQueueState() const;

  SLuint32 GetBufferCount();

  void LogBufferState() const;

  SequenceChecker thread_checker_;

  SequenceChecker thread_checker_opensles_;

  const AudioParameters audio_parameters_;

  AudioDeviceBuffer* audio_device_buffer_;

  SLDataFormat_PCM pcm_format_;

  bool initialized_;
  bool recording_;

  const rtc::scoped_refptr<OpenSLEngineManager> engine_manager_;

  SLEngineItf engine_;

  ScopedSLObjectItf recorder_object_;

  SLRecordItf recorder_;

  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  std::unique_ptr<std::unique_ptr<SLint16[]>[]> audio_buffers_;

  int buffer_index_;

  uint32_t last_rec_time_;
};

}

}

#endif
