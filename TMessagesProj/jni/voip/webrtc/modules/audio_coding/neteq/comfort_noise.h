/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_COMFORT_NOISE_H_
#define MODULES_AUDIO_CODING_NETEQ_COMFORT_NOISE_H_

#include <stddef.h>

namespace webrtc {

class AudioMultiVector;
class DecoderDatabase;
class SyncBuffer;
struct Packet;

class ComfortNoise {
 public:
  enum ReturnCodes {
    kOK = 0,
    kUnknownPayloadType,
    kInternalError,
    kMultiChannelNotSupported
  };

  ComfortNoise(int fs_hz,
               DecoderDatabase* decoder_database,
               SyncBuffer* sync_buffer)
      : fs_hz_(fs_hz),
        first_call_(true),
        overlap_length_(5 * fs_hz_ / 8000),
        decoder_database_(decoder_database),
        sync_buffer_(sync_buffer) {}

  ComfortNoise(const ComfortNoise&) = delete;
  ComfortNoise& operator=(const ComfortNoise&) = delete;

  void Reset();

  int UpdateParameters(const Packet& packet);

  int Generate(size_t requested_length, AudioMultiVector* output);

  int internal_error_code() { return internal_error_code_; }

 private:
  int fs_hz_;
  bool first_call_;
  size_t overlap_length_;
  DecoderDatabase* decoder_database_;
  SyncBuffer* sync_buffer_;
  int internal_error_code_;
};

}
#endif
