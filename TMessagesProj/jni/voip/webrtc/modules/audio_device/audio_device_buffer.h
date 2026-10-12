/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_
#define MODULES_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/timestamp_aligner.h"

namespace webrtc {

const size_t kMaxDeltaTimeInMs = 500;

const size_t kMaxBufferSizeBytes = 3840;

class AudioDeviceBuffer {
 public:
  enum LogState {
    LOG_START = 0,
    LOG_STOP,
    LOG_ACTIVE,
  };

  struct Stats {
    void ResetRecStats() {
      rec_callbacks = 0;
      rec_samples = 0;
      max_rec_level = 0;
    }

    void ResetPlayStats() {
      play_callbacks = 0;
      play_samples = 0;
      max_play_level = 0;
    }

    uint64_t rec_callbacks = 0;

    uint64_t play_callbacks = 0;

    uint64_t rec_samples = 0;

    uint64_t play_samples = 0;

    int16_t max_rec_level = 0;

    int16_t max_play_level = 0;
  };

  explicit AudioDeviceBuffer(TaskQueueFactory* task_queue_factory,
                             bool create_detached = false);
  virtual ~AudioDeviceBuffer();

  int32_t RegisterAudioCallback(AudioTransport* audio_callback);

  void StartPlayout();
  void StartRecording();
  void StopPlayout();
  void StopRecording();

  int32_t SetRecordingSampleRate(uint32_t fsHz);
  int32_t SetPlayoutSampleRate(uint32_t fsHz);
  uint32_t RecordingSampleRate() const;
  uint32_t PlayoutSampleRate() const;

  int32_t SetRecordingChannels(size_t channels);
  int32_t SetPlayoutChannels(size_t channels);
  size_t RecordingChannels() const;
  size_t PlayoutChannels() const;

  virtual int32_t SetRecordedBuffer(const void* audio_buffer,
                                    size_t samples_per_channel);

  virtual int32_t SetRecordedBuffer(
      const void* audio_buffer,
      size_t samples_per_channel,
      absl::optional<int64_t> capture_timestamp_ns);
  virtual void SetVQEData(int play_delay_ms, int rec_delay_ms);
  virtual int32_t DeliverRecordedData();
  uint32_t NewMicLevel() const;

  virtual int32_t RequestPlayoutData(size_t samples_per_channel);
  virtual int32_t GetPlayoutData(void* audio_buffer);

  int32_t SetTypingStatus(bool typing_status);

 private:

  void StartPeriodicLogging();
  void StopPeriodicLogging();

  void LogStats(LogState state);

  void UpdateRecStats(int16_t max_abs, size_t samples_per_channel);
  void UpdatePlayStats(int16_t max_abs, size_t samples_per_channel);

  void ResetRecStats();
  void ResetPlayStats();

  SequenceChecker main_thread_checker_;

  Mutex lock_;

  rtc::TaskQueue task_queue_;

  AudioTransport* audio_transport_cb_;

  std::atomic<uint32_t> rec_sample_rate_;
  std::atomic<uint32_t> play_sample_rate_;

  std::atomic<size_t> rec_channels_;
  std::atomic<size_t> play_channels_;

  bool playing_ RTC_GUARDED_BY(main_thread_checker_);
  bool recording_ RTC_GUARDED_BY(main_thread_checker_);

  rtc::BufferT<int16_t> play_buffer_;

  rtc::BufferT<int16_t> rec_buffer_;

  bool typing_status_;

  int play_delay_ms_;
  int rec_delay_ms_;

  absl::optional<int64_t> capture_timestamp_ns_;

  absl::optional<int64_t> align_offsync_estimation_time_;

  size_t num_stat_reports_ RTC_GUARDED_BY(task_queue_);

  int64_t last_timer_task_time_ RTC_GUARDED_BY(task_queue_);

  int16_t rec_stat_count_;
  int16_t play_stat_count_;

  int64_t play_start_time_ RTC_GUARDED_BY(main_thread_checker_);
  int64_t rec_start_time_ RTC_GUARDED_BY(main_thread_checker_);

  Stats stats_ RTC_GUARDED_BY(lock_);

  Stats last_stats_ RTC_GUARDED_BY(task_queue_);

  bool only_silence_recorded_;

  bool log_stats_ RTC_GUARDED_BY(task_queue_);

  rtc::TimestampAligner timestamp_aligner_;

#ifdef AUDIO_DEVICE_PLAYS_SINUS_TONE
  double phase_;
#endif
};

}

#endif
