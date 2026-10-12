/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_FAKE_DECODE_FROM_FILE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_FAKE_DECODE_FROM_FILE_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_decoder.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"

namespace webrtc {
namespace test {

class FakeDecodeFromFile : public AudioDecoder {
 public:
  FakeDecodeFromFile(std::unique_ptr<InputAudioFile> input,
                     int sample_rate_hz,
                     bool stereo)
      : input_(std::move(input)),
        sample_rate_hz_(sample_rate_hz),
        stereo_(stereo) {}

  ~FakeDecodeFromFile() = default;

  std::vector<ParseResult> ParsePayload(rtc::Buffer&& payload,
                                        uint32_t timestamp) override;

  void Reset() override {}

  int SampleRateHz() const override { return sample_rate_hz_; }

  size_t Channels() const override { return stereo_ ? 2 : 1; }

  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

  void ReadFromFile(uint32_t timestamp, size_t samples, int16_t* destination);

  static void PrepareEncoded(uint32_t timestamp,
                             size_t samples,
                             size_t original_payload_size_bytes,
                             rtc::ArrayView<uint8_t> encoded);

 private:
  std::unique_ptr<InputAudioFile> input_;
  absl::optional<uint32_t> next_timestamp_from_input_;
  const int sample_rate_hz_;
  const bool stereo_;
};

}
}
#endif
