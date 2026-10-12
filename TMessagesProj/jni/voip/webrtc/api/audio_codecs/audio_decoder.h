/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_DECODER_H_
#define API_AUDIO_CODECS_AUDIO_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class AudioDecoder {
 public:
  enum SpeechType {
    kSpeech = 1,
    kComfortNoise = 2,
  };

  enum { kNotImplemented = -2 };

  AudioDecoder() = default;
  virtual ~AudioDecoder() = default;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

  class EncodedAudioFrame {
   public:
    struct DecodeResult {
      size_t num_decoded_samples;
      SpeechType speech_type;
    };

    virtual ~EncodedAudioFrame() = default;

    virtual size_t Duration() const = 0;

    virtual bool IsDtxPacket() const;

    virtual absl::optional<DecodeResult> Decode(
        rtc::ArrayView<int16_t> decoded) const = 0;
  };

  struct ParseResult {
    ParseResult();
    ParseResult(uint32_t timestamp,
                int priority,
                std::unique_ptr<EncodedAudioFrame> frame);
    ParseResult(ParseResult&& b);
    ~ParseResult();

    ParseResult& operator=(ParseResult&& b);

    uint32_t timestamp;

    int priority;
    std::unique_ptr<EncodedAudioFrame> frame;
  };

  virtual std::vector<ParseResult> ParsePayload(rtc::Buffer&& payload,
                                                uint32_t timestamp);

  int Decode(const uint8_t* encoded,
             size_t encoded_len,
             int sample_rate_hz,
             size_t max_decoded_bytes,
             int16_t* decoded,
             SpeechType* speech_type);

  int DecodeRedundant(const uint8_t* encoded,
                      size_t encoded_len,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded,
                      SpeechType* speech_type);

  virtual bool HasDecodePlc() const;

  virtual size_t DecodePlc(size_t num_frames, int16_t* decoded);

  virtual void GeneratePlc(size_t requested_samples_per_channel,
                           rtc::BufferT<int16_t>* concealment_audio);

  virtual void Reset() = 0;

  virtual int ErrorCode();

  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;

  virtual int PacketDurationRedundant(const uint8_t* encoded,
                                      size_t encoded_len) const;

  virtual bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const;

  virtual int SampleRateHz() const = 0;

  virtual size_t Channels() const = 0;

  static constexpr int kMaxNumberOfChannels = 24;

 protected:
  static SpeechType ConvertSpeechType(int16_t type);

  virtual int DecodeInternal(const uint8_t* encoded,
                             size_t encoded_len,
                             int sample_rate_hz,
                             int16_t* decoded,
                             SpeechType* speech_type) = 0;

  virtual int DecodeRedundantInternal(const uint8_t* encoded,
                                      size_t encoded_len,
                                      int sample_rate_hz,
                                      int16_t* decoded,
                                      SpeechType* speech_type);
};

}
#endif
