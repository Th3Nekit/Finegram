/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/ns/noise_suppressor.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "modules/audio_processing/ns/fast_math.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

size_t NumBandsForRate(size_t sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == 16000 || sample_rate_hz == 32000 ||
             sample_rate_hz == 48000);
  return sample_rate_hz / 16000;
}

constexpr size_t kMaxNumChannelsOnStack = 2;

size_t NumChannelsOnHeap(size_t num_channels) {
  return num_channels > kMaxNumChannelsOnStack ? num_channels : 0;
}

constexpr std::array<float, 96> kBlocks160w256FirstHalf = {
    0.00000000f, 0.01636173f, 0.03271908f, 0.04906767f, 0.06540313f,
    0.08172107f, 0.09801714f, 0.11428696f, 0.13052619f, 0.14673047f,
    0.16289547f, 0.17901686f, 0.19509032f, 0.21111155f, 0.22707626f,
    0.24298018f, 0.25881905f, 0.27458862f, 0.29028468f, 0.30590302f,
    0.32143947f, 0.33688985f, 0.35225005f, 0.36751594f, 0.38268343f,
    0.39774847f, 0.41270703f, 0.42755509f, 0.44228869f, 0.45690388f,
    0.47139674f, 0.48576339f, 0.50000000f, 0.51410274f, 0.52806785f,
    0.54189158f, 0.55557023f, 0.56910015f, 0.58247770f, 0.59569930f,
    0.60876143f, 0.62166057f, 0.63439328f, 0.64695615f, 0.65934582f,
    0.67155895f, 0.68359230f, 0.69544264f, 0.70710678f, 0.71858162f,
    0.72986407f, 0.74095113f, 0.75183981f, 0.76252720f, 0.77301045f,
    0.78328675f, 0.79335334f, 0.80320753f, 0.81284668f, 0.82226822f,
    0.83146961f, 0.84044840f, 0.84920218f, 0.85772861f, 0.86602540f,
    0.87409034f, 0.88192126f, 0.88951608f, 0.89687274f, 0.90398929f,
    0.91086382f, 0.91749450f, 0.92387953f, 0.93001722f, 0.93590593f,
    0.94154407f, 0.94693013f, 0.95206268f, 0.95694034f, 0.96156180f,
    0.96592583f, 0.97003125f, 0.97387698f, 0.97746197f, 0.98078528f,
    0.98384601f, 0.98664333f, 0.98917651f, 0.99144486f, 0.99344778f,
    0.99518473f, 0.99665524f, 0.99785892f, 0.99879546f, 0.99946459f,
    0.99986614f};

void ApplyFilterBankWindow(rtc::ArrayView<float, kFftSize> x) {
  for (size_t i = 0; i < 96; ++i) {
    x[i] = kBlocks160w256FirstHalf[i] * x[i];
  }

  for (size_t i = 161, k = 95; i < kFftSize; ++i, --k) {
    RTC_DCHECK_NE(0, k);
    x[i] = kBlocks160w256FirstHalf[k] * x[i];
  }
}

void FormExtendedFrame(rtc::ArrayView<const float, kNsFrameSize> frame,
                       rtc::ArrayView<float, kFftSize - kNsFrameSize> old_data,
                       rtc::ArrayView<float, kFftSize> extended_frame) {
  std::copy(old_data.begin(), old_data.end(), extended_frame.begin());
  std::copy(frame.begin(), frame.end(),
            extended_frame.begin() + old_data.size());
  std::copy(extended_frame.end() - old_data.size(), extended_frame.end(),
            old_data.begin());
}

void OverlapAndAdd(rtc::ArrayView<const float, kFftSize> extended_frame,
                   rtc::ArrayView<float, kOverlapSize> overlap_memory,
                   rtc::ArrayView<float, kNsFrameSize> output_frame) {
  for (size_t i = 0; i < kOverlapSize; ++i) {
    output_frame[i] = overlap_memory[i] + extended_frame[i];
  }
  std::copy(extended_frame.begin() + kOverlapSize,
            extended_frame.begin() + kNsFrameSize,
            output_frame.begin() + kOverlapSize);
  std::copy(extended_frame.begin() + kNsFrameSize, extended_frame.end(),
            overlap_memory.begin());
}

void DelaySignal(rtc::ArrayView<const float, kNsFrameSize> frame,
                 rtc::ArrayView<float, kFftSize - kNsFrameSize> delay_buffer,
                 rtc::ArrayView<float, kNsFrameSize> delayed_frame) {
  constexpr size_t kSamplesFromFrame = kNsFrameSize - (kFftSize - kNsFrameSize);
  std::copy(delay_buffer.begin(), delay_buffer.end(), delayed_frame.begin());
  std::copy(frame.begin(), frame.begin() + kSamplesFromFrame,
            delayed_frame.begin() + delay_buffer.size());

  std::copy(frame.begin() + kSamplesFromFrame, frame.end(),
            delay_buffer.begin());
}

float ComputeEnergyOfExtendedFrame(rtc::ArrayView<const float, kFftSize> x) {
  float energy = 0.f;
  for (float x_k : x) {
    energy += x_k * x_k;
  }

  return energy;
}

float ComputeEnergyOfExtendedFrame(
    rtc::ArrayView<const float, kNsFrameSize> frame,
    rtc::ArrayView<float, kFftSize - kNsFrameSize> old_data) {
  float energy = 0.f;
  for (float v : old_data) {
    energy += v * v;
  }
  for (float v : frame) {
    energy += v * v;
  }

  return energy;
}

void ComputeMagnitudeSpectrum(
    rtc::ArrayView<const float, kFftSize> real,
    rtc::ArrayView<const float, kFftSize> imag,
    rtc::ArrayView<float, kFftSizeBy2Plus1> signal_spectrum) {
  signal_spectrum[0] = fabsf(real[0]) + 1.f;
  signal_spectrum[kFftSizeBy2Plus1 - 1] =
      fabsf(real[kFftSizeBy2Plus1 - 1]) + 1.f;

  for (size_t i = 1; i < kFftSizeBy2Plus1 - 1; ++i) {
    signal_spectrum[i] =
        SqrtFastApproximation(real[i] * real[i] + imag[i] * imag[i]) + 1.f;
  }
}

void ComputeSnr(rtc::ArrayView<const float, kFftSizeBy2Plus1> filter,
                rtc::ArrayView<const float> prev_signal_spectrum,
                rtc::ArrayView<const float> signal_spectrum,
                rtc::ArrayView<const float> prev_noise_spectrum,
                rtc::ArrayView<const float> noise_spectrum,
                rtc::ArrayView<float> prior_snr,
                rtc::ArrayView<float> post_snr) {
  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {

    float prev_estimate = prev_signal_spectrum[i] /
                          (prev_noise_spectrum[i] + 0.0001f) * filter[i];

    if (signal_spectrum[i] > noise_spectrum[i]) {
      post_snr[i] = signal_spectrum[i] / (noise_spectrum[i] + 0.0001f) - 1.f;
    } else {
      post_snr[i] = 0.f;
    }

    prior_snr[i] = 0.98f * prev_estimate + (1.f - 0.98f) * post_snr[i];
  }
}

float ComputeUpperBandsGain(
    float minimum_attenuating_gain,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> filter,
    rtc::ArrayView<const float> speech_probability,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> prev_analysis_signal_spectrum,
    rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum) {

  constexpr int kNumAvgBins = 32;
  constexpr float kOneByNumAvgBins = 1.f / kNumAvgBins;

  float avg_prob_speech = 0.f;
  float avg_filter_gain = 0.f;
  for (size_t i = kFftSizeBy2Plus1 - kNumAvgBins - 1; i < kFftSizeBy2Plus1 - 1;
       i++) {
    avg_prob_speech += speech_probability[i];
    avg_filter_gain += filter[i];
  }
  avg_prob_speech = avg_prob_speech * kOneByNumAvgBins;
  avg_filter_gain = avg_filter_gain * kOneByNumAvgBins;

  float sum_analysis_spectrum = 0.f;
  float sum_processing_spectrum = 0.f;
  for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
    sum_analysis_spectrum += prev_analysis_signal_spectrum[i];
    sum_processing_spectrum += signal_spectrum[i];
  }

  RTC_DCHECK_GT(sum_analysis_spectrum, 0.f);
  avg_prob_speech *= sum_processing_spectrum / sum_analysis_spectrum;

  float gain =
      0.5f * (1.f + static_cast<float>(tanh(2.f * avg_prob_speech - 1.f)));

  if (avg_prob_speech >= 0.5f) {
    gain = 0.25f * gain + 0.75f * avg_filter_gain;
  } else {
    gain = 0.5f * gain + 0.5f * avg_filter_gain;
  }

  return std::min(std::max(gain, minimum_attenuating_gain), 1.f);
}

}

NoiseSuppressor::ChannelState::ChannelState(
    const SuppressionParams& suppression_params,
    size_t num_bands)
    : wiener_filter(suppression_params),
      noise_estimator(suppression_params),
      process_delay_memory(num_bands > 1 ? num_bands - 1 : 0) {
  analyze_analysis_memory.fill(0.f);
  prev_analysis_signal_spectrum.fill(1.f);
  process_analysis_memory.fill(0.f);
  process_synthesis_memory.fill(0.f);
  for (auto& d : process_delay_memory) {
    d.fill(0.f);
  }
}

NoiseSuppressor::NoiseSuppressor(const NsConfig& config,
                                 size_t sample_rate_hz,
                                 size_t num_channels)
    : num_bands_(NumBandsForRate(sample_rate_hz)),
      num_channels_(num_channels),
      suppression_params_(config.target_level),
      filter_bank_states_heap_(NumChannelsOnHeap(num_channels_)),
      upper_band_gains_heap_(NumChannelsOnHeap(num_channels_)),
      energies_before_filtering_heap_(NumChannelsOnHeap(num_channels_)),
      gain_adjustments_heap_(NumChannelsOnHeap(num_channels_)),
      channels_(num_channels_) {
  for (size_t ch = 0; ch < num_channels_; ++ch) {
    channels_[ch] =
        std::make_unique<ChannelState>(suppression_params_, num_bands_);
  }
}

void NoiseSuppressor::AggregateWienerFilters(
    rtc::ArrayView<float, kFftSizeBy2Plus1> filter) const {
  rtc::ArrayView<const float, kFftSizeBy2Plus1> filter0 =
      channels_[0]->wiener_filter.get_filter();
  std::copy(filter0.begin(), filter0.end(), filter.begin());

  for (size_t ch = 1; ch < num_channels_; ++ch) {
    rtc::ArrayView<const float, kFftSizeBy2Plus1> filter_ch =
        channels_[ch]->wiener_filter.get_filter();

    for (size_t k = 0; k < kFftSizeBy2Plus1; ++k) {
      filter[k] = std::min(filter[k], filter_ch[k]);
    }
  }
}

void NoiseSuppressor::Analyze(const AudioBuffer& audio) {

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    channels_[ch]->noise_estimator.PrepareAnalysis();
  }

  bool zero_frame = true;
  for (size_t ch = 0; ch < num_channels_; ++ch) {
    rtc::ArrayView<const float, kNsFrameSize> y_band0(
        &audio.split_bands_const(ch)[0][0], kNsFrameSize);
    float energy = ComputeEnergyOfExtendedFrame(
        y_band0, channels_[ch]->analyze_analysis_memory);
    if (energy > 0.f) {
      zero_frame = false;
      break;
    }
  }

  if (zero_frame) {

    return;
  }

  if (++num_analyzed_frames_ < 0) {
    num_analyzed_frames_ = 0;
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    std::unique_ptr<ChannelState>& ch_p = channels_[ch];
    rtc::ArrayView<const float, kNsFrameSize> y_band0(
        &audio.split_bands_const(ch)[0][0], kNsFrameSize);

    std::array<float, kFftSize> extended_frame;
    FormExtendedFrame(y_band0, ch_p->analyze_analysis_memory, extended_frame);
    ApplyFilterBankWindow(extended_frame);

    std::array<float, kFftSize> real;
    std::array<float, kFftSize> imag;
    fft_.Fft(extended_frame, real, imag);

    std::array<float, kFftSizeBy2Plus1> signal_spectrum;
    ComputeMagnitudeSpectrum(real, imag, signal_spectrum);

    float signal_energy = 0.f;
    for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
      signal_energy += real[i] * real[i] + imag[i] * imag[i];
    }
    signal_energy /= kFftSizeBy2Plus1;

    float signal_spectral_sum = 0.f;
    for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
      signal_spectral_sum += signal_spectrum[i];
    }

    ch_p->noise_estimator.PreUpdate(num_analyzed_frames_, signal_spectrum,
                                    signal_spectral_sum);

    std::array<float, kFftSizeBy2Plus1> post_snr;
    std::array<float, kFftSizeBy2Plus1> prior_snr;
    ComputeSnr(ch_p->wiener_filter.get_filter(),
               ch_p->prev_analysis_signal_spectrum, signal_spectrum,
               ch_p->noise_estimator.get_prev_noise_spectrum(),
               ch_p->noise_estimator.get_noise_spectrum(), prior_snr, post_snr);

    ch_p->speech_probability_estimator.Update(
        num_analyzed_frames_, prior_snr, post_snr,
        ch_p->noise_estimator.get_conservative_noise_spectrum(),
        signal_spectrum, signal_spectral_sum, signal_energy);

    ch_p->noise_estimator.PostUpdate(
        ch_p->speech_probability_estimator.get_probability(), signal_spectrum);

    std::copy(signal_spectrum.begin(), signal_spectrum.end(),
              ch_p->prev_analysis_signal_spectrum.begin());
  }
}

void NoiseSuppressor::Process(AudioBuffer* audio) {

  std::array<FilterBankState, kMaxNumChannelsOnStack> filter_bank_states_stack;
  rtc::ArrayView<FilterBankState> filter_bank_states(
      filter_bank_states_stack.data(), num_channels_);
  std::array<float, kMaxNumChannelsOnStack> upper_band_gains_stack;
  rtc::ArrayView<float> upper_band_gains(upper_band_gains_stack.data(),
                                         num_channels_);
  std::array<float, kMaxNumChannelsOnStack> energies_before_filtering_stack;
  rtc::ArrayView<float> energies_before_filtering(
      energies_before_filtering_stack.data(), num_channels_);
  std::array<float, kMaxNumChannelsOnStack> gain_adjustments_stack;
  rtc::ArrayView<float> gain_adjustments(gain_adjustments_stack.data(),
                                         num_channels_);
  if (NumChannelsOnHeap(num_channels_) > 0) {

    filter_bank_states = rtc::ArrayView<FilterBankState>(
        filter_bank_states_heap_.data(), num_channels_);
    upper_band_gains =
        rtc::ArrayView<float>(upper_band_gains_heap_.data(), num_channels_);
    energies_before_filtering = rtc::ArrayView<float>(
        energies_before_filtering_heap_.data(), num_channels_);
    gain_adjustments =
        rtc::ArrayView<float>(gain_adjustments_heap_.data(), num_channels_);
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {

    rtc::ArrayView<float, kNsFrameSize> y_band0(&audio->split_bands(ch)[0][0],
                                                kNsFrameSize);

    FormExtendedFrame(y_band0, channels_[ch]->process_analysis_memory,
                      filter_bank_states[ch].extended_frame);

    ApplyFilterBankWindow(filter_bank_states[ch].extended_frame);

    energies_before_filtering[ch] =
        ComputeEnergyOfExtendedFrame(filter_bank_states[ch].extended_frame);

    fft_.Fft(filter_bank_states[ch].extended_frame, filter_bank_states[ch].real,
             filter_bank_states[ch].imag);

    std::array<float, kFftSizeBy2Plus1> signal_spectrum;
    ComputeMagnitudeSpectrum(filter_bank_states[ch].real,
                             filter_bank_states[ch].imag, signal_spectrum);

    channels_[ch]->wiener_filter.Update(
        num_analyzed_frames_,
        channels_[ch]->noise_estimator.get_noise_spectrum(),
        channels_[ch]->noise_estimator.get_prev_noise_spectrum(),
        channels_[ch]->noise_estimator.get_parametric_noise_spectrum(),
        signal_spectrum);

    if (num_bands_ > 1) {

      upper_band_gains[ch] = ComputeUpperBandsGain(
          suppression_params_.minimum_attenuating_gain,
          channels_[ch]->wiener_filter.get_filter(),
          channels_[ch]->speech_probability_estimator.get_probability(),
          channels_[ch]->prev_analysis_signal_spectrum, signal_spectrum);
    }
  }

  if (!capture_output_used_) {
    return;
  }

  std::array<float, kFftSizeBy2Plus1> filter_data;
  rtc::ArrayView<const float, kFftSizeBy2Plus1> filter = filter_data;
  if (num_channels_ == 1) {
    filter = channels_[0]->wiener_filter.get_filter();
  } else {
    AggregateWienerFilters(filter_data);
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {

    for (size_t i = 0; i < kFftSizeBy2Plus1; ++i) {
      filter_bank_states[ch].real[i] *= filter[i];
      filter_bank_states[ch].imag[i] *= filter[i];
    }
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    fft_.Ifft(filter_bank_states[ch].real, filter_bank_states[ch].imag,
              filter_bank_states[ch].extended_frame);
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    const float energy_after_filtering =
        ComputeEnergyOfExtendedFrame(filter_bank_states[ch].extended_frame);

    ApplyFilterBankWindow(filter_bank_states[ch].extended_frame);

    gain_adjustments[ch] =
        channels_[ch]->wiener_filter.ComputeOverallScalingFactor(
            num_analyzed_frames_,
            channels_[ch]->speech_probability_estimator.get_prior_probability(),
            energies_before_filtering[ch], energy_after_filtering);
  }

  float gain_adjustment = gain_adjustments[0];
  for (size_t ch = 1; ch < num_channels_; ++ch) {
    gain_adjustment = std::min(gain_adjustment, gain_adjustments[ch]);
  }
  for (size_t ch = 0; ch < num_channels_; ++ch) {
    for (size_t i = 0; i < kFftSize; ++i) {
      filter_bank_states[ch].extended_frame[i] =
          gain_adjustment * filter_bank_states[ch].extended_frame[i];
    }
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    rtc::ArrayView<float, kNsFrameSize> y_band0(&audio->split_bands(ch)[0][0],
                                                kNsFrameSize);
    OverlapAndAdd(filter_bank_states[ch].extended_frame,
                  channels_[ch]->process_synthesis_memory, y_band0);
  }

  if (num_bands_ > 1) {

    float upper_band_gain = upper_band_gains[0];
    for (size_t ch = 1; ch < num_channels_; ++ch) {
      upper_band_gain = std::min(upper_band_gain, upper_band_gains[ch]);
    }

    for (size_t ch = 0; ch < num_channels_; ++ch) {
      for (size_t b = 1; b < num_bands_; ++b) {

        rtc::ArrayView<float, kNsFrameSize> y_band(
            &audio->split_bands(ch)[b][0], kNsFrameSize);
        std::array<float, kNsFrameSize> delayed_frame;
        DelaySignal(y_band, channels_[ch]->process_delay_memory[b - 1],
                    delayed_frame);

        for (size_t j = 0; j < kNsFrameSize; j++) {
          y_band[j] = upper_band_gain * delayed_frame[j];
        }
      }
    }
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    for (size_t b = 0; b < num_bands_; ++b) {
      rtc::ArrayView<float, kNsFrameSize> y_band(&audio->split_bands(ch)[b][0],
                                                 kNsFrameSize);
      for (size_t j = 0; j < kNsFrameSize; j++) {
        y_band[j] = std::min(std::max(y_band[j], -32768.f), 32767.f);
      }
    }
  }
}

}
