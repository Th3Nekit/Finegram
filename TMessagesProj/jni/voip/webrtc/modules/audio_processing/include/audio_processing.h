/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_H_

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_control.h"
#include "api/ref_count.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class AecDump;
class AudioBuffer;

class StreamConfig;
class ProcessingConfig;

class EchoDetector;
class CustomAudioAnalyzer;
class CustomProcessing;

class RTC_EXPORT AudioProcessing : public RefCountInterface {
 public:

  struct RTC_EXPORT Config {

    struct RTC_EXPORT Pipeline {

      enum class DownmixMethod {
        kAverageChannels,
        kUseFirstChannel
      };

      int maximum_internal_processing_rate = 48000;

      bool multi_channel_render = false;

      bool multi_channel_capture = false;

      DownmixMethod capture_downmix_method = DownmixMethod::kAverageChannels;
    } pipeline;

    struct PreAmplifier {
      bool enabled = false;
      float fixed_gain_factor = 1.0f;
    } pre_amplifier;

    struct CaptureLevelAdjustment {
      bool operator==(const CaptureLevelAdjustment& rhs) const;
      bool operator!=(const CaptureLevelAdjustment& rhs) const {
        return !(*this == rhs);
      }
      bool enabled = false;

      float pre_gain_factor = 1.0f;

      float post_gain_factor = 1.0f;
      struct AnalogMicGainEmulation {
        bool operator==(const AnalogMicGainEmulation& rhs) const;
        bool operator!=(const AnalogMicGainEmulation& rhs) const {
          return !(*this == rhs);
        }
        bool enabled = false;

        int initial_level = 255;
      } analog_mic_gain_emulation;
    } capture_level_adjustment;

    struct HighPassFilter {
      bool enabled = false;
      bool apply_in_full_band = true;
    } high_pass_filter;

    struct EchoCanceller {
      bool enabled = false;
      bool mobile_mode = false;
      bool export_linear_aec_output = false;

      bool enforce_high_pass_filtering = true;
    } echo_canceller;

    struct NoiseSuppression {
      bool enabled = false;
      enum Level { kLow, kModerate, kHigh, kVeryHigh };
      Level level = kModerate;
      bool analyze_linear_aec_output_when_available = false;
    } noise_suppression;

    struct TransientSuppression {
      bool enabled = false;
    } transient_suppression;

    struct RTC_EXPORT GainController1 {
      bool operator==(const GainController1& rhs) const;
      bool operator!=(const GainController1& rhs) const {
        return !(*this == rhs);
      }

      bool enabled = false;
      enum Mode {

        kAdaptiveAnalog,

        kAdaptiveDigital,

        kFixedDigital
      };
      Mode mode = kAdaptiveAnalog;

      int target_level_dbfs = 3;

      int compression_gain_db = 9;

      bool enable_limiter = true;

      struct AnalogGainController {
        bool enabled = true;

        int startup_min_volume = 0;

        int clipped_level_min = 70;

        bool enable_digital_adaptive = true;

        int clipped_level_step = 15;

        float clipped_ratio_threshold = 0.1f;

        int clipped_wait_frames = 300;

        struct ClippingPredictor {
          bool enabled = false;
          enum Mode {

            kClippingEventPrediction,

            kAdaptiveStepClippingPeakPrediction,

            kFixedStepClippingPeakPrediction,
          };
          Mode mode = kClippingEventPrediction;

          int window_length = 5;

          int reference_window_length = 5;

          int reference_window_delay = 5;

          float clipping_threshold = -1.0f;

          float crest_factor_margin = 3.0f;

          bool use_predicted_step = true;
        } clipping_predictor;
      } analog_gain_controller;
    } gain_controller1;

    struct RTC_EXPORT GainController2 {
      bool operator==(const GainController2& rhs) const;
      bool operator!=(const GainController2& rhs) const {
        return !(*this == rhs);
      }

      bool enabled = false;

      struct InputVolumeController {
        bool operator==(const InputVolumeController& rhs) const;
        bool operator!=(const InputVolumeController& rhs) const {
          return !(*this == rhs);
        }
        bool enabled = false;
      } input_volume_controller;

      struct RTC_EXPORT AdaptiveDigital {
        bool operator==(const AdaptiveDigital& rhs) const;
        bool operator!=(const AdaptiveDigital& rhs) const {
          return !(*this == rhs);
        }
        bool enabled = false;
        float headroom_db = 6.0f;
        float max_gain_db = 30.0f;
        float initial_gain_db = 8.0f;
        float max_gain_change_db_per_second = 3.0f;
        float max_output_noise_level_dbfs = -50.0f;
      } adaptive_digital;

      struct FixedDigital {

        float gain_db = 0.0f;
      } fixed_digital;
    } gain_controller2;

    std::string ToString() const;
  };

  class RuntimeSetting {
   public:
    enum class Type {
      kNotSpecified,
      kCapturePreGain,
      kCaptureCompressionGain,
      kCaptureFixedPostGain,
      kPlayoutVolumeChange,
      kCustomRenderProcessingRuntimeSetting,
      kPlayoutAudioDeviceChange,
      kCapturePostGain,
      kCaptureOutputUsed
    };

    struct PlayoutAudioDeviceInfo {
      int id;
      int max_volume;
    };

    RuntimeSetting() : type_(Type::kNotSpecified), value_(0.0f) {}
    ~RuntimeSetting() = default;

    static RuntimeSetting CreateCapturePreGain(float gain) {
      return {Type::kCapturePreGain, gain};
    }

    static RuntimeSetting CreateCapturePostGain(float gain) {
      return {Type::kCapturePostGain, gain};
    }

    static RuntimeSetting CreateCompressionGainDb(int gain_db) {
      RTC_DCHECK_GE(gain_db, 0);
      RTC_DCHECK_LE(gain_db, 90);
      return {Type::kCaptureCompressionGain, static_cast<float>(gain_db)};
    }

    static RuntimeSetting CreateCaptureFixedPostGain(float gain_db) {
      RTC_DCHECK_GE(gain_db, 0.0f);
      RTC_DCHECK_LE(gain_db, 90.0f);
      return {Type::kCaptureFixedPostGain, gain_db};
    }

    static RuntimeSetting CreatePlayoutAudioDeviceChange(
        PlayoutAudioDeviceInfo audio_device) {
      return {Type::kPlayoutAudioDeviceChange, audio_device};
    }

    static RuntimeSetting CreatePlayoutVolumeChange(int volume) {
      return {Type::kPlayoutVolumeChange, volume};
    }

    static RuntimeSetting CreateCustomRenderSetting(float payload) {
      return {Type::kCustomRenderProcessingRuntimeSetting, payload};
    }

    static RuntimeSetting CreateCaptureOutputUsedSetting(
        bool capture_output_used) {
      return {Type::kCaptureOutputUsed, capture_output_used};
    }

    Type type() const { return type_; }

    void GetFloat(float* value) const {
      RTC_DCHECK(value);
      *value = value_.float_value;
    }
    void GetInt(int* value) const {
      RTC_DCHECK(value);
      *value = value_.int_value;
    }
    void GetBool(bool* value) const {
      RTC_DCHECK(value);
      *value = value_.bool_value;
    }
    void GetPlayoutAudioDeviceInfo(PlayoutAudioDeviceInfo* value) const {
      RTC_DCHECK(value);
      *value = value_.playout_audio_device_info;
    }

   private:
    RuntimeSetting(Type id, float value) : type_(id), value_(value) {}
    RuntimeSetting(Type id, int value) : type_(id), value_(value) {}
    RuntimeSetting(Type id, PlayoutAudioDeviceInfo value)
        : type_(id), value_(value) {}
    Type type_;
    union U {
      U() {}
      U(int value) : int_value(value) {}
      U(float value) : float_value(value) {}
      U(PlayoutAudioDeviceInfo value) : playout_audio_device_info(value) {}
      float float_value;
      int int_value;
      bool bool_value;
      PlayoutAudioDeviceInfo playout_audio_device_info;
    } value_;
  };

  ~AudioProcessing() override {}

  virtual int Initialize() = 0;

  virtual int Initialize(const ProcessingConfig& processing_config) = 0;

  virtual void ApplyConfig(const Config& config) = 0;

  virtual int proc_sample_rate_hz() const = 0;
  virtual int proc_split_sample_rate_hz() const = 0;
  virtual size_t num_input_channels() const = 0;
  virtual size_t num_proc_channels() const = 0;
  virtual size_t num_output_channels() const = 0;
  virtual size_t num_reverse_channels() const = 0;

  virtual void set_output_will_be_muted(bool muted) = 0;

  virtual void SetRuntimeSetting(RuntimeSetting setting) = 0;

  virtual bool PostRuntimeSetting(RuntimeSetting setting) = 0;

  virtual int ProcessStream(const int16_t* const src,
                            const StreamConfig& input_config,
                            const StreamConfig& output_config,
                            int16_t* const dest) = 0;

  virtual int ProcessStream(const float* const* src,
                            const StreamConfig& input_config,
                            const StreamConfig& output_config,
                            float* const* dest) = 0;

  virtual int ProcessReverseStream(const int16_t* const src,
                                   const StreamConfig& input_config,
                                   const StreamConfig& output_config,
                                   int16_t* const dest) = 0;

  virtual int ProcessReverseStream(const float* const* src,
                                   const StreamConfig& input_config,
                                   const StreamConfig& output_config,
                                   float* const* dest) = 0;

  virtual int AnalyzeReverseStream(const float* const* data,
                                   const StreamConfig& reverse_config) = 0;

  virtual bool GetLinearAecOutput(
      rtc::ArrayView<std::array<float, 160>> linear_output) const = 0;

  virtual void set_stream_analog_level(int level) = 0;

  virtual int recommended_stream_analog_level() const = 0;

  virtual int set_stream_delay_ms(int delay) = 0;
  virtual int stream_delay_ms() const = 0;

  virtual void set_stream_key_pressed(bool key_pressed) = 0;

  virtual bool CreateAndAttachAecDump(
      absl::string_view file_name,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue) = 0;
  virtual bool CreateAndAttachAecDump(
      absl::Nonnull<FILE*> handle,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue) = 0;

  virtual void AttachAecDump(std::unique_ptr<AecDump> aec_dump) = 0;

  virtual void DetachAecDump() = 0;

  virtual AudioProcessingStats GetStatistics() = 0;

  virtual AudioProcessingStats GetStatistics(bool has_remote_tracks) = 0;

  virtual AudioProcessing::Config GetConfig() const = 0;

  enum Error {

    kNoError = 0,
    kUnspecifiedError = -1,
    kCreationFailedError = -2,
    kUnsupportedComponentError = -3,
    kUnsupportedFunctionError = -4,
    kNullPointerError = -5,
    kBadParameterError = -6,
    kBadSampleRateError = -7,
    kBadDataLengthError = -8,
    kBadNumberChannelsError = -9,
    kFileError = -10,
    kStreamParameterNotSetError = -11,
    kNotEnabledError = -12,

    kBadStreamParameterWarning = -13
  };

  enum NativeRate {
    kSampleRate8kHz = 8000,
    kSampleRate16kHz = 16000,
    kSampleRate32kHz = 32000,
    kSampleRate48kHz = 48000
  };

  static constexpr int kNativeSampleRatesHz[4] = {
      kSampleRate8kHz, kSampleRate16kHz, kSampleRate32kHz, kSampleRate48kHz};
  static constexpr size_t kNumNativeSampleRates =
      arraysize(kNativeSampleRatesHz);
  static constexpr int kMaxNativeSampleRateHz =
      kNativeSampleRatesHz[kNumNativeSampleRates - 1];

  static constexpr int kChunkSizeMs = 10;

  static int GetFrameSize(int sample_rate_hz) { return sample_rate_hz / 100; }
};

class RTC_EXPORT AudioProcessingBuilder {
 public:
  AudioProcessingBuilder();
  AudioProcessingBuilder(const AudioProcessingBuilder&) = delete;
  AudioProcessingBuilder& operator=(const AudioProcessingBuilder&) = delete;
  ~AudioProcessingBuilder();

  AudioProcessingBuilder& SetConfig(const AudioProcessing::Config& config) {
    config_ = config;
    return *this;
  }

  AudioProcessingBuilder& SetEchoControlFactory(
      std::unique_ptr<EchoControlFactory> echo_control_factory) {
    echo_control_factory_ = std::move(echo_control_factory);
    return *this;
  }

  AudioProcessingBuilder& SetCapturePostProcessing(
      std::unique_ptr<CustomProcessing> capture_post_processing) {
    capture_post_processing_ = std::move(capture_post_processing);
    return *this;
  }

  AudioProcessingBuilder& SetRenderPreProcessing(
      std::unique_ptr<CustomProcessing> render_pre_processing) {
    render_pre_processing_ = std::move(render_pre_processing);
    return *this;
  }

  AudioProcessingBuilder& SetEchoDetector(
      rtc::scoped_refptr<EchoDetector> echo_detector) {
    echo_detector_ = std::move(echo_detector);
    return *this;
  }

  AudioProcessingBuilder& SetCaptureAnalyzer(
      std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
    capture_analyzer_ = std::move(capture_analyzer);
    return *this;
  }

  rtc::scoped_refptr<AudioProcessing> Create();

 private:
  AudioProcessing::Config config_;
  std::unique_ptr<EchoControlFactory> echo_control_factory_;
  std::unique_ptr<CustomProcessing> capture_post_processing_;
  std::unique_ptr<CustomProcessing> render_pre_processing_;
  rtc::scoped_refptr<EchoDetector> echo_detector_;
  std::unique_ptr<CustomAudioAnalyzer> capture_analyzer_;
};

class StreamConfig {
 public:

  StreamConfig(int sample_rate_hz = 0, size_t num_channels = 0)
      : sample_rate_hz_(sample_rate_hz),
        num_channels_(num_channels),
        num_frames_(calculate_frames(sample_rate_hz)) {}

  void set_sample_rate_hz(int value) {
    sample_rate_hz_ = value;
    num_frames_ = calculate_frames(value);
  }
  void set_num_channels(size_t value) { num_channels_ = value; }

  int sample_rate_hz() const { return sample_rate_hz_; }

  size_t num_channels() const { return num_channels_; }

  size_t num_frames() const { return num_frames_; }
  size_t num_samples() const { return num_channels_ * num_frames_; }

  bool operator==(const StreamConfig& other) const {
    return sample_rate_hz_ == other.sample_rate_hz_ &&
           num_channels_ == other.num_channels_;
  }

  bool operator!=(const StreamConfig& other) const { return !(*this == other); }

 private:
  static size_t calculate_frames(int sample_rate_hz) {
    return static_cast<size_t>(AudioProcessing::GetFrameSize(sample_rate_hz));
  }

  int sample_rate_hz_;
  size_t num_channels_;
  size_t num_frames_;
};

class ProcessingConfig {
 public:
  enum StreamName {
    kInputStream,
    kOutputStream,
    kReverseInputStream,
    kReverseOutputStream,
    kNumStreamNames,
  };

  const StreamConfig& input_stream() const {
    return streams[StreamName::kInputStream];
  }
  const StreamConfig& output_stream() const {
    return streams[StreamName::kOutputStream];
  }
  const StreamConfig& reverse_input_stream() const {
    return streams[StreamName::kReverseInputStream];
  }
  const StreamConfig& reverse_output_stream() const {
    return streams[StreamName::kReverseOutputStream];
  }

  StreamConfig& input_stream() { return streams[StreamName::kInputStream]; }
  StreamConfig& output_stream() { return streams[StreamName::kOutputStream]; }
  StreamConfig& reverse_input_stream() {
    return streams[StreamName::kReverseInputStream];
  }
  StreamConfig& reverse_output_stream() {
    return streams[StreamName::kReverseOutputStream];
  }

  bool operator==(const ProcessingConfig& other) const {
    for (int i = 0; i < StreamName::kNumStreamNames; ++i) {
      if (this->streams[i] != other.streams[i]) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const ProcessingConfig& other) const {
    return !(*this == other);
  }

  StreamConfig streams[StreamName::kNumStreamNames];
};

class CustomAudioAnalyzer {
 public:

  virtual void Initialize(int sample_rate_hz, int num_channels) = 0;

  virtual void Analyze(const AudioBuffer* audio) = 0;

  virtual std::string ToString() const = 0;

  virtual ~CustomAudioAnalyzer() {}
};

class CustomProcessing {
 public:

  virtual void Initialize(int sample_rate_hz, int num_channels) = 0;

  virtual void Process(AudioBuffer* audio) = 0;

  virtual std::string ToString() const = 0;

  virtual void SetRuntimeSetting(AudioProcessing::RuntimeSetting setting);

  virtual ~CustomProcessing() {}
};

class EchoDetector : public RefCountInterface {
 public:

  virtual void Initialize(int capture_sample_rate_hz,
                          int num_capture_channels,
                          int render_sample_rate_hz,
                          int num_render_channels) = 0;

  virtual void AnalyzeRenderAudio(rtc::ArrayView<const float> render_audio) = 0;

  virtual void AnalyzeCaptureAudio(
      rtc::ArrayView<const float> capture_audio) = 0;

  struct Metrics {
    absl::optional<double> echo_likelihood;
    absl::optional<double> echo_likelihood_recent_max;
  };

  virtual Metrics GetMetrics() const = 0;
};

}

#endif
