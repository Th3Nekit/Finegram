/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LOGGING_APM_DATA_DUMPER_H_
#define MODULES_AUDIO_PROCESSING_LOGGING_APM_DATA_DUMPER_H_

#include <stdint.h>
#include <stdio.h>

#if WEBRTC_APM_DEBUG_DUMP == 1
#include <memory>
#include <string>
#include <unordered_map>
#endif

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#if WEBRTC_APM_DEBUG_DUMP == 1
#include "common_audio/wav_file.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
#endif

#if !defined(WEBRTC_APM_DEBUG_DUMP) || \
    (WEBRTC_APM_DEBUG_DUMP != 0 && WEBRTC_APM_DEBUG_DUMP != 1)
#error "Set WEBRTC_APM_DEBUG_DUMP to either 0 or 1"
#endif

namespace webrtc {

#if WEBRTC_APM_DEBUG_DUMP == 1

struct RawFileCloseFunctor {
  void operator()(FILE* f) const { fclose(f); }
};
#endif

class ApmDataDumper {
 public:

  explicit ApmDataDumper(int instance_index);

  ApmDataDumper() = delete;
  ApmDataDumper(const ApmDataDumper&) = delete;
  ApmDataDumper& operator=(const ApmDataDumper&) = delete;

  ~ApmDataDumper();

  static void SetActivated(bool activated) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    recording_activated_ = activated;
#endif
  }

  static bool IsAvailable() {
#if WEBRTC_APM_DEBUG_DUMP == 1
    return true;
#else
    return false;
#endif
  }

  static constexpr size_t kDefaultDumpSet = 0;

  static void SetDumpSetToUse(int dump_set_to_use) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    dump_set_to_use_ = dump_set_to_use;
#endif
  }

  static void SetOutputDirectory(absl::string_view output_dir) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    RTC_CHECK_LT(output_dir.size(), kOutputDirMaxLength);
    rtc::strcpyn(output_dir_, kOutputDirMaxLength, output_dir);
#endif
  }

  void InitiateNewSetOfRecordings() {
#if WEBRTC_APM_DEBUG_DUMP == 1
    ++recording_set_index_;
#endif
  }

  void DumpRaw(absl::string_view name,
               double v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(&v, sizeof(v), 1, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const double* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(v, sizeof(v[0]), v_length, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const double> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, v.size(), v.data());
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               float v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(&v, sizeof(v), 1, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const float* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(v, sizeof(v[0]), v_length, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const float> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, v.size(), v.data());
    }
#endif
  }

  void DumpRaw(absl::string_view name, bool v, int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, static_cast<int16_t>(v));
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const bool* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      for (size_t k = 0; k < v_length; ++k) {
        int16_t value = static_cast<int16_t>(v[k]);
        fwrite(&value, sizeof(value), 1, file);
      }
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const bool> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, v.size(), v.data());
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               int16_t v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(&v, sizeof(v), 1, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const int16_t* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(v, sizeof(v[0]), v_length, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const int16_t> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, v.size(), v.data());
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               int32_t v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(&v, sizeof(v), 1, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const int32_t* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(v, sizeof(v[0]), v_length, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(&v, sizeof(v), 1, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               size_t v_length,
               const size_t* v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      FILE* file = GetRawFile(name);
      fwrite(v, sizeof(v[0]), v_length, file);
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const int32_t> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpRaw(name, v.size(), v.data());
    }
#endif
  }

  void DumpRaw(absl::string_view name,
               rtc::ArrayView<const size_t> v,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    DumpRaw(name, v.size(), v.data());
#endif
  }

  void DumpWav(absl::string_view name,
               size_t v_length,
               const float* v,
               int sample_rate_hz,
               int num_channels,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      WavWriter* file = GetWavFile(name, sample_rate_hz, num_channels,
                                   WavFile::SampleFormat::kFloat);
      file->WriteSamples(v, v_length);
    }
#endif
  }

  void DumpWav(absl::string_view name,
               rtc::ArrayView<const float> v,
               int sample_rate_hz,
               int num_channels,
               int dump_set = kDefaultDumpSet) {
#if WEBRTC_APM_DEBUG_DUMP == 1
    if (dump_set_to_use_ && *dump_set_to_use_ != dump_set)
      return;

    if (recording_activated_) {
      DumpWav(name, v.size(), v.data(), sample_rate_hz, num_channels);
    }
#endif
  }

 private:
#if WEBRTC_APM_DEBUG_DUMP == 1
  static bool recording_activated_;
  static absl::optional<int> dump_set_to_use_;
  static constexpr size_t kOutputDirMaxLength = 1024;
  static char output_dir_[kOutputDirMaxLength];
  const int instance_index_;
  int recording_set_index_ = 0;
  std::unordered_map<std::string, std::unique_ptr<FILE, RawFileCloseFunctor>>
      raw_files_;
  std::unordered_map<std::string, std::unique_ptr<WavWriter>> wav_files_;

  FILE* GetRawFile(absl::string_view name);
  WavWriter* GetWavFile(absl::string_view name,
                        int sample_rate_hz,
                        int num_channels,
                        WavFile::SampleFormat format);
#endif
};

}

#endif
