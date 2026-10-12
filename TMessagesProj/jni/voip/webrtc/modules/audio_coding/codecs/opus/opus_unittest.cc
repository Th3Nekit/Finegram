/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>

#include "modules/audio_coding/codecs/opus/opus_inst.h"
#include "modules/audio_coding/codecs/opus/opus_interface.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

namespace {

constexpr unsigned char kQuadChannelMapping[] = {0, 1, 2, 3};
constexpr int kQuadTotalStreams = 2;
constexpr int kQuadCoupledStreams = 2;

constexpr unsigned char kStereoChannelMapping[] = {0, 1};
constexpr int kStereoTotalStreams = 1;
constexpr int kStereoCoupledStreams = 1;

constexpr unsigned char kMonoChannelMapping[] = {0};
constexpr int kMonoTotalStreams = 1;
constexpr int kMonoCoupledStreams = 0;

void CreateSingleOrMultiStreamEncoder(WebRtcOpusEncInst** opus_encoder,
                                      int channels,
                                      int application,
                                      bool use_multistream,
                                      int encoder_sample_rate_hz) {
  EXPECT_TRUE(channels == 1 || channels == 2 || use_multistream);
  if (use_multistream) {
    EXPECT_EQ(encoder_sample_rate_hz, 48000);
    if (channels == 1) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamEncoderCreate(
                       opus_encoder, channels, application, kMonoTotalStreams,
                       kMonoCoupledStreams, kMonoChannelMapping));
    } else if (channels == 2) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamEncoderCreate(
                       opus_encoder, channels, application, kStereoTotalStreams,
                       kStereoCoupledStreams, kStereoChannelMapping));
    } else if (channels == 4) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamEncoderCreate(
                       opus_encoder, channels, application, kQuadTotalStreams,
                       kQuadCoupledStreams, kQuadChannelMapping));
    } else {
      EXPECT_TRUE(false) << channels;
    }
  } else {
    EXPECT_EQ(0, WebRtcOpus_EncoderCreate(opus_encoder, channels, application,
                                          encoder_sample_rate_hz));
  }
}

void CreateSingleOrMultiStreamDecoder(WebRtcOpusDecInst** opus_decoder,
                                      int channels,
                                      bool use_multistream,
                                      int decoder_sample_rate_hz) {
  EXPECT_TRUE(channels == 1 || channels == 2 || use_multistream);
  if (use_multistream) {
    EXPECT_EQ(decoder_sample_rate_hz, 48000);
    if (channels == 1) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamDecoderCreate(
                       opus_decoder, channels, kMonoTotalStreams,
                       kMonoCoupledStreams, kMonoChannelMapping));
    } else if (channels == 2) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamDecoderCreate(
                       opus_decoder, channels, kStereoTotalStreams,
                       kStereoCoupledStreams, kStereoChannelMapping));
    } else if (channels == 4) {
      EXPECT_EQ(0, WebRtcOpus_MultistreamDecoderCreate(
                       opus_decoder, channels, kQuadTotalStreams,
                       kQuadCoupledStreams, kQuadChannelMapping));
    } else {
      EXPECT_TRUE(false) << channels;
    }
  } else {
    EXPECT_EQ(0, WebRtcOpus_DecoderCreate(opus_decoder, channels,
                                          decoder_sample_rate_hz));
  }
}

int SamplesPerChannel(int sample_rate_hz, int duration_ms) {
  const int samples_per_ms = rtc::CheckedDivExact(sample_rate_hz, 1000);
  return samples_per_ms * duration_ms;
}

using test::AudioLoop;
using ::testing::Combine;
using ::testing::TestWithParam;
using ::testing::Values;

const size_t kMaxBytes = 2000;

class OpusTest
    : public TestWithParam<::testing::tuple<size_t, int, bool, int, int>> {
 protected:
  OpusTest() = default;

  void TestDtxEffect(bool dtx, int block_length_ms);

  void TestCbrEffect(bool dtx, int block_length_ms);

  void PrepareSpeechData(int block_length_ms, int loop_length_ms);

  int EncodeDecode(WebRtcOpusEncInst* encoder,
                   rtc::ArrayView<const int16_t> input_audio,
                   WebRtcOpusDecInst* decoder,
                   int16_t* output_audio,
                   int16_t* audio_type);

  void SetMaxPlaybackRate(WebRtcOpusEncInst* encoder,
                          opus_int32 expect,
                          int32_t set);

  void CheckAudioBounded(const int16_t* audio,
                         size_t samples,
                         size_t channels,
                         uint16_t bound) const;

  WebRtcOpusEncInst* opus_encoder_ = nullptr;
  WebRtcOpusDecInst* opus_decoder_ = nullptr;
  AudioLoop speech_data_;
  uint8_t bitstream_[kMaxBytes];
  size_t encoded_bytes_ = 0;
  const size_t channels_{std::get<0>(GetParam())};
  const int application_{std::get<1>(GetParam())};
  const bool use_multistream_{std::get<2>(GetParam())};
  const int encoder_sample_rate_hz_{std::get<3>(GetParam())};
  const int decoder_sample_rate_hz_{std::get<4>(GetParam())};
};

}

INSTANTIATE_TEST_SUITE_P(Singlestream,
                         OpusTest,
                         testing::Combine(testing::Values(1, 2),
                                          testing::Values(0, 1),
                                          testing::Values(false),
                                          testing::Values(16000, 48000),
                                          testing::Values(16000, 48000)));

INSTANTIATE_TEST_SUITE_P(
    Multistream,
    OpusTest,
    testing::Values(std::make_tuple(1, 0, true, 48000, 48000),
                    std::make_tuple(2, 1, true, 48000, 48000),
                    std::make_tuple(4, 0, true, 48000, 48000),
                    std::make_tuple(4, 1, true, 48000, 48000)));

void OpusTest::PrepareSpeechData(int block_length_ms, int loop_length_ms) {
  std::map<int, std::string> channel_to_basename = {
      {1, "audio_coding/testfile32kHz"},
      {2, "audio_coding/teststereo32kHz"},
      {4, "audio_coding/speech_4_channels_48k_one_second"}};
  std::map<int, std::string> channel_to_suffix = {
      {1, "pcm"}, {2, "pcm"}, {4, "wav"}};
  const std::string file_name = webrtc::test::ResourcePath(
      channel_to_basename[channels_], channel_to_suffix[channels_]);
  if (loop_length_ms < block_length_ms) {
    loop_length_ms = block_length_ms;
  }
  const int sample_rate_khz =
      rtc::CheckedDivExact(encoder_sample_rate_hz_, 1000);
  EXPECT_TRUE(speech_data_.Init(file_name,
                                loop_length_ms * sample_rate_khz * channels_,
                                block_length_ms * sample_rate_khz * channels_));
}

void OpusTest::SetMaxPlaybackRate(WebRtcOpusEncInst* encoder,
                                  opus_int32 expect,
                                  int32_t set) {
  opus_int32 bandwidth;
  EXPECT_EQ(0, WebRtcOpus_SetMaxPlaybackRate(opus_encoder_, set));
  EXPECT_EQ(0, WebRtcOpus_GetMaxPlaybackRate(opus_encoder_, &bandwidth));
  EXPECT_EQ(expect, bandwidth);
}

void OpusTest::CheckAudioBounded(const int16_t* audio,
                                 size_t samples,
                                 size_t channels,
                                 uint16_t bound) const {
  for (size_t i = 0; i < samples; ++i) {
    for (size_t c = 0; c < channels; ++c) {
      ASSERT_GE(audio[i * channels + c], -bound);
      ASSERT_LE(audio[i * channels + c], bound);
    }
  }
}

int OpusTest::EncodeDecode(WebRtcOpusEncInst* encoder,
                           rtc::ArrayView<const int16_t> input_audio,
                           WebRtcOpusDecInst* decoder,
                           int16_t* output_audio,
                           int16_t* audio_type) {
  const int input_samples_per_channel =
      rtc::CheckedDivExact(input_audio.size(), channels_);
  int encoded_bytes_int =
      WebRtcOpus_Encode(encoder, input_audio.data(), input_samples_per_channel,
                        kMaxBytes, bitstream_);
  EXPECT_GE(encoded_bytes_int, 0);
  encoded_bytes_ = static_cast<size_t>(encoded_bytes_int);
  if (encoded_bytes_ != 0) {
    int est_len = WebRtcOpus_DurationEst(decoder, bitstream_, encoded_bytes_);
    int act_len = WebRtcOpus_Decode(decoder, bitstream_, encoded_bytes_,
                                    output_audio, audio_type);
    EXPECT_EQ(est_len, act_len);
    return act_len;
  } else {
    int total_dtx_len = 0;
    const int output_samples_per_channel = input_samples_per_channel *
                                           decoder_sample_rate_hz_ /
                                           encoder_sample_rate_hz_;
    while (total_dtx_len < output_samples_per_channel) {
      int est_len = WebRtcOpus_DurationEst(decoder, NULL, 0);
      int act_len = WebRtcOpus_Decode(decoder, NULL, 0,
                                      &output_audio[total_dtx_len * channels_],
                                      audio_type);
      EXPECT_EQ(est_len, act_len);
      total_dtx_len += act_len;
    }
    return total_dtx_len;
  }
}

void OpusTest::TestDtxEffect(bool dtx, int block_length_ms) {
  PrepareSpeechData(block_length_ms, 2000);
  const size_t input_samples =
      rtc::CheckedDivExact(encoder_sample_rate_hz_, 1000) * block_length_ms;
  const size_t output_samples =
      rtc::CheckedDivExact(decoder_sample_rate_hz_, 1000) * block_length_ms;

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  EXPECT_EQ(
      0, WebRtcOpus_SetBitRate(opus_encoder_, channels_ == 1 ? 32000 : 64000));

  std::vector<int16_t> silence(input_samples * channels_, 0);

  EXPECT_EQ(0, dtx ? WebRtcOpus_EnableDtx(opus_encoder_)
                   : WebRtcOpus_DisableDtx(opus_encoder_));

  int16_t audio_type;
  int16_t* output_data_decode = new int16_t[output_samples * channels_];

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(output_samples,
              static_cast<size_t>(EncodeDecode(
                  opus_encoder_, speech_data_.GetNextBlock(), opus_decoder_,
                  output_data_decode, &audio_type)));

    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1U);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);
    }
  }

  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(output_samples, static_cast<size_t>(EncodeDecode(
                                  opus_encoder_, silence, opus_decoder_,
                                  output_data_decode, &audio_type)));
    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1U);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);
    } else if (encoded_bytes_ == 1) {
      EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);
      break;
    }
  }

  const int max_dtx_frames =
      (encoder_sample_rate_hz_ == 16000 ? 800 : 400) / block_length_ms + 1;

  const int kRunTimeMs = 4500;

  const int kCheckTimeMs = 4000;

#if defined(OPUS_FIXED_POINT)

  const uint16_t kOutputValueBound = 30;

#else
  const uint16_t kOutputValueBound = 2;
#endif

  int time = 0;
  while (time < kRunTimeMs) {

    int i = 0;
    for (; i < max_dtx_frames; ++i) {
      time += block_length_ms;
      EXPECT_EQ(output_samples, static_cast<size_t>(EncodeDecode(
                                    opus_encoder_, silence, opus_decoder_,
                                    output_data_decode, &audio_type)));
      if (dtx) {
        if (encoded_bytes_ > 1)
          break;
        EXPECT_EQ(0U, encoded_bytes_)
            << "Opus should have entered DTX mode.";
        EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
        EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
        EXPECT_EQ(2, audio_type);
        if (time >= kCheckTimeMs) {
          CheckAudioBounded(output_data_decode, output_samples, channels_,
                            kOutputValueBound);
        }
      } else {
        EXPECT_GT(encoded_bytes_, 1U);
        EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
        EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
        EXPECT_EQ(0, audio_type);
      }
    }

    if (dtx) {

      EXPECT_GT(i, 1);
    }

    EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);

    time += block_length_ms;
    EXPECT_EQ(output_samples, static_cast<size_t>(EncodeDecode(
                                  opus_encoder_, silence, opus_decoder_,
                                  output_data_decode, &audio_type)));
    if (dtx) {
      EXPECT_EQ(1U, encoded_bytes_);
      EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);
      if (time >= kCheckTimeMs) {
        CheckAudioBounded(output_data_decode, output_samples, channels_,
                          kOutputValueBound);
      }
    } else {
      EXPECT_GT(encoded_bytes_, 1U);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);
    }
  }

  silence[0] = 10000;
  if (dtx) {

    EXPECT_EQ(output_samples, static_cast<size_t>(EncodeDecode(
                                  opus_encoder_, silence, opus_decoder_,
                                  output_data_decode, &audio_type)));
    EXPECT_GT(encoded_bytes_, 1U);
    EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);
  }

  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

void OpusTest::TestCbrEffect(bool cbr, int block_length_ms) {
  PrepareSpeechData(block_length_ms, 2000);
  const size_t output_samples =
      rtc::CheckedDivExact(decoder_sample_rate_hz_, 1000) * block_length_ms;

  int32_t max_pkt_size_diff = 0;
  int32_t prev_pkt_size = 0;

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  EXPECT_EQ(
      0, WebRtcOpus_SetBitRate(opus_encoder_, channels_ == 1 ? 32000 : 64000));

  EXPECT_EQ(0, cbr ? WebRtcOpus_EnableCbr(opus_encoder_)
                   : WebRtcOpus_DisableCbr(opus_encoder_));

  int16_t audio_type;
  std::vector<int16_t> audio_out(output_samples * channels_);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(output_samples,
              static_cast<size_t>(
                  EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                               opus_decoder_, audio_out.data(), &audio_type)));

    if (prev_pkt_size > 0) {
      int32_t diff = std::abs((int32_t)encoded_bytes_ - prev_pkt_size);
      max_pkt_size_diff = std::max(max_pkt_size_diff, diff);
    }
    prev_pkt_size = rtc::checked_cast<int32_t>(encoded_bytes_);
  }

  if (cbr) {
    EXPECT_EQ(max_pkt_size_diff, 0);
  } else {
    EXPECT_GT(max_pkt_size_diff, 0);
  }

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST(OpusTest, OpusCreateFail) {
  WebRtcOpusEncInst* opus_encoder;
  WebRtcOpusDecInst* opus_decoder;

  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(NULL, 1, 0, 48000));

  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_encoder, 257, 0, 48000));

  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_encoder, 1, 2, 48000));

  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_encoder, 1, 0, 12345));

  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(NULL, 1, 48000));

  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(&opus_decoder, 257, 48000));

  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(&opus_decoder, 1, 12345));
}

TEST(OpusTest, OpusFreeFail) {

  EXPECT_EQ(-1, WebRtcOpus_EncoderFree(NULL));
  EXPECT_EQ(-1, WebRtcOpus_DecoderFree(NULL));
}

TEST_P(OpusTest, OpusCreateFree) {
  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);
  EXPECT_TRUE(opus_encoder_ != NULL);
  EXPECT_TRUE(opus_decoder_ != NULL);

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

#define ENCODER_CTL(inst, vargs)               \
  inst->encoder                                \
      ? opus_encoder_ctl(inst->encoder, vargs) \
      : opus_multistream_encoder_ctl(inst->multistream_encoder, vargs)

TEST_P(OpusTest, OpusEncodeDecode) {
  PrepareSpeechData(20, 20);

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  EXPECT_EQ(
      0, WebRtcOpus_SetBitRate(opus_encoder_, channels_ == 1 ? 32000 : 64000));

  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  opus_int32 app;
  ENCODER_CTL(opus_encoder_, OPUS_GET_APPLICATION(&app));
  EXPECT_EQ(application_ == 0 ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO,
            app);

  int16_t audio_type;
  const int decode_samples_per_channel =
      SamplesPerChannel(decoder_sample_rate_hz_,        20);
  int16_t* output_data_decode =
      new int16_t[decode_samples_per_channel * channels_];
  EXPECT_EQ(decode_samples_per_channel,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         opus_decoder_, output_data_decode, &audio_type));

  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusSetBitRate) {

  EXPECT_EQ(-1, WebRtcOpus_SetBitRate(opus_encoder_, 60000));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 30000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 60000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 300000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 600000));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusSetComplexity) {

  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_encoder_, 9));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);

  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_encoder_, 0));
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_encoder_, 10));
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_encoder_, 11));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusSetBandwidth) {
  if (channels_ > 2) {

    return;
  }
  PrepareSpeechData(20, 20);

  int16_t audio_type;
  const int decode_samples_per_channel =
      SamplesPerChannel(decoder_sample_rate_hz_,        20);
  std::unique_ptr<int16_t[]> output_data_decode(
      new int16_t[decode_samples_per_channel * channels_]());

  EXPECT_EQ(-1,
            WebRtcOpus_SetBandwidth(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND));
  EXPECT_EQ(-1, WebRtcOpus_GetBandwidth(opus_encoder_));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  EXPECT_EQ(-1, WebRtcOpus_SetBandwidth(opus_encoder_,
                                        OPUS_BANDWIDTH_NARROWBAND - 1));
  EXPECT_EQ(0,
            WebRtcOpus_SetBandwidth(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND));
  EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(), opus_decoder_,
               output_data_decode.get(), &audio_type);
  EXPECT_EQ(OPUS_BANDWIDTH_NARROWBAND, WebRtcOpus_GetBandwidth(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_SetBandwidth(opus_encoder_, OPUS_BANDWIDTH_FULLBAND));
  EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(), opus_decoder_,
               output_data_decode.get(), &audio_type);
  EXPECT_EQ(encoder_sample_rate_hz_ == 16000 ? OPUS_BANDWIDTH_WIDEBAND
                                             : OPUS_BANDWIDTH_FULLBAND,
            WebRtcOpus_GetBandwidth(opus_encoder_));
  EXPECT_EQ(
      -1, WebRtcOpus_SetBandwidth(opus_encoder_, OPUS_BANDWIDTH_FULLBAND + 1));
  EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(), opus_decoder_,
               output_data_decode.get(), &audio_type);
  EXPECT_EQ(encoder_sample_rate_hz_ == 16000 ? OPUS_BANDWIDTH_WIDEBAND
                                             : OPUS_BANDWIDTH_FULLBAND,
            WebRtcOpus_GetBandwidth(opus_encoder_));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusForceChannels) {

  EXPECT_EQ(-1, WebRtcOpus_SetForceChannels(opus_encoder_, 1));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  ASSERT_NE(nullptr, opus_encoder_);

  if (channels_ >= 2) {
    EXPECT_EQ(-1, WebRtcOpus_SetForceChannels(opus_encoder_, 3));
    EXPECT_EQ(0, WebRtcOpus_SetForceChannels(opus_encoder_, 2));
    EXPECT_EQ(0, WebRtcOpus_SetForceChannels(opus_encoder_, 1));
    EXPECT_EQ(0, WebRtcOpus_SetForceChannels(opus_encoder_, 0));
  } else {
    EXPECT_EQ(-1, WebRtcOpus_SetForceChannels(opus_encoder_, 2));
    EXPECT_EQ(0, WebRtcOpus_SetForceChannels(opus_encoder_, 1));
    EXPECT_EQ(0, WebRtcOpus_SetForceChannels(opus_encoder_, 0));
  }

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusDecodeInit) {
  PrepareSpeechData(20, 20);

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  int16_t audio_type;
  const int decode_samples_per_channel =
      SamplesPerChannel(decoder_sample_rate_hz_,        20);
  int16_t* output_data_decode =
      new int16_t[decode_samples_per_channel * channels_];
  EXPECT_EQ(decode_samples_per_channel,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         opus_decoder_, output_data_decode, &audio_type));

  WebRtcOpus_DecoderInit(opus_decoder_);

  EXPECT_EQ(decode_samples_per_channel,
            WebRtcOpus_Decode(opus_decoder_, bitstream_, encoded_bytes_,
                              output_data_decode, &audio_type));

  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusEnableDisableFec) {

  EXPECT_EQ(-1, WebRtcOpus_EnableFec(opus_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableFec(opus_encoder_));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);

  EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DisableFec(opus_encoder_));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusEnableDisableDtx) {

  EXPECT_EQ(-1, WebRtcOpus_EnableDtx(opus_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableDtx(opus_encoder_));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);

  opus_int32 dtx;

  ENCODER_CTL(opus_encoder_, OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_encoder_));
  ENCODER_CTL(opus_encoder_, OPUS_GET_DTX(&dtx));
  EXPECT_EQ(1, dtx);

  EXPECT_EQ(0, WebRtcOpus_DisableDtx(opus_encoder_));
  ENCODER_CTL(opus_encoder_, OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusDtxOff) {
  TestDtxEffect(false, 10);
  TestDtxEffect(false, 20);
  TestDtxEffect(false, 40);
}

TEST_P(OpusTest, OpusDtxOn) {
  if (channels_ > 2 || application_ != 0) {

    return;
  }
  TestDtxEffect(true, 10);
  TestDtxEffect(true, 20);
  TestDtxEffect(true, 40);
}

TEST_P(OpusTest, OpusCbrOff) {
  TestCbrEffect(false, 10);
  TestCbrEffect(false, 20);
  TestCbrEffect(false, 40);
}

TEST_P(OpusTest, OpusCbrOn) {
  TestCbrEffect(true, 10);
  TestCbrEffect(true, 20);
  TestCbrEffect(true, 40);
}

TEST_P(OpusTest, OpusSetPacketLossRate) {

  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, 50));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);

  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_encoder_, 50));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, -1));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, 101));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusSetMaxPlaybackRate) {

  EXPECT_EQ(-1, WebRtcOpus_SetMaxPlaybackRate(opus_encoder_, 20000));

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);

  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_FULLBAND, 48000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_FULLBAND, 24001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_SUPERWIDEBAND, 24000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_SUPERWIDEBAND, 16001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_WIDEBAND, 16000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_WIDEBAND, 12001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_MEDIUMBAND, 12000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_MEDIUMBAND, 8001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND, 8000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND, 4000);

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusDecodePlc) {
  PrepareSpeechData(20, 20);

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  EXPECT_EQ(
      0, WebRtcOpus_SetBitRate(opus_encoder_, channels_ == 1 ? 32000 : 64000));

  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  int16_t audio_type;
  const int decode_samples_per_channel =
      SamplesPerChannel(decoder_sample_rate_hz_,        20);
  int16_t* output_data_decode =
      new int16_t[decode_samples_per_channel * channels_];
  EXPECT_EQ(decode_samples_per_channel,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         opus_decoder_, output_data_decode, &audio_type));

  constexpr int kPlcDurationMs = 10;
  const int plc_samples = decoder_sample_rate_hz_ * kPlcDurationMs / 1000;
  int16_t* plc_buffer = new int16_t[plc_samples * channels_];
  EXPECT_EQ(plc_samples,
            WebRtcOpus_Decode(opus_decoder_, NULL, 0, plc_buffer, &audio_type));

  delete[] plc_buffer;
  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusDurationEstimation) {
  PrepareSpeechData(20, 20);

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);

  auto speech_block = speech_data_.GetNextBlock();
  int encoded_bytes_int = WebRtcOpus_Encode(
      opus_encoder_, speech_block.data(),
      rtc::CheckedDivExact(speech_block.size(), 2 * channels_), kMaxBytes,
      bitstream_);
  EXPECT_GE(encoded_bytes_int, 0);
  EXPECT_EQ(SamplesPerChannel(decoder_sample_rate_hz_,        10),
            WebRtcOpus_DurationEst(opus_decoder_, bitstream_,
                                   static_cast<size_t>(encoded_bytes_int)));

  speech_block = speech_data_.GetNextBlock();
  encoded_bytes_int =
      WebRtcOpus_Encode(opus_encoder_, speech_block.data(),
                        rtc::CheckedDivExact(speech_block.size(), channels_),
                        kMaxBytes, bitstream_);
  EXPECT_GE(encoded_bytes_int, 0);
  EXPECT_EQ(SamplesPerChannel(decoder_sample_rate_hz_,        20),
            WebRtcOpus_DurationEst(opus_decoder_, bitstream_,
                                   static_cast<size_t>(encoded_bytes_int)));

  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusDecodeRepacketized) {
  if (channels_ > 2) {

    return;
  }
  constexpr size_t kPackets = 6;

  PrepareSpeechData(20, 20 * kPackets);

  CreateSingleOrMultiStreamEncoder(&opus_encoder_, channels_, application_,
                                   use_multistream_, encoder_sample_rate_hz_);
  ASSERT_NE(nullptr, opus_encoder_);
  CreateSingleOrMultiStreamDecoder(&opus_decoder_, channels_, use_multistream_,
                                   decoder_sample_rate_hz_);
  ASSERT_NE(nullptr, opus_decoder_);

  EXPECT_EQ(
      0, WebRtcOpus_SetBitRate(opus_encoder_, channels_ == 1 ? 32000 : 64000));

  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  int16_t audio_type;
  const int decode_samples_per_channel =
      SamplesPerChannel(decoder_sample_rate_hz_,        20);
  std::unique_ptr<int16_t[]> output_data_decode(
      new int16_t[kPackets * decode_samples_per_channel * channels_]);
  OpusRepacketizer* rp = opus_repacketizer_create();

  size_t num_packets = 0;
  constexpr size_t kMaxCycles = 100;
  for (size_t idx = 0; idx < kMaxCycles; ++idx) {
    auto speech_block = speech_data_.GetNextBlock();
    encoded_bytes_ =
        WebRtcOpus_Encode(opus_encoder_, speech_block.data(),
                          rtc::CheckedDivExact(speech_block.size(), channels_),
                          kMaxBytes, bitstream_);
    if (opus_repacketizer_cat(rp, bitstream_,
                              rtc::checked_cast<opus_int32>(encoded_bytes_)) ==
        OPUS_OK) {
      ++num_packets;
      if (num_packets == kPackets) {
        break;
      }
    } else {

      opus_repacketizer_init(rp);
      num_packets = 0;
    }
  }
  EXPECT_EQ(kPackets, num_packets);

  encoded_bytes_ = opus_repacketizer_out(rp, bitstream_, kMaxBytes);

  EXPECT_EQ(decode_samples_per_channel * kPackets,
            static_cast<size_t>(WebRtcOpus_DurationEst(
                opus_decoder_, bitstream_, encoded_bytes_)));

  EXPECT_EQ(decode_samples_per_channel * kPackets,
            static_cast<size_t>(
                WebRtcOpus_Decode(opus_decoder_, bitstream_, encoded_bytes_,
                                  output_data_decode.get(), &audio_type)));

  opus_repacketizer_destroy(rp);
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST(OpusVadTest, CeltUnknownStatus) {
  const uint8_t celt[] = {0x80};
  EXPECT_EQ(WebRtcOpus_PacketHasVoiceActivity(celt, 1), -1);
}

TEST(OpusVadTest, Mono20msVadSet) {
  uint8_t silk20msMonoVad[] = {0x78, 0x80};
  EXPECT_TRUE(WebRtcOpus_PacketHasVoiceActivity(silk20msMonoVad, 2));
}

TEST(OpusVadTest, Mono20MsVadUnset) {
  uint8_t silk20msMonoSilence[] = {0x78, 0x00};
  EXPECT_FALSE(WebRtcOpus_PacketHasVoiceActivity(silk20msMonoSilence, 2));
}

TEST(OpusVadTest, Stereo20MsVadOnSideChannel) {
  uint8_t silk20msStereoVadSideChannel[] = {0x78 | 0x04, 0x20};
  EXPECT_TRUE(
      WebRtcOpus_PacketHasVoiceActivity(silk20msStereoVadSideChannel, 2));
}

TEST(OpusVadTest, TwoOpusMonoFramesVadOnSecond) {
  uint8_t twoMonoFrames[] = {0x78 | 0x1, 0x00, 0x80};
  EXPECT_TRUE(WebRtcOpus_PacketHasVoiceActivity(twoMonoFrames, 3));
}

}
