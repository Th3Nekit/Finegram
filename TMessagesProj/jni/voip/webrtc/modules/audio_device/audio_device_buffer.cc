/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/audio_device_buffer.h"

#include <string.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

static const char kTimerQueueName[] = "AudioDeviceBufferTimer";

static const size_t kTimerIntervalInSeconds = 10;
static const size_t kTimerIntervalInMilliseconds =
    kTimerIntervalInSeconds * rtc::kNumMillisecsPerSec;

static const size_t kMinValidCallTimeTimeInSeconds = 10;
static const size_t kMinValidCallTimeTimeInMilliseconds =
    kMinValidCallTimeTimeInSeconds * rtc::kNumMillisecsPerSec;
#ifdef AUDIO_DEVICE_PLAYS_SINUS_TONE
static const double k2Pi = 6.28318530717959;
#endif

AudioDeviceBuffer::AudioDeviceBuffer(TaskQueueFactory* task_queue_factory,
                                     bool create_detached)
    : task_queue_(task_queue_factory->CreateTaskQueue(
          kTimerQueueName,
          TaskQueueFactory::Priority::NORMAL)),
      audio_transport_cb_(nullptr),
      rec_sample_rate_(0),
      play_sample_rate_(0),
      rec_channels_(0),
      play_channels_(0),
      playing_(false),
      recording_(false),
      typing_status_(false),
      play_delay_ms_(0),
      rec_delay_ms_(0),
      num_stat_reports_(0),
      last_timer_task_time_(0),
      rec_stat_count_(0),
      play_stat_count_(0),
      play_start_time_(0),
      only_silence_recorded_(true),
      log_stats_(false) {
  RTC_LOG(LS_INFO) << "AudioDeviceBuffer::ctor";
#ifdef AUDIO_DEVICE_PLAYS_SINUS_TONE
  phase_ = 0.0;
  RTC_LOG(LS_WARNING) << "AUDIO_DEVICE_PLAYS_SINUS_TONE is defined!";
#endif
  if (create_detached) {
    main_thread_checker_.Detach();
  }
}

AudioDeviceBuffer::~AudioDeviceBuffer() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  RTC_DCHECK(!playing_);
  RTC_DCHECK(!recording_);
  RTC_LOG(LS_INFO) << "AudioDeviceBuffer::~dtor";
}

int32_t AudioDeviceBuffer::RegisterAudioCallback(
    AudioTransport* audio_callback) {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  if (playing_ || recording_) {
    RTC_LOG(LS_ERROR) << "Failed to set audio transport since media was active";
    return -1;
  }
  audio_transport_cb_ = audio_callback;
  return 0;
}

void AudioDeviceBuffer::StartPlayout() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);

  if (playing_) {
    return;
  }
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  task_queue_.PostTask([this] { ResetPlayStats(); });

  if (!recording_) {
    StartPeriodicLogging();
  }
  const int64_t now_time = rtc::TimeMillis();

  play_start_time_ = now_time;
  playing_ = true;
}

void AudioDeviceBuffer::StartRecording() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (recording_) {
    return;
  }
  RTC_DLOG(LS_INFO) << __FUNCTION__;

  task_queue_.PostTask([this] { ResetRecStats(); });

  if (!playing_) {
    StartPeriodicLogging();
  }

  rec_start_time_ = rtc::TimeMillis();
  recording_ = true;

  only_silence_recorded_ = true;
}

void AudioDeviceBuffer::StopPlayout() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (!playing_) {
    return;
  }
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  playing_ = false;

  if (!recording_) {
    StopPeriodicLogging();
  }
  RTC_LOG(LS_INFO) << "total playout time: "
                   << rtc::TimeSince(play_start_time_);
}

void AudioDeviceBuffer::StopRecording() {
  RTC_DCHECK_RUN_ON(&main_thread_checker_);
  if (!recording_) {
    return;
  }
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  recording_ = false;

  if (!playing_) {
    StopPeriodicLogging();
  }

  const size_t time_since_start = rtc::TimeSince(rec_start_time_);
  if (time_since_start > kMinValidCallTimeTimeInMilliseconds) {
    const int only_zeros = static_cast<int>(only_silence_recorded_);
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.RecordedOnlyZeros", only_zeros);
    RTC_LOG(LS_INFO) << "HISTOGRAM(WebRTC.Audio.RecordedOnlyZeros): "
                     << only_zeros;
  }
  RTC_LOG(LS_INFO) << "total recording time: " << time_since_start;
}

int32_t AudioDeviceBuffer::SetRecordingSampleRate(uint32_t fsHz) {
  RTC_LOG(LS_INFO) << "SetRecordingSampleRate(" << fsHz << ")";
  rec_sample_rate_ = fsHz;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutSampleRate(uint32_t fsHz) {
  RTC_LOG(LS_INFO) << "SetPlayoutSampleRate(" << fsHz << ")";
  play_sample_rate_ = fsHz;
  return 0;
}

uint32_t AudioDeviceBuffer::RecordingSampleRate() const {
  return rec_sample_rate_;
}

uint32_t AudioDeviceBuffer::PlayoutSampleRate() const {
  return play_sample_rate_;
}

int32_t AudioDeviceBuffer::SetRecordingChannels(size_t channels) {
  RTC_LOG(LS_INFO) << "SetRecordingChannels(" << channels << ")";
  rec_channels_ = channels;
  return 0;
}

int32_t AudioDeviceBuffer::SetPlayoutChannels(size_t channels) {
  RTC_LOG(LS_INFO) << "SetPlayoutChannels(" << channels << ")";
  play_channels_ = channels;
  return 0;
}

size_t AudioDeviceBuffer::RecordingChannels() const {
  return rec_channels_;
}

size_t AudioDeviceBuffer::PlayoutChannels() const {
  return play_channels_;
}

int32_t AudioDeviceBuffer::SetTypingStatus(bool typing_status) {
  typing_status_ = typing_status;
  return 0;
}

void AudioDeviceBuffer::SetVQEData(int play_delay_ms, int rec_delay_ms) {
  play_delay_ms_ = play_delay_ms;
  rec_delay_ms_ = rec_delay_ms;
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(const void* audio_buffer,
                                             size_t samples_per_channel) {
  return SetRecordedBuffer(audio_buffer, samples_per_channel, absl::nullopt);
}

int32_t AudioDeviceBuffer::SetRecordedBuffer(
    const void* audio_buffer,
    size_t samples_per_channel,
    absl::optional<int64_t> capture_timestamp_ns) {

  const size_t old_size = rec_buffer_.size();
  rec_buffer_.SetData(static_cast<const int16_t*>(audio_buffer),
                      rec_channels_ * samples_per_channel);

  if (old_size != rec_buffer_.size()) {
    RTC_LOG(LS_INFO) << "Size of recording buffer: " << rec_buffer_.size();
  }

  if (capture_timestamp_ns) {
    int64_t align_offsync_estimation_time = rtc::TimeMicros();
    if (align_offsync_estimation_time -
            rtc::TimestampAligner::kMinFrameIntervalUs >
        align_offsync_estimation_time_) {
      align_offsync_estimation_time_ = align_offsync_estimation_time;
      capture_timestamp_ns_ =
          rtc::kNumNanosecsPerMicrosec *
          timestamp_aligner_.TranslateTimestamp(
              *capture_timestamp_ns / rtc::kNumNanosecsPerMicrosec,
              align_offsync_estimation_time);
    } else {

      capture_timestamp_ns_ =
          rtc::kNumNanosecsPerMicrosec *
          timestamp_aligner_.TranslateTimestamp(*capture_timestamp_ns /
                                                rtc::kNumNanosecsPerMicrosec);
    }
  }

  int16_t max_abs = 0;
  RTC_DCHECK_LT(rec_stat_count_, 50);
  if (++rec_stat_count_ >= 50) {

    max_abs = WebRtcSpl_MaxAbsValueW16(rec_buffer_.data(), rec_buffer_.size());
    rec_stat_count_ = 0;

    if (max_abs > 0) {
      only_silence_recorded_ = false;
    }
  }

  UpdateRecStats(max_abs, samples_per_channel);
  return 0;
}

int32_t AudioDeviceBuffer::DeliverRecordedData() {
  if (!audio_transport_cb_) {
    RTC_LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }
  const size_t frames = rec_buffer_.size() / rec_channels_;
  const size_t bytes_per_frame = rec_channels_ * sizeof(int16_t);
  uint32_t new_mic_level_dummy = 0;
  uint32_t total_delay_ms = play_delay_ms_ + rec_delay_ms_;
  int32_t res = audio_transport_cb_->RecordedDataIsAvailable(
      rec_buffer_.data(), frames, bytes_per_frame, rec_channels_,
      rec_sample_rate_, total_delay_ms, 0, 0, typing_status_,
      new_mic_level_dummy, capture_timestamp_ns_);
  if (res == -1) {
    RTC_LOG(LS_ERROR) << "RecordedDataIsAvailable() failed";
  }
  return 0;
}

int32_t AudioDeviceBuffer::RequestPlayoutData(size_t samples_per_channel) {
  TRACE_EVENT1("webrtc", "AudioDeviceBuffer::RequestPlayoutData",
               "samples_per_channel", samples_per_channel);

  const size_t total_samples = play_channels_ * samples_per_channel;
  if (play_buffer_.size() != total_samples) {
    play_buffer_.SetSize(total_samples);
    RTC_LOG(LS_INFO) << "Size of playout buffer: " << play_buffer_.size();
  }

  size_t num_samples_out(0);

  if (!audio_transport_cb_) {
    RTC_LOG(LS_WARNING) << "Invalid audio transport";
    return 0;
  }

  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;
  const size_t bytes_per_frame = play_channels_ * sizeof(int16_t);
  uint32_t res = audio_transport_cb_->NeedMorePlayData(
      samples_per_channel, bytes_per_frame, play_channels_, play_sample_rate_,
      play_buffer_.data(), num_samples_out, &elapsed_time_ms, &ntp_time_ms);
  if (res != 0) {
    RTC_LOG(LS_ERROR) << "NeedMorePlayData() failed";
  }

  int16_t max_abs = 0;
  RTC_DCHECK_LT(play_stat_count_, 50);
  if (++play_stat_count_ >= 50) {

    max_abs =
        WebRtcSpl_MaxAbsValueW16(play_buffer_.data(), play_buffer_.size());
    play_stat_count_ = 0;
  }

  UpdatePlayStats(max_abs, num_samples_out / play_channels_);
  return static_cast<int32_t>(num_samples_out / play_channels_);
}

int32_t AudioDeviceBuffer::GetPlayoutData(void* audio_buffer) {
  RTC_DCHECK_GT(play_buffer_.size(), 0);
#ifdef AUDIO_DEVICE_PLAYS_SINUS_TONE
  const double phase_increment =
      k2Pi * 440.0 / static_cast<double>(play_sample_rate_);
  int16_t* destination_r = reinterpret_cast<int16_t*>(audio_buffer);
  if (play_channels_ == 1) {
    for (size_t i = 0; i < play_buffer_.size(); ++i) {
      destination_r[i] = static_cast<int16_t>((sin(phase_) * (1 << 14)));
      phase_ += phase_increment;
    }
  } else if (play_channels_ == 2) {
    for (size_t i = 0; i < play_buffer_.size() / 2; ++i) {
      destination_r[2 * i] = destination_r[2 * i + 1] =
          static_cast<int16_t>((sin(phase_) * (1 << 14)));
      phase_ += phase_increment;
    }
  }
#else
  memcpy(audio_buffer, play_buffer_.data(),
         play_buffer_.size() * sizeof(int16_t));
#endif

  return static_cast<int32_t>(play_buffer_.size() / play_channels_);
}

void AudioDeviceBuffer::StartPeriodicLogging() {
  task_queue_.PostTask([this] { LogStats(AudioDeviceBuffer::LOG_START); });
}

void AudioDeviceBuffer::StopPeriodicLogging() {
  task_queue_.PostTask([this] { LogStats(AudioDeviceBuffer::LOG_STOP); });
}

void AudioDeviceBuffer::LogStats(LogState state) {
  RTC_DCHECK_RUN_ON(&task_queue_);
  int64_t now_time = rtc::TimeMillis();

  if (state == AudioDeviceBuffer::LOG_START) {

    num_stat_reports_ = 0;
    last_timer_task_time_ = now_time;
    log_stats_ = true;
  } else if (state == AudioDeviceBuffer::LOG_STOP) {

    log_stats_ = false;
  } else if (state == AudioDeviceBuffer::LOG_ACTIVE) {

  }

  if (!log_stats_) {
    return;
  }

  int64_t next_callback_time = now_time + kTimerIntervalInMilliseconds;
  int64_t time_since_last = rtc::TimeDiff(now_time, last_timer_task_time_);
  last_timer_task_time_ = now_time;

  Stats stats;
  {
    MutexLock lock(&lock_);
    stats = stats_;
    stats_.max_rec_level = 0;
    stats_.max_play_level = 0;
  }

  const uint32_t rec_sample_rate = rec_sample_rate_;
  const uint32_t play_sample_rate = play_sample_rate_;

  if (++num_stat_reports_ > 2 &&
      static_cast<size_t>(time_since_last) > kTimerIntervalInMilliseconds / 2) {
    uint32_t diff_samples = stats.rec_samples - last_stats_.rec_samples;
    float rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    uint32_t abs_diff_rate_in_percent = 0;
    if (rec_sample_rate > 0 && rate > 0) {
      abs_diff_rate_in_percent = static_cast<uint32_t>(
          0.5f +
          ((100.0f * std::abs(rate - rec_sample_rate)) / rec_sample_rate));
      RTC_HISTOGRAM_PERCENTAGE("WebRTC.Audio.RecordSampleRateOffsetInPercent",
                               abs_diff_rate_in_percent);
      RTC_LOG(LS_INFO) << "[REC : " << time_since_last << "msec, "
                       << rec_sample_rate / 1000 << "kHz] callbacks: "
                       << stats.rec_callbacks - last_stats_.rec_callbacks
                       << ", "
                          "samples: "
                       << diff_samples
                       << ", "
                          "rate: "
                       << static_cast<int>(rate + 0.5)
                       << ", "
                          "rate diff: "
                       << abs_diff_rate_in_percent
                       << "%, "
                          "level: "
                       << stats.max_rec_level;
    }

    diff_samples = stats.play_samples - last_stats_.play_samples;
    rate = diff_samples / (static_cast<float>(time_since_last) / 1000.0);
    abs_diff_rate_in_percent = 0;
    if (play_sample_rate > 0 && rate > 0) {
      abs_diff_rate_in_percent = static_cast<uint32_t>(
          0.5f +
          ((100.0f * std::abs(rate - play_sample_rate)) / play_sample_rate));
      RTC_HISTOGRAM_PERCENTAGE("WebRTC.Audio.PlayoutSampleRateOffsetInPercent",
                               abs_diff_rate_in_percent);
      RTC_LOG(LS_INFO) << "[PLAY: " << time_since_last << "msec, "
                       << play_sample_rate / 1000 << "kHz] callbacks: "
                       << stats.play_callbacks - last_stats_.play_callbacks
                       << ", "
                          "samples: "
                       << diff_samples
                       << ", "
                          "rate: "
                       << static_cast<int>(rate + 0.5)
                       << ", "
                          "rate diff: "
                       << abs_diff_rate_in_percent
                       << "%, "
                          "level: "
                       << stats.max_play_level;
    }
  }
  last_stats_ = stats;

  int64_t time_to_wait_ms = next_callback_time - rtc::TimeMillis();
  RTC_DCHECK_GT(time_to_wait_ms, 0) << "Invalid timer interval";

  task_queue_.PostDelayedTask(
      [this] { AudioDeviceBuffer::LogStats(AudioDeviceBuffer::LOG_ACTIVE); },
      TimeDelta::Millis(time_to_wait_ms));
}

void AudioDeviceBuffer::ResetRecStats() {
  RTC_DCHECK_RUN_ON(&task_queue_);
  last_stats_.ResetRecStats();
  MutexLock lock(&lock_);
  stats_.ResetRecStats();
}

void AudioDeviceBuffer::ResetPlayStats() {
  RTC_DCHECK_RUN_ON(&task_queue_);
  last_stats_.ResetPlayStats();
  MutexLock lock(&lock_);
  stats_.ResetPlayStats();
}

void AudioDeviceBuffer::UpdateRecStats(int16_t max_abs,
                                       size_t samples_per_channel) {
  MutexLock lock(&lock_);
  ++stats_.rec_callbacks;
  stats_.rec_samples += samples_per_channel;
  if (max_abs > stats_.max_rec_level) {
    stats_.max_rec_level = max_abs;
  }
}

void AudioDeviceBuffer::UpdatePlayStats(int16_t max_abs,
                                        size_t samples_per_channel) {
  MutexLock lock(&lock_);
  ++stats_.play_callbacks;
  stats_.play_samples += samples_per_channel;
  if (max_abs > stats_.max_play_level) {
    stats_.max_play_level = max_abs;
  }
}

}
