/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_INCLUDE_AUDIO_CODING_MODULE_TYPEDEFS_H_
#define MODULES_AUDIO_CODING_INCLUDE_AUDIO_CODING_MODULE_TYPEDEFS_H_

#include <map>

namespace webrtc {

enum ACMVADMode {
  VADNormal = 0,
  VADLowBitrate = 1,
  VADAggr = 2,
  VADVeryAggr = 3
};

enum class AudioFrameType {
  kEmptyFrame = 0,
  kAudioFrameSpeech = 1,
  kAudioFrameCN = 2,
};

enum OpusApplicationMode {
  kVoip = 0,
  kAudio = 1,
};

struct AudioDecodingCallStats {
  AudioDecodingCallStats()
      : calls_to_silence_generator(0),
        calls_to_neteq(0),
        decoded_normal(0),
        decoded_neteq_plc(0),
        decoded_codec_plc(0),
        decoded_cng(0),
        decoded_plc_cng(0),
        decoded_muted_output(0) {}

  int calls_to_silence_generator;

  int calls_to_neteq;
  int decoded_normal;
  int decoded_neteq_plc;
  int decoded_codec_plc;
  int decoded_cng;
  int decoded_plc_cng;
  int decoded_muted_output;
};

struct NetworkStatistics {

  uint16_t currentBufferSize;

  uint16_t preferredBufferSize;

  bool jitterPeaksFound;

  uint64_t totalSamplesReceived;
  uint64_t concealedSamples;
  uint64_t silentConcealedSamples;
  uint64_t concealmentEvents;
  uint64_t jitterBufferDelayMs;
  uint64_t jitterBufferTargetDelayMs;
  uint64_t jitterBufferMinimumDelayMs;
  uint64_t jitterBufferEmittedCount;
  uint64_t insertedSamplesForDeceleration;
  uint64_t removedSamplesForAcceleration;
  uint64_t fecPacketsReceived;
  uint64_t fecPacketsDiscarded;

  uint64_t packetsDiscarded;

  uint16_t currentExpandRate;

  uint16_t currentSpeechExpandRate;

  uint16_t currentPreemptiveRate;

  uint16_t currentAccelerateRate;

  uint16_t currentSecondaryDecodedRate;

  uint16_t currentSecondaryDiscardedRate;

  int meanWaitingTimeMs;

  int maxWaitingTimeMs;

  uint64_t packetBufferFlushes;

  uint64_t delayedPacketOutageSamples;

  uint64_t relativePacketArrivalDelayMs;

  int32_t interruptionCount;

  int32_t totalInterruptionDurationMs;
};

}

#endif
