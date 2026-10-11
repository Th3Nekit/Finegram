/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/audio_device/aaudio_wrapper.h"

#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

#define LOG_ON_ERROR(op)                                                      \
  do {                                                                        \
    aaudio_result_t result = (op);                                            \
    if (result != AAUDIO_OK) {                                                \
      RTC_LOG(LS_ERROR) << #op << ": " << AAudio_convertResultToText(result); \
    }                                                                         \
  } while (0)

#define RETURN_ON_ERROR(op, ...)                                              \
  do {                                                                        \
    aaudio_result_t result = (op);                                            \
    if (result != AAUDIO_OK) {                                                \
      RTC_LOG(LS_ERROR) << #op << ": " << AAudio_convertResultToText(result); \
      return __VA_ARGS__;                                                     \
    }                                                                         \
  } while (0)

namespace webrtc {

namespace jni {

namespace {

const char* DirectionToString(aaudio_direction_t direction) {
  switch (direction) {
    case AAUDIO_DIRECTION_OUTPUT:
      return "OUTPUT";
    case AAUDIO_DIRECTION_INPUT:
      return "INPUT";
    default:
      return "UNKNOWN";
  }
}

const char* SharingModeToString(aaudio_sharing_mode_t mode) {
  switch (mode) {
    case AAUDIO_SHARING_MODE_EXCLUSIVE:
      return "EXCLUSIVE";
    case AAUDIO_SHARING_MODE_SHARED:
      return "SHARED";
    default:
      return "UNKNOWN";
  }
}

const char* PerformanceModeToString(aaudio_performance_mode_t mode) {
  switch (mode) {
    case AAUDIO_PERFORMANCE_MODE_NONE:
      return "NONE";
    case AAUDIO_PERFORMANCE_MODE_POWER_SAVING:
      return "POWER_SAVING";
    case AAUDIO_PERFORMANCE_MODE_LOW_LATENCY:
      return "LOW_LATENCY";
    default:
      return "UNKNOWN";
  }
}

const char* FormatToString(int32_t id) {
  switch (id) {
    case AAUDIO_FORMAT_INVALID:
      return "INVALID";
    case AAUDIO_FORMAT_UNSPECIFIED:
      return "UNSPECIFIED";
    case AAUDIO_FORMAT_PCM_I16:
      return "PCM_I16";
    case AAUDIO_FORMAT_PCM_FLOAT:
      return "FLOAT";
    default:
      return "UNKNOWN";
  }
}

void ErrorCallback(AAudioStream* stream,
                   void* user_data,
                   aaudio_result_t error) {
  RTC_DCHECK(user_data);
  AAudioWrapper* aaudio_wrapper = reinterpret_cast<AAudioWrapper*>(user_data);
  RTC_LOG(LS_WARNING) << "ErrorCallback: "
                      << DirectionToString(aaudio_wrapper->direction());
  RTC_DCHECK(aaudio_wrapper->observer());
  aaudio_wrapper->observer()->OnErrorCallback(error);
}

aaudio_data_callback_result_t DataCallback(AAudioStream* stream,
                                           void* user_data,
                                           void* audio_data,
                                           int32_t num_frames) {
  RTC_DCHECK(user_data);
  RTC_DCHECK(audio_data);
  AAudioWrapper* aaudio_wrapper = reinterpret_cast<AAudioWrapper*>(user_data);
  RTC_DCHECK(aaudio_wrapper->observer());
  return aaudio_wrapper->observer()->OnDataCallback(audio_data, num_frames);
}

class ScopedStreamBuilder {
 public:
  ScopedStreamBuilder() {
    LOG_ON_ERROR(AAudio_createStreamBuilder(&builder_));
    RTC_DCHECK(builder_);
  }
  ~ScopedStreamBuilder() {
    if (builder_) {
      LOG_ON_ERROR(AAudioStreamBuilder_delete(builder_));
    }
  }

  AAudioStreamBuilder* get() const { return builder_; }

 private:
  AAudioStreamBuilder* builder_ = nullptr;
};

}

AAudioWrapper::AAudioWrapper(const AudioParameters& audio_parameters,
                             aaudio_direction_t direction,
                             AAudioObserverInterface* observer)
    : audio_parameters_(audio_parameters),
      direction_(direction),
      observer_(observer) {
  RTC_LOG(LS_INFO) << "ctor";
  RTC_DCHECK(observer_);
  aaudio_thread_checker_.Detach();
  RTC_LOG(LS_INFO) << audio_parameters_.ToString();
}

AAudioWrapper::~AAudioWrapper() {
  RTC_LOG(LS_INFO) << "dtor";
  RTC_DCHECK(thread_checker_.IsCurrent());
  RTC_DCHECK(!stream_);
}

bool AAudioWrapper::Init() {
  RTC_LOG(LS_INFO) << "Init";
  RTC_DCHECK(thread_checker_.IsCurrent());

  ScopedStreamBuilder builder;

  SetStreamConfiguration(builder.get());

  if (!OpenStream(builder.get())) {
    return false;
  }

  if (!VerifyStreamConfiguration()) {
    return false;
  }

  if (!OptimizeBuffers()) {
    return false;
  }
  LogStreamState();
  return true;
}

bool AAudioWrapper::Start() {
  RTC_LOG(LS_INFO) << "Start";
  RTC_DCHECK(thread_checker_.IsCurrent());

  aaudio_stream_state_t current_state = AAudioStream_getState(stream_);
  if (current_state != AAUDIO_STREAM_STATE_OPEN) {
    RTC_LOG(LS_ERROR) << "Invalid state: "
                      << AAudio_convertStreamStateToText(current_state);
    return false;
  }

  RETURN_ON_ERROR(AAudioStream_requestStart(stream_), false);
  LogStreamState();
  return true;
}

bool AAudioWrapper::Stop() {
  RTC_LOG(LS_INFO) << "Stop: " << DirectionToString(direction());
  RTC_DCHECK(thread_checker_.IsCurrent());

  RETURN_ON_ERROR(AAudioStream_requestStop(stream_), false);
  CloseStream();
  aaudio_thread_checker_.Detach();
  return true;
}

double AAudioWrapper::EstimateLatencyMillis() const {
  RTC_DCHECK(stream_);
  double latency_millis = 0.0;
  if (direction() == AAUDIO_DIRECTION_INPUT) {

    latency_millis = static_cast<double>(frames_per_burst()) / sample_rate() *
                     rtc::kNumMillisecsPerSec;
  } else {
    int64_t existing_frame_index;
    int64_t existing_frame_presentation_time;

    aaudio_result_t result = AAudioStream_getTimestamp(
        stream_, CLOCK_MONOTONIC, &existing_frame_index,
        &existing_frame_presentation_time);

    if (result == AAUDIO_OK) {

      int64_t next_frame_index = frames_written();

      int64_t frame_index_delta = next_frame_index - existing_frame_index;

      int64_t next_frame_write_time = rtc::TimeNanos();

      int64_t frame_time_delta =
          (frame_index_delta * rtc::kNumNanosecsPerSec) / sample_rate();
      int64_t next_frame_presentation_time =
          existing_frame_presentation_time + frame_time_delta;

      latency_millis = static_cast<double>(next_frame_presentation_time -
                                           next_frame_write_time) /
                       rtc::kNumNanosecsPerMillisec;
    }
  }
  return latency_millis;
}

bool AAudioWrapper::IncreaseOutputBufferSize() {
  RTC_LOG(LS_INFO) << "IncreaseBufferSize";
  RTC_DCHECK(stream_);
  RTC_DCHECK(aaudio_thread_checker_.IsCurrent());
  RTC_DCHECK_EQ(direction(), AAUDIO_DIRECTION_OUTPUT);
  aaudio_result_t buffer_size = AAudioStream_getBufferSizeInFrames(stream_);

  buffer_size += frames_per_burst();

  const int32_t max_buffer_size = buffer_capacity_in_frames();
  if (buffer_size > max_buffer_size) {
    RTC_LOG(LS_ERROR) << "Required buffer size (" << buffer_size
                      << ") is higher than max: " << max_buffer_size;
    return false;
  }
  RTC_LOG(LS_INFO) << "Updating buffer size to: " << buffer_size
                   << " (max=" << max_buffer_size << ")";
  buffer_size = AAudioStream_setBufferSizeInFrames(stream_, buffer_size);
  if (buffer_size < 0) {
    RTC_LOG(LS_ERROR) << "Failed to change buffer size: "
                      << AAudio_convertResultToText(buffer_size);
    return false;
  }
  RTC_LOG(LS_INFO) << "Buffer size changed to: " << buffer_size;
  return true;
}

void AAudioWrapper::ClearInputStream(void* audio_data, int32_t num_frames) {
  RTC_LOG(LS_INFO) << "ClearInputStream";
  RTC_DCHECK(stream_);
  RTC_DCHECK(aaudio_thread_checker_.IsCurrent());
  RTC_DCHECK_EQ(direction(), AAUDIO_DIRECTION_INPUT);
  aaudio_result_t cleared_frames = 0;
  do {
    cleared_frames = AAudioStream_read(stream_, audio_data, num_frames, 0);
  } while (cleared_frames > 0);
}

AAudioObserverInterface* AAudioWrapper::observer() const {
  return observer_;
}

AudioParameters AAudioWrapper::audio_parameters() const {
  return audio_parameters_;
}

int32_t AAudioWrapper::samples_per_frame() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSamplesPerFrame(stream_);
}

int32_t AAudioWrapper::buffer_size_in_frames() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getBufferSizeInFrames(stream_);
}

int32_t AAudioWrapper::buffer_capacity_in_frames() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getBufferCapacityInFrames(stream_);
}

int32_t AAudioWrapper::device_id() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getDeviceId(stream_);
}

int32_t AAudioWrapper::xrun_count() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getXRunCount(stream_);
}

int32_t AAudioWrapper::format() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFormat(stream_);
}

int32_t AAudioWrapper::sample_rate() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSampleRate(stream_);
}

int32_t AAudioWrapper::channel_count() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getChannelCount(stream_);
}

int32_t AAudioWrapper::frames_per_callback() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFramesPerDataCallback(stream_);
}

aaudio_sharing_mode_t AAudioWrapper::sharing_mode() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getSharingMode(stream_);
}

aaudio_performance_mode_t AAudioWrapper::performance_mode() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getPerformanceMode(stream_);
}

aaudio_stream_state_t AAudioWrapper::stream_state() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getState(stream_);
}

int64_t AAudioWrapper::frames_written() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFramesWritten(stream_);
}

int64_t AAudioWrapper::frames_read() const {
  RTC_DCHECK(stream_);
  return AAudioStream_getFramesRead(stream_);
}

void AAudioWrapper::SetStreamConfiguration(AAudioStreamBuilder* builder) {
  RTC_LOG(LS_INFO) << "SetStreamConfiguration";
  RTC_DCHECK(builder);
  RTC_DCHECK(thread_checker_.IsCurrent());

  AAudioStreamBuilder_setDeviceId(builder, AAUDIO_UNSPECIFIED);

  AAudioStreamBuilder_setSampleRate(builder, audio_parameters().sample_rate());

  AAudioStreamBuilder_setChannelCount(builder, audio_parameters().channels());

  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);

  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);

  AAudioStreamBuilder_setDirection(builder, direction_);

  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

  AAudioStreamBuilder_setDataCallback(builder, DataCallback, this);

  AAudioStreamBuilder_setErrorCallback(builder, ErrorCallback, this);
}

bool AAudioWrapper::OpenStream(AAudioStreamBuilder* builder) {
  RTC_LOG(LS_INFO) << "OpenStream";
  RTC_DCHECK(builder);
  AAudioStream* stream = nullptr;
  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder, &stream), false);
  stream_ = stream;
  LogStreamConfiguration();
  return true;
}

void AAudioWrapper::CloseStream() {
  RTC_LOG(LS_INFO) << "CloseStream";
  RTC_DCHECK(stream_);
  LOG_ON_ERROR(AAudioStream_close(stream_));
  stream_ = nullptr;
}

void AAudioWrapper::LogStreamConfiguration() {
  RTC_DCHECK(stream_);
  char ss_buf[1024];
  rtc::SimpleStringBuilder ss(ss_buf);
  ss << "Stream Configuration: ";
  ss << "sample rate=" << sample_rate() << ", channels=" << channel_count();
  ss << ", samples per frame=" << samples_per_frame();
  ss << ", format=" << FormatToString(format());
  ss << ", sharing mode=" << SharingModeToString(sharing_mode());
  ss << ", performance mode=" << PerformanceModeToString(performance_mode());
  ss << ", direction=" << DirectionToString(direction());
  ss << ", device id=" << AAudioStream_getDeviceId(stream_);
  ss << ", frames per callback=" << frames_per_callback();
  RTC_LOG(LS_INFO) << ss.str();
}

void AAudioWrapper::LogStreamState() {
  RTC_LOG(LS_INFO) << "AAudio stream state: "
                   << AAudio_convertStreamStateToText(stream_state());
}

bool AAudioWrapper::VerifyStreamConfiguration() {
  RTC_LOG(LS_INFO) << "VerifyStreamConfiguration";
  RTC_DCHECK(stream_);

  if (AAudioStream_getSampleRate(stream_) != audio_parameters().sample_rate()) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested sample rate";
    return false;
  }
  if (AAudioStream_getChannelCount(stream_) !=
      static_cast<int32_t>(audio_parameters().channels())) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested channel count";
    return false;
  }
  if (AAudioStream_getFormat(stream_) != AAUDIO_FORMAT_PCM_I16) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested format";
    return false;
  }
  if (AAudioStream_getSharingMode(stream_) != AAUDIO_SHARING_MODE_SHARED) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested sharing mode";
    return false;
  }
  if (AAudioStream_getPerformanceMode(stream_) !=
      AAUDIO_PERFORMANCE_MODE_LOW_LATENCY) {
    RTC_LOG(LS_ERROR) << "Stream unable to use requested performance mode";
    return false;
  }
  if (AAudioStream_getDirection(stream_) != direction()) {
    RTC_LOG(LS_ERROR) << "Stream direction could not be set";
    return false;
  }
  if (AAudioStream_getSamplesPerFrame(stream_) !=
      static_cast<int32_t>(audio_parameters().channels())) {
    RTC_LOG(LS_ERROR) << "Invalid number of samples per frame";
    return false;
  }
  return true;
}

bool AAudioWrapper::OptimizeBuffers() {
  RTC_LOG(LS_INFO) << "OptimizeBuffers";
  RTC_DCHECK(stream_);

  RTC_LOG(LS_INFO) << "max buffer capacity in frames: "
                   << buffer_capacity_in_frames();

  int32_t frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  RTC_LOG(LS_INFO) << "frames per burst for optimal performance: "
                   << frames_per_burst;
  frames_per_burst_ = frames_per_burst;
  if (direction() == AAUDIO_DIRECTION_INPUT) {

    return true;
  }

  AAudioStream_setBufferSizeInFrames(stream_, frames_per_burst);
  int32_t buffer_size = AAudioStream_getBufferSizeInFrames(stream_);
  if (buffer_size != frames_per_burst) {
    RTC_LOG(LS_ERROR) << "Failed to use optimal buffer burst size";
    return false;
  }

  RTC_LOG(LS_INFO) << "buffer burst size in frames: " << buffer_size;
  return true;
}

}

}
