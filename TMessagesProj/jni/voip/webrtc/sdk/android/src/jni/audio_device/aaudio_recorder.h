/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AAUDIO_RECORDER_H_
#define SDK_ANDROID_SRC_JNI_AUDIO_DEVICE_AAUDIO_RECORDER_H_

#include <aaudio/AAudio.h>

#include <memory>

#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "sdk/android/src/jni/audio_device/aaudio_wrapper.h"
#include "sdk/android/src/jni/audio_device/audio_device_module.h"

namespace webrtc {

class FineAudioBuffer;
class AudioDeviceBuffer;

namespace jni {

class AAudioRecorder : public AudioInput, public AAudioObserverInterface {
 public:
  explicit AAudioRecorder(const AudioParameters& audio_parameters);
  ~AAudioRecorder() override;

  int Init() override;
  int Terminate() override;

  int InitRecording() override;
  bool RecordingIsInitialized() const override;

  int StartRecording() override;
  int StopRecording() override;
  bool Recording() const override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  bool IsAcousticEchoCancelerSupported() const override;
  bool IsNoiseSuppressorSupported() const override;
  int EnableBuiltInAEC(bool enable) override;
  int EnableBuiltInNS(bool enable) override;

 protected:

  aaudio_data_callback_result_t OnDataCallback(void* audio_data,
                                               int32_t num_frames) override;

  void OnErrorCallback(aaudio_result_t error) override;

 private:

  void HandleStreamDisconnected();

  SequenceChecker thread_checker_;

  SequenceChecker thread_checker_aaudio_;

  TaskQueueBase* main_thread_;

  AAudioWrapper aaudio_;

  AudioDeviceBuffer* audio_device_buffer_ = nullptr;

  bool initialized_ = false;
  bool recording_ = false;

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  int32_t overflow_count_ = 0;

  double latency_millis_ = 0;

  bool first_data_callback_ = true;
};

}

}

#endif
