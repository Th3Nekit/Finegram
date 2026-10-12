/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_performance_test.h"

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/neteq/neteq.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "modules/audio_coding/neteq/default_neteq_factory.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "modules/audio_coding/neteq/tools/rtp_generator.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/clock.h"
#include "test/testsupport/file_utils.h"

using webrtc::NetEq;
using webrtc::test::AudioLoop;
using webrtc::test::RtpGenerator;

namespace webrtc {
namespace test {

int64_t NetEqPerformanceTest::Run(int runtime_ms,
                                  int lossrate,
                                  double drift_factor) {
  const std::string kInputFileName =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  const int kSampRateHz = 32000;
  const std::string kDecoderName = "pcm16-swb32";
  const int kPayloadType = 95;

  NetEq::Config config;
  config.sample_rate_hz = kSampRateHz;
  webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
  auto audio_decoder_factory = CreateBuiltinAudioDecoderFactory();
  auto neteq =
      DefaultNetEqFactory().CreateNetEq(config, audio_decoder_factory, clock);

  if (!neteq->RegisterPayloadType(kPayloadType,
                                  SdpAudioFormat("l16", kSampRateHz, 1)))
    return -1;

  AudioLoop audio_loop;
  const size_t kMaxLoopLengthSamples = kSampRateHz * 10;
  const size_t kInputBlockSizeSamples = 60 * kSampRateHz / 1000;
  if (!audio_loop.Init(kInputFileName, kMaxLoopLengthSamples,
                       kInputBlockSizeSamples))
    return -1;

  int32_t time_now_ms = 0;

  RTPHeader rtp_header;
  RtpGenerator rtp_gen(kSampRateHz / 1000);

  rtp_gen.set_drift_factor(drift_factor);
  bool drift_flipped = false;
  int32_t packet_input_time_ms =
      rtp_gen.GetRtpHeader(kPayloadType, kInputBlockSizeSamples, &rtp_header);
  auto input_samples = audio_loop.GetNextBlock();
  if (input_samples.empty())
    exit(1);
  uint8_t input_payload[kInputBlockSizeSamples * sizeof(int16_t)];
  size_t payload_len = WebRtcPcm16b_Encode(input_samples.data(),
                                           input_samples.size(), input_payload);
  RTC_CHECK_EQ(sizeof(input_payload), payload_len);

  int64_t start_time_ms = clock->TimeInMilliseconds();
  AudioFrame out_frame;
  while (time_now_ms < runtime_ms) {
    while (packet_input_time_ms <= time_now_ms) {

      bool lost = false;
      if (lossrate > 0) {
        lost = ((rtp_header.sequenceNumber - 1) % lossrate) == 0;
      }
      if (!lost) {

        int error = neteq->InsertPacket(rtp_header, input_payload);
        if (error != NetEq::kOK)
          return -1;
      }

      packet_input_time_ms = rtp_gen.GetRtpHeader(
          kPayloadType, kInputBlockSizeSamples, &rtp_header);
      input_samples = audio_loop.GetNextBlock();
      if (input_samples.empty())
        return -1;
      payload_len = WebRtcPcm16b_Encode(input_samples.data(),
                                        input_samples.size(), input_payload);
      RTC_DCHECK_EQ(payload_len, kInputBlockSizeSamples * sizeof(int16_t));
    }

    bool muted;
    int error = neteq->GetAudio(&out_frame, &muted);
    RTC_CHECK(!muted);
    if (error != NetEq::kOK)
      return -1;

    RTC_DCHECK_EQ(out_frame.samples_per_channel_, (kSampRateHz * 10) / 1000);

    static const int kOutputBlockSizeMs = 10;
    time_now_ms += kOutputBlockSizeMs;
    if (time_now_ms >= runtime_ms / 2 && !drift_flipped) {

      rtp_gen.set_drift_factor(-drift_factor);
      drift_flipped = true;
    }
  }
  int64_t end_time_ms = clock->TimeInMilliseconds();
  return end_time_ms - start_time_ms;
}

}
}
