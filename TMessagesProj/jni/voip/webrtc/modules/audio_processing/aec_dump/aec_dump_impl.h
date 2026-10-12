/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "api/task_queue/task_queue_base.h"
#include "modules/audio_processing/aec_dump/capture_stream_info.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread_annotations.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "modules/audio_processing/debug.pb.h"
#endif

namespace webrtc {

class AecDumpImpl : public AecDump {
 public:

  AecDumpImpl(FileWrapper debug_file,
              int64_t max_log_size_bytes,
              absl::Nonnull<TaskQueueBase*> worker_queue);
  AecDumpImpl(const AecDumpImpl&) = delete;
  AecDumpImpl& operator=(const AecDumpImpl&) = delete;
  ~AecDumpImpl() override;

  void WriteInitMessage(const ProcessingConfig& api_format,
                        int64_t time_now_ms) override;
  void AddCaptureStreamInput(const AudioFrameView<const float>& src) override;
  void AddCaptureStreamOutput(const AudioFrameView<const float>& src) override;
  void AddCaptureStreamInput(const int16_t* const data,
                             int num_channels,
                             int samples_per_channel) override;
  void AddCaptureStreamOutput(const int16_t* const data,
                              int num_channels,
                              int samples_per_channel) override;
  void AddAudioProcessingState(const AudioProcessingState& state) override;
  void WriteCaptureStreamMessage() override;

  void WriteRenderStreamMessage(const int16_t* const data,
                                int num_channels,
                                int samples_per_channel) override;
  void WriteRenderStreamMessage(
      const AudioFrameView<const float>& src) override;

  void WriteConfig(const InternalAPMConfig& config) override;

  void WriteRuntimeSetting(
      const AudioProcessing::RuntimeSetting& runtime_setting) override;

 private:
  void PostWriteToFileTask(std::unique_ptr<audioproc::Event> event);

  FileWrapper debug_file_;
  int64_t num_bytes_left_for_log_ = 0;
  rtc::RaceChecker race_checker_;
  absl::Nonnull<TaskQueueBase*> worker_queue_;
  CaptureStreamInfo capture_stream_info_;
};
}

#endif
