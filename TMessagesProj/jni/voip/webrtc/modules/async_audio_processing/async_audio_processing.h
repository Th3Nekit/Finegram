/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_ASYNC_AUDIO_PROCESSING_ASYNC_AUDIO_PROCESSING_H_
#define MODULES_ASYNC_AUDIO_PROCESSING_ASYNC_AUDIO_PROCESSING_H_

#include <memory>

#include "api/audio/audio_frame_processor.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

class AudioFrame;
class TaskQueueFactory;

class AsyncAudioProcessing final {
 public:

  class Factory : public rtc::RefCountInterface {
   public:
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory();
    Factory(AudioFrameProcessor& frame_processor,
            TaskQueueFactory& task_queue_factory);
    Factory(std::unique_ptr<AudioFrameProcessor> frame_processor,
            TaskQueueFactory& task_queue_factory);

    std::unique_ptr<AsyncAudioProcessing> CreateAsyncAudioProcessing(
        AudioFrameProcessor::OnAudioFrameCallback on_frame_processed_callback);

   private:

    AudioFrameProcessor& frame_processor_;
    std::unique_ptr<AudioFrameProcessor> owned_frame_processor_;
    TaskQueueFactory& task_queue_factory_;
  };

  AsyncAudioProcessing(const AsyncAudioProcessing&) = delete;
  AsyncAudioProcessing& operator=(const AsyncAudioProcessing&) = delete;

  ~AsyncAudioProcessing();

  AsyncAudioProcessing(
      AudioFrameProcessor& frame_processor,
      TaskQueueFactory& task_queue_factory,
      AudioFrameProcessor::OnAudioFrameCallback on_frame_processed_callback);

  AsyncAudioProcessing(
      std::unique_ptr<AudioFrameProcessor> frame_processor,
      TaskQueueFactory& task_queue_factory,
      AudioFrameProcessor::OnAudioFrameCallback on_frame_processed_callback);

  void Process(std::unique_ptr<AudioFrame> frame);

 private:
  AudioFrameProcessor::OnAudioFrameCallback on_frame_processed_callback_;

  AudioFrameProcessor& frame_processor_;
  std::unique_ptr<AudioFrameProcessor> owned_frame_processor_;
  rtc::TaskQueue task_queue_;
};

}

#endif
