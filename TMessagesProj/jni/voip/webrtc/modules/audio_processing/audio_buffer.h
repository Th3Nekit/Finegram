/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "common_audio/channel_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class PushSincResampler;
class SplittingFilter;

enum Band { kBand0To8kHz = 0, kBand8To16kHz = 1, kBand16To24kHz = 2 };

class AudioBuffer {
 public:
  static const int kSplitBandSize = 160;
  static const int kMaxSampleRate = 384000;
  AudioBuffer(size_t input_rate,
              size_t input_num_channels,
              size_t buffer_rate,
              size_t buffer_num_channels,
              size_t output_rate,
              size_t output_num_channels);

  virtual ~AudioBuffer();

  AudioBuffer(const AudioBuffer&) = delete;
  AudioBuffer& operator=(const AudioBuffer&) = delete;

  void set_downmixing_to_specific_channel(size_t channel);

  void set_downmixing_by_averaging();

  void set_num_channels(size_t num_channels);

  size_t num_channels() const { return num_channels_; }
  size_t num_frames() const { return buffer_num_frames_; }
  size_t num_frames_per_band() const { return num_split_frames_; }
  size_t num_bands() const { return num_bands_; }

  float* const* channels() { return data_->channels(); }
  const float* const* channels_const() const { return data_->channels(); }

  const float* const* split_bands_const(size_t channel) const {
    return split_data_.get() ? split_data_->bands(channel)
                             : data_->bands(channel);
  }
  float* const* split_bands(size_t channel) {
    return split_data_.get() ? split_data_->bands(channel)
                             : data_->bands(channel);
  }

  const float* const* split_channels_const(Band band) const {
    if (split_data_.get()) {
      return split_data_->channels(band);
    } else {
      return band == kBand0To8kHz ? data_->channels() : nullptr;
    }
  }

  void CopyFrom(const int16_t* const interleaved_data,
                const StreamConfig& stream_config);
  void CopyFrom(const float* const* stacked_data,
                const StreamConfig& stream_config);

  void CopyTo(const StreamConfig& stream_config,
              int16_t* const interleaved_data);
  void CopyTo(const StreamConfig& stream_config, float* const* stacked_data);
  void CopyTo(AudioBuffer* buffer) const;

  void SplitIntoFrequencyBands();

  void MergeFrequencyBands();

  void ExportSplitChannelData(size_t channel,
                              int16_t* const* split_band_data) const;

  void ImportSplitChannelData(size_t channel,
                              const int16_t* const* split_band_data);

  static const size_t kMaxSplitFrameLength = 160;
  static const size_t kMaxNumBands = 3;

  float* const* channels_f() { return channels(); }
  const float* const* channels_const_f() const { return channels_const(); }
  const float* const* split_bands_const_f(size_t channel) const {
    return split_bands_const(channel);
  }
  float* const* split_bands_f(size_t channel) { return split_bands(channel); }
  const float* const* split_channels_const_f(Band band) const {
    return split_channels_const(band);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioBufferTest,
                           SetNumChannelsSetsChannelBuffersNumChannels);
  void RestoreNumChannels();

  const size_t input_num_frames_;
  const size_t input_num_channels_;
  const size_t buffer_num_frames_;
  const size_t buffer_num_channels_;
  const size_t output_num_frames_;
  const size_t output_num_channels_;

  size_t num_channels_;
  size_t num_bands_;
  size_t num_split_frames_;

  std::unique_ptr<ChannelBuffer<float>> data_;
  std::unique_ptr<ChannelBuffer<float>> split_data_;
  std::unique_ptr<SplittingFilter> splitting_filter_;
  std::vector<std::unique_ptr<PushSincResampler>> input_resamplers_;
  std::vector<std::unique_ptr<PushSincResampler>> output_resamplers_;
  bool downmix_by_averaging_ = true;
  size_t channel_for_downmixing_ = 0;
};

}

#endif
