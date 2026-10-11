/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/transient/transient_suppressor_impl.h"

#include <string.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <deque>
#include <limits>
#include <set>
#include <string>

#include "common_audio/include/audio_util.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "common_audio/third_party/ooura/fft_size_256/fft4g.h"
#include "modules/audio_processing/transient/common.h"
#include "modules/audio_processing/transient/transient_detector.h"
#include "modules/audio_processing/transient/transient_suppressor.h"
#include "modules/audio_processing/transient/windows_private.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

static const float kMeanIIRCoefficient = 0.5f;

static const size_t kMinVoiceBin = 3;
static const size_t kMaxVoiceBin = 60;

namespace {

float ComplexMagnitude(float a, float b) {
  return std::abs(a) + std::abs(b);
}

std::string GetVadModeLabel(TransientSuppressor::VadMode vad_mode) {
  switch (vad_mode) {
    case TransientSuppressor::VadMode::kDefault:
      return "default";
    case TransientSuppressor::VadMode::kRnnVad:
      return "RNN VAD";
    case TransientSuppressor::VadMode::kNoVad:
      return "no VAD";
  }
}

}

TransientSuppressorImpl::TransientSuppressorImpl(VadMode vad_mode,
                                                 int sample_rate_hz,
                                                 int detector_rate_hz,
                                                 int num_channels)
    : vad_mode_(vad_mode),
      voice_probability_delay_unit_(                      0, sample_rate_hz),
      analyzed_audio_is_silent_(false),
      data_length_(0),
      detection_length_(0),
      analysis_length_(0),
      buffer_delay_(0),
      complex_analysis_length_(0),
      num_channels_(0),
      window_(NULL),
      detector_smoothed_(0.f),
      keypress_counter_(0),
      chunks_since_keypress_(0),
      detection_enabled_(false),
      suppression_enabled_(false),
      use_hard_restoration_(false),
      chunks_since_voice_change_(0),
      seed_(182),
      using_reference_(false) {
  RTC_LOG(LS_INFO) << "VAD mode: " << GetVadModeLabel(vad_mode_);
  Initialize(sample_rate_hz, detector_rate_hz, num_channels);
}

TransientSuppressorImpl::~TransientSuppressorImpl() {}

void TransientSuppressorImpl::Initialize(int sample_rate_hz,
                                         int detection_rate_hz,
                                         int num_channels) {
  RTC_DCHECK(sample_rate_hz == ts::kSampleRate8kHz ||
             sample_rate_hz == ts::kSampleRate16kHz ||
             sample_rate_hz == ts::kSampleRate32kHz ||
             sample_rate_hz == ts::kSampleRate48kHz);
  RTC_DCHECK(detection_rate_hz == ts::kSampleRate8kHz ||
             detection_rate_hz == ts::kSampleRate16kHz ||
             detection_rate_hz == ts::kSampleRate32kHz ||
             detection_rate_hz == ts::kSampleRate48kHz);
  RTC_DCHECK_GT(num_channels, 0);

  switch (sample_rate_hz) {
    case ts::kSampleRate8kHz:
      analysis_length_ = 128u;
      window_ = kBlocks80w128;
      break;
    case ts::kSampleRate16kHz:
      analysis_length_ = 256u;
      window_ = kBlocks160w256;
      break;
    case ts::kSampleRate32kHz:
      analysis_length_ = 512u;
      window_ = kBlocks320w512;
      break;
    case ts::kSampleRate48kHz:
      analysis_length_ = 1024u;
      window_ = kBlocks480w1024;
      break;
    default:
      RTC_DCHECK_NOTREACHED();
      return;
  }

  detector_.reset(new TransientDetector(detection_rate_hz));
  data_length_ = sample_rate_hz * ts::kChunkSizeMs / 1000;
  RTC_DCHECK_LE(data_length_, analysis_length_);
  buffer_delay_ = analysis_length_ - data_length_;

  voice_probability_delay_unit_.Initialize(                      buffer_delay_,
                                           sample_rate_hz);

  complex_analysis_length_ = analysis_length_ / 2 + 1;
  RTC_DCHECK_GE(complex_analysis_length_, kMaxVoiceBin);
  num_channels_ = num_channels;
  in_buffer_.reset(new float[analysis_length_ * num_channels_]);
  memset(in_buffer_.get(), 0,
         analysis_length_ * num_channels_ * sizeof(in_buffer_[0]));
  detection_length_ = detection_rate_hz * ts::kChunkSizeMs / 1000;
  detection_buffer_.reset(new float[detection_length_]);
  memset(detection_buffer_.get(), 0,
         detection_length_ * sizeof(detection_buffer_[0]));
  out_buffer_.reset(new float[analysis_length_ * num_channels_]);
  memset(out_buffer_.get(), 0,
         analysis_length_ * num_channels_ * sizeof(out_buffer_[0]));

  size_t ip_length = 2 + sqrtf(analysis_length_);
  ip_.reset(new size_t[ip_length]());
  memset(ip_.get(), 0, ip_length * sizeof(ip_[0]));
  wfft_.reset(new float[complex_analysis_length_ - 1]);
  memset(wfft_.get(), 0, (complex_analysis_length_ - 1) * sizeof(wfft_[0]));
  spectral_mean_.reset(new float[complex_analysis_length_ * num_channels_]);
  memset(spectral_mean_.get(), 0,
         complex_analysis_length_ * num_channels_ * sizeof(spectral_mean_[0]));
  fft_buffer_.reset(new float[analysis_length_ + 2]);
  memset(fft_buffer_.get(), 0, (analysis_length_ + 2) * sizeof(fft_buffer_[0]));
  magnitudes_.reset(new float[complex_analysis_length_]);
  memset(magnitudes_.get(), 0,
         complex_analysis_length_ * sizeof(magnitudes_[0]));
  mean_factor_.reset(new float[complex_analysis_length_]);

  static const float kFactorHeight = 10.f;
  static const float kLowSlope = 1.f;
  static const float kHighSlope = 0.3f;
  for (size_t i = 0; i < complex_analysis_length_; ++i) {
    mean_factor_[i] =
        kFactorHeight /
            (1.f + std::exp(kLowSlope * static_cast<int>(i - kMinVoiceBin))) +
        kFactorHeight /
            (1.f + std::exp(kHighSlope * static_cast<int>(kMaxVoiceBin - i)));
  }
  detector_smoothed_ = 0.f;
  keypress_counter_ = 0;
  chunks_since_keypress_ = 0;
  detection_enabled_ = false;
  suppression_enabled_ = false;
  use_hard_restoration_ = false;
  chunks_since_voice_change_ = 0;
  seed_ = 182;
  using_reference_ = false;
}

float TransientSuppressorImpl::Suppress(float* data,
                                        size_t data_length,
                                        int num_channels,
                                        const float* detection_data,
                                        size_t detection_length,
                                        const float* reference_data,
                                        size_t reference_length,
                                        float voice_probability,
                                        bool key_pressed) {
  if (!data || data_length != data_length_ || num_channels != num_channels_ ||
      detection_length != detection_length_ || voice_probability < 0 ||
      voice_probability > 1) {

    return voice_probability;
  }

  UpdateKeypress(key_pressed);
  UpdateBuffers(data);

  if (detection_enabled_) {
    UpdateRestoration(voice_probability);

    if (!detection_data) {

      detection_data = &in_buffer_[buffer_delay_];
    }

    float detector_result = detector_->Detect(detection_data, detection_length,
                                              reference_data, reference_length);
    if (detector_result < 0) {

      return voice_probability;
    }

    using_reference_ = detector_->using_reference();

    float smooth_factor = using_reference_ ? 0.6 : 0.1;
    detector_smoothed_ = detector_result >= detector_smoothed_
                             ? detector_result
                             : smooth_factor * detector_smoothed_ +
                                   (1 - smooth_factor) * detector_result;

    for (int i = 0; i < num_channels_; ++i) {
      Suppress(&in_buffer_[i * analysis_length_],
               &spectral_mean_[i * complex_analysis_length_],
               &out_buffer_[i * analysis_length_]);
    }
  }

  for (int i = 0; i < num_channels_; ++i) {
    memcpy(&data[i * data_length_],
           suppression_enabled_ ? &out_buffer_[i * analysis_length_]
                                : &in_buffer_[i * analysis_length_],
           data_length_ * sizeof(*data));
  }

  return voice_probability_delay_unit_.Delay(voice_probability);
}

void TransientSuppressorImpl::Suppress(float* in_ptr,
                                       float* spectral_mean,
                                       float* out_ptr) {

  for (size_t i = 0; i < analysis_length_; ++i) {

    fft_buffer_[i] = in_ptr[i] * window_[i];
  }

  WebRtc_rdft(analysis_length_, 1, fft_buffer_.get(), ip_.get(), wfft_.get());

  fft_buffer_[analysis_length_] = fft_buffer_[1];
  fft_buffer_[analysis_length_ + 1] = 0.f;
  fft_buffer_[1] = 0.f;

  for (size_t i = 0; i < complex_analysis_length_; ++i) {
    magnitudes_[i] =
        ComplexMagnitude(fft_buffer_[i * 2], fft_buffer_[i * 2 + 1]);
  }

  if (suppression_enabled_) {
    if (use_hard_restoration_) {
      HardRestoration(spectral_mean);
    } else {
      SoftRestoration(spectral_mean);
    }
  }

  for (size_t i = 0; i < complex_analysis_length_; ++i) {
    spectral_mean[i] = (1 - kMeanIIRCoefficient) * spectral_mean[i] +
                       kMeanIIRCoefficient * magnitudes_[i];
  }

  fft_buffer_[1] = fft_buffer_[analysis_length_];

  WebRtc_rdft(analysis_length_, -1, fft_buffer_.get(), ip_.get(), wfft_.get());
  const float fft_scaling = 2.f / analysis_length_;

  for (size_t i = 0; i < analysis_length_; ++i) {
    out_ptr[i] += fft_buffer_[i] * window_[i] * fft_scaling;
  }
}

void TransientSuppressorImpl::UpdateKeypress(bool key_pressed) {
  const int kKeypressPenalty = 1000 / ts::kChunkSizeMs;
  const int kIsTypingThreshold = 1000 / ts::kChunkSizeMs;
  const int kChunksUntilNotTyping = 4000 / ts::kChunkSizeMs;

  if (key_pressed) {
    keypress_counter_ += kKeypressPenalty;
    chunks_since_keypress_ = 0;
    detection_enabled_ = true;
  }
  keypress_counter_ = std::max(0, keypress_counter_ - 1);

  if (keypress_counter_ > kIsTypingThreshold) {
    if (!suppression_enabled_) {
      RTC_LOG(LS_INFO) << "[ts] Transient suppression is now enabled.";
    }
    suppression_enabled_ = true;
    keypress_counter_ = 0;
  }

  if (detection_enabled_ && ++chunks_since_keypress_ > kChunksUntilNotTyping) {
    if (suppression_enabled_) {
      RTC_LOG(LS_INFO) << "[ts] Transient suppression is now disabled.";
    }
    detection_enabled_ = false;
    suppression_enabled_ = false;
    keypress_counter_ = 0;
  }
}

void TransientSuppressorImpl::UpdateRestoration(float voice_probability) {
  bool not_voiced;
  switch (vad_mode_) {
    case TransientSuppressor::VadMode::kDefault: {
      constexpr float kVoiceThreshold = 0.02f;
      not_voiced = voice_probability < kVoiceThreshold;
      break;
    }
    case TransientSuppressor::VadMode::kRnnVad: {
      constexpr float kVoiceThreshold = 0.7f;
      not_voiced = voice_probability < kVoiceThreshold;
      break;
    }
    case TransientSuppressor::VadMode::kNoVad:

      not_voiced = false;
      break;
  }

  if (not_voiced == use_hard_restoration_) {
    chunks_since_voice_change_ = 0;
  } else {
    ++chunks_since_voice_change_;

    constexpr int kHardRestorationOffsetDelay = 3;
    constexpr int kHardRestorationOnsetDelay = 80;

    if ((use_hard_restoration_ &&
         chunks_since_voice_change_ > kHardRestorationOffsetDelay) ||
        (!use_hard_restoration_ &&
         chunks_since_voice_change_ > kHardRestorationOnsetDelay)) {
      use_hard_restoration_ = not_voiced;
      chunks_since_voice_change_ = 0;
    }
  }
}

void TransientSuppressorImpl::UpdateBuffers(float* data) {

  memmove(in_buffer_.get(), &in_buffer_[data_length_],
          (buffer_delay_ + (num_channels_ - 1) * analysis_length_) *
              sizeof(in_buffer_[0]));

  for (int i = 0; i < num_channels_; ++i) {
    memcpy(&in_buffer_[buffer_delay_ + i * analysis_length_],
           &data[i * data_length_], data_length_ * sizeof(*data));
  }
  if (detection_enabled_) {

    memmove(out_buffer_.get(), &out_buffer_[data_length_],
            (buffer_delay_ + (num_channels_ - 1) * analysis_length_) *
                sizeof(out_buffer_[0]));

    for (int i = 0; i < num_channels_; ++i) {
      memset(&out_buffer_[buffer_delay_ + i * analysis_length_], 0,
             data_length_ * sizeof(out_buffer_[0]));
    }
  }
}

void TransientSuppressorImpl::HardRestoration(float* spectral_mean) {
  const float detector_result =
      1.f - std::pow(1.f - detector_smoothed_, using_reference_ ? 200.f : 50.f);

  for (size_t i = 0; i < complex_analysis_length_; ++i) {
    if (magnitudes_[i] > spectral_mean[i] && magnitudes_[i] > 0) {

      const float phase = 2 * ts::kPi * WebRtcSpl_RandU(&seed_) /
                          std::numeric_limits<int16_t>::max();
      const float scaled_mean = detector_result * spectral_mean[i];

      fft_buffer_[i * 2] = (1 - detector_result) * fft_buffer_[i * 2] +
                           scaled_mean * cosf(phase);
      fft_buffer_[i * 2 + 1] = (1 - detector_result) * fft_buffer_[i * 2 + 1] +
                               scaled_mean * sinf(phase);
      magnitudes_[i] = magnitudes_[i] -
                       detector_result * (magnitudes_[i] - spectral_mean[i]);
    }
  }
}

void TransientSuppressorImpl::SoftRestoration(float* spectral_mean) {

  float block_frequency_mean = 0;
  for (size_t i = kMinVoiceBin; i < kMaxVoiceBin; ++i) {
    block_frequency_mean += magnitudes_[i];
  }
  block_frequency_mean /= (kMaxVoiceBin - kMinVoiceBin);

  for (size_t i = 0; i < complex_analysis_length_; ++i) {
    if (magnitudes_[i] > spectral_mean[i] && magnitudes_[i] > 0 &&
        (using_reference_ ||
         magnitudes_[i] < block_frequency_mean * mean_factor_[i])) {
      const float new_magnitude =
          magnitudes_[i] -
          detector_smoothed_ * (magnitudes_[i] - spectral_mean[i]);
      const float magnitude_ratio = new_magnitude / magnitudes_[i];

      fft_buffer_[i * 2] *= magnitude_ratio;
      fft_buffer_[i * 2 + 1] *= magnitude_ratio;
      magnitudes_[i] = new_magnitude;
    }
  }
}

}
