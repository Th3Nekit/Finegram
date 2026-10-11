/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_WAV_FILE_H_
#define COMMON_AUDIO_WAV_FILE_H_

#include <stdint.h>

#include <cstddef>
#include <string>

#include "common_audio/wav_header.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {

class WavFile {
 public:
  enum class SampleFormat { kInt16, kFloat };

  virtual ~WavFile() {}

  virtual int sample_rate() const = 0;
  virtual size_t num_channels() const = 0;
  virtual size_t num_samples() const = 0;
};

class WavWriter final : public WavFile {
 public:

  WavWriter(absl::string_view filename,
            int sample_rate,
            size_t num_channels,
            SampleFormat sample_format = SampleFormat::kInt16);
  WavWriter(FileWrapper file,
            int sample_rate,
            size_t num_channels,
            SampleFormat sample_format = SampleFormat::kInt16);

  ~WavWriter() { Close(); }

  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;

  void WriteSamples(const float* samples, size_t num_samples);
  void WriteSamples(const int16_t* samples, size_t num_samples);

  int sample_rate() const override { return sample_rate_; }
  size_t num_channels() const override { return num_channels_; }
  size_t num_samples() const override { return num_samples_written_; }

 private:
  void Close();
  const int sample_rate_;
  const size_t num_channels_;
  size_t num_samples_written_;
  WavFormat format_;
  FileWrapper file_;
};

class WavReader final : public WavFile {
 public:

  explicit WavReader(absl::string_view filename);
  explicit WavReader(FileWrapper file);

  ~WavReader() { Close(); }

  WavReader(const WavReader&) = delete;
  WavReader& operator=(const WavReader&) = delete;

  void Reset();

  size_t ReadSamples(size_t num_samples, float* samples);
  size_t ReadSamples(size_t num_samples, int16_t* samples);

  int sample_rate() const override { return sample_rate_; }
  size_t num_channels() const override { return num_channels_; }
  size_t num_samples() const override { return num_samples_in_file_; }

 private:
  void Close();
  int sample_rate_;
  size_t num_channels_;
  WavFormat format_;
  size_t num_samples_in_file_;
  size_t num_unread_samples_;
  FileWrapper file_;
  int64_t
      data_start_pos_;
};

}

#endif
