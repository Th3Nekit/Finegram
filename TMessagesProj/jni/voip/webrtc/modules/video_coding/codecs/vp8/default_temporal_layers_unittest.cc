/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/default_temporal_layers.h"

#include <cstdint>
#include <memory>

#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/vp8_frame_config.h"
#include "modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "vpx/vp8cx.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::Each;

enum {
  kTemporalUpdateLast = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF,
  kTemporalUpdateGoldenWithoutDependency =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGolden =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltrefWithoutDependency =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF |
      VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateAltref = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateNone = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                        VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefAltRef =
      VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefGolden =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
      VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateNoneNoRefGoldenAltRef =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_REF_ARF |
      VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY,
  kTemporalUpdateGoldenWithoutDependencyRefAltRef =
      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateGoldenRefAltRef = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST,
  kTemporalUpdateLastRefAltRef =
      VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
  kTemporalUpdateLastAndGoldenRefAltRef =
      VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF,
};

using BufferFlags = Vp8FrameConfig::BufferFlags;
using Vp8BufferReference = Vp8FrameConfig::Vp8BufferReference;

constexpr uint8_t kNone = static_cast<uint8_t>(Vp8BufferReference::kNone);
constexpr uint8_t kLast = static_cast<uint8_t>(Vp8BufferReference::kLast);
constexpr uint8_t kGolden = static_cast<uint8_t>(Vp8BufferReference::kGolden);
constexpr uint8_t kAltref = static_cast<uint8_t>(Vp8BufferReference::kAltref);
constexpr uint8_t kAll = kLast | kGolden | kAltref;

constexpr int ToVp8CodecFlags(uint8_t referenced_buffers,
                              uint8_t updated_buffers,
                              bool update_entropy) {
  return (((referenced_buffers & kLast) == 0) ? VP8_EFLAG_NO_REF_LAST : 0) |
         (((referenced_buffers & kGolden) == 0) ? VP8_EFLAG_NO_REF_GF : 0) |
         (((referenced_buffers & kAltref) == 0) ? VP8_EFLAG_NO_REF_ARF : 0) |
         (((updated_buffers & kLast) == 0) ? VP8_EFLAG_NO_UPD_LAST : 0) |
         (((updated_buffers & kGolden) == 0) ? VP8_EFLAG_NO_UPD_GF : 0) |
         (((updated_buffers & kAltref) == 0) ? VP8_EFLAG_NO_UPD_ARF : 0) |
         (update_entropy ? 0 : VP8_EFLAG_NO_UPD_ENTROPY);
}

constexpr int kKeyFrameFlags = ToVp8CodecFlags(kNone, kAll, true);

std::vector<uint32_t> GetTemporalLayerRates(int target_bitrate_kbps,
                                            int framerate_fps,
                                            int num_temporal_layers) {
  VideoCodec codec;
  codec.codecType = VideoCodecType::kVideoCodecVP8;
  codec.numberOfSimulcastStreams = 1;
  codec.maxBitrate = target_bitrate_kbps;
  codec.maxFramerate = framerate_fps;
  codec.simulcastStream[0].targetBitrate = target_bitrate_kbps;
  codec.simulcastStream[0].maxBitrate = target_bitrate_kbps;
  codec.simulcastStream[0].numberOfTemporalLayers = num_temporal_layers;
  codec.simulcastStream[0].active = true;
  SimulcastRateAllocator allocator(codec);
  return allocator
      .Allocate(
          VideoBitrateAllocationParameters(target_bitrate_kbps, framerate_fps))
      .GetTemporalLayerAllocation(0);
}

constexpr int kDefaultBitrateBps = 500;
constexpr int kDefaultFramerate = 30;
constexpr int kDefaultBytesPerFrame =
    (kDefaultBitrateBps / 8) / kDefaultFramerate;
constexpr int kDefaultQp = 2;
}

class TemporalLayersTest : public ::testing::Test {
 public:
  ~TemporalLayersTest() override = default;

  CodecSpecificInfo* IgnoredCodecSpecificInfo() {
    codec_specific_info_ = std::make_unique<CodecSpecificInfo>();
    return codec_specific_info_.get();
  }

 private:
  std::unique_ptr<CodecSpecificInfo> codec_specific_info_;
};

TEST_F(TemporalLayersTest, 2Layers) {
  constexpr int kNumLayers = 2;
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  constexpr size_t kPatternSize = 4;
  constexpr size_t kRepetitions = 4;

  const int expected_flags[kPatternSize] = {
      ToVp8CodecFlags(kLast, kLast, true),
      ToVp8CodecFlags(kLast, kGolden, true),
      ToVp8CodecFlags(kLast, kLast, true),
      ToVp8CodecFlags(kLast | kGolden, kNone, false),
  };
  const int expected_temporal_idx[kPatternSize] = {0, 1, 0, 1};
  const bool expected_layer_sync[kPatternSize] = {false, true, false, false};

  uint32_t timestamp = 0;
  for (size_t i = 0; i < kPatternSize * kRepetitions; ++i) {
    const size_t ind = i % kPatternSize;
    const bool is_keyframe = (i == 0);
    CodecSpecificInfo info;
    Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
    EXPECT_EQ(is_keyframe ? kKeyFrameFlags : expected_flags[ind],
              LibvpxVp8Encoder::EncodeFlags(tl_config))
        << i;
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, is_keyframe,
                    kDefaultQp, &info);
    EXPECT_TRUE(checker.CheckTemporalConfig(is_keyframe, tl_config));
    EXPECT_EQ(expected_temporal_idx[ind], info.codecSpecific.VP8.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[ind], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[ind], tl_config.encoder_layer_id);
    EXPECT_EQ(is_keyframe || expected_layer_sync[ind],
              info.codecSpecific.VP8.layerSync);
    EXPECT_EQ(expected_layer_sync[ind], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST_F(TemporalLayersTest, 3Layers) {
  constexpr int kNumLayers = 3;
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  int expected_flags[16] = {
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefGoldenAltRef,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateGolden,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefGoldenAltRef,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateGolden,
      kTemporalUpdateNoneNoRefAltRef,
  };
  int expected_temporal_idx[16] = {0, 2, 1, 2, 0, 2, 1, 2,
                                   0, 2, 1, 2, 0, 2, 1, 2};

  bool expected_layer_sync[16] = {false, true,  true,  false, false, false,
                                  false, false, false, true,  true,  false,
                                  false, false, false, false};

  unsigned int timestamp = 0;
  for (int i = 0; i < 16; ++i) {
    const bool is_keyframe = (i == 0);
    CodecSpecificInfo info;
    Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
    EXPECT_EQ(is_keyframe ? kKeyFrameFlags : expected_flags[i],
              LibvpxVp8Encoder::EncodeFlags(tl_config))
        << i;
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, is_keyframe,
                    kDefaultQp, &info);
    EXPECT_TRUE(checker.CheckTemporalConfig(is_keyframe, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], info.codecSpecific.VP8.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(is_keyframe || expected_layer_sync[i],
              info.codecSpecific.VP8.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST_F(TemporalLayersTest, Alternative3Layers) {
  constexpr int kNumLayers = 3;
  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  int expected_flags[8] = {kTemporalUpdateLast,
                           kTemporalUpdateAltrefWithoutDependency,
                           kTemporalUpdateGoldenWithoutDependency,
                           kTemporalUpdateNone,
                           kTemporalUpdateLast,
                           kTemporalUpdateAltrefWithoutDependency,
                           kTemporalUpdateGoldenWithoutDependency,
                           kTemporalUpdateNone};
  int expected_temporal_idx[8] = {0, 2, 1, 2, 0, 2, 1, 2};

  bool expected_layer_sync[8] = {false, true, true, false,
                                 false, true, true, false};

  unsigned int timestamp = 0;
  for (int i = 0; i < 8; ++i) {
    const bool is_keyframe = (i == 0);
    CodecSpecificInfo info;
    Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
    EXPECT_EQ(is_keyframe ? kKeyFrameFlags : expected_flags[i],
              LibvpxVp8Encoder::EncodeFlags(tl_config))
        << i;
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, is_keyframe,
                    kDefaultQp, &info);
    EXPECT_TRUE(checker.CheckTemporalConfig(is_keyframe, tl_config));
    EXPECT_EQ(expected_temporal_idx[i], info.codecSpecific.VP8.temporalIdx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.packetizer_temporal_idx);
    EXPECT_EQ(expected_temporal_idx[i], tl_config.encoder_layer_id);
    EXPECT_EQ(is_keyframe || expected_layer_sync[i],
              info.codecSpecific.VP8.layerSync);
    EXPECT_EQ(expected_layer_sync[i], tl_config.layer_sync);
    timestamp += 3000;
  }
}

TEST_F(TemporalLayersTest, SearchOrder) {
  constexpr int kNumLayers = 3;
  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  uint32_t timestamp = 0;
  Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_EQ(tl_config.first_reference, Vp8BufferReference::kLast);
  EXPECT_EQ(tl_config.second_reference, Vp8BufferReference::kNone);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_EQ(tl_config.first_reference, Vp8BufferReference::kLast);
  EXPECT_EQ(tl_config.second_reference, Vp8BufferReference::kNone);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_EQ(tl_config.first_reference, Vp8BufferReference::kGolden);
  EXPECT_EQ(tl_config.second_reference, Vp8BufferReference::kAltref);
}

TEST_F(TemporalLayersTest, SearchOrderWithDrop) {
  constexpr int kNumLayers = 3;
  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  uint32_t timestamp = 0;
  Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_EQ(tl_config.first_reference, Vp8BufferReference::kLast);
  EXPECT_EQ(tl_config.second_reference, Vp8BufferReference::kNone);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, 0, false, 0, nullptr);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_EQ(tl_config.first_reference, Vp8BufferReference::kAltref);
  EXPECT_EQ(tl_config.second_reference, Vp8BufferReference::kLast);
}

TEST_F(TemporalLayersTest, DoesNotReferenceDroppedFrames) {
  constexpr int kNumLayers = 3;

  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  uint32_t timestamp = 0;
  Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, 0, false, 0, nullptr);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, 0, false, 0, nullptr);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_TRUE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_TRUE(tl_config.arf_buffer_flags & BufferFlags::kReference);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_TRUE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_TRUE(tl_config.arf_buffer_flags & BufferFlags::kReference);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, 0, false, 0, nullptr);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, 0, false, 0, nullptr);

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.arf_buffer_flags & BufferFlags::kReference);
}

TEST_F(TemporalLayersTest, DoesNotReferenceUnlessGuaranteedToExist) {
  constexpr int kNumLayers = 3;

  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  uint32_t timestamp = 0;
  Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  for (int i = 0; i < 7; ++i) {
    tl_config = tl.NextFrameConfig(0, ++timestamp);
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                    IgnoredCodecSpecificInfo());
  }

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);

  tl_config = tl.NextFrameConfig(0, timestamp + 1);
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.arf_buffer_flags & BufferFlags::kReference);

  tl_config = tl.NextFrameConfig(0, timestamp + 2);

  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl.OnEncodeDone(0, ++timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl.OnEncodeDone(0, ++timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp + 1);
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_TRUE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.arf_buffer_flags & BufferFlags::kReference);
}

TEST_F(TemporalLayersTest, DoesNotReferenceUnlessGuaranteedToExistLongDelay) {
  constexpr int kNumLayers = 3;

  ScopedFieldTrials field_trial("WebRTC-UseShortVP8TL3Pattern/Enabled/");
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  uint32_t timestamp = 0;
  Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  for (int i = 0; i < 3; ++i) {
    tl_config = tl.NextFrameConfig(0, ++timestamp);
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                    IgnoredCodecSpecificInfo());
  }

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, ++timestamp);
  tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, timestamp + 1);
  tl_config = tl.NextFrameConfig(0, timestamp + 2);

  tl_config = tl.NextFrameConfig(0, timestamp + 3);
  tl_config = tl.NextFrameConfig(0, timestamp + 4);

  tl.OnEncodeDone(0, timestamp + 1, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl.OnEncodeDone(0, timestamp + 2, kDefaultBytesPerFrame, false, kDefaultQp,
                  IgnoredCodecSpecificInfo());

  tl_config = tl.NextFrameConfig(0, timestamp + 5);
  EXPECT_TRUE(tl_config.last_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.golden_buffer_flags & BufferFlags::kReference);
  EXPECT_FALSE(tl_config.arf_buffer_flags & BufferFlags::kReference);
}

TEST_F(TemporalLayersTest, KeyFrame) {
  constexpr int kNumLayers = 3;
  DefaultTemporalLayers tl(kNumLayers);
  DefaultTemporalLayersChecker checker(kNumLayers);
  tl.OnRatesUpdated(0,
                    GetTemporalLayerRates(kDefaultBytesPerFrame,
                                          kDefaultFramerate, kNumLayers),
                    kDefaultFramerate);
  tl.UpdateConfiguration(0);

  int expected_flags[8] = {
      kTemporalUpdateLastRefAltRef,
      kTemporalUpdateNoneNoRefGoldenAltRef,
      kTemporalUpdateGoldenWithoutDependency,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateLast,
      kTemporalUpdateNoneNoRefAltRef,
      kTemporalUpdateGolden,
      kTemporalUpdateNone,
  };
  int expected_temporal_idx[8] = {0, 2, 1, 2, 0, 2, 1, 2};
  bool expected_layer_sync[8] = {true,  true,  true,  false,
                                 false, false, false, false};

  uint32_t timestamp = 0;
  for (int i = 0; i < 7; ++i) {

    for (int j = 1; j <= i; ++j) {

      Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
      EXPECT_EQ(expected_flags[j], LibvpxVp8Encoder::EncodeFlags(tl_config))
          << j;
      tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, false, kDefaultQp,
                      IgnoredCodecSpecificInfo());
      EXPECT_TRUE(checker.CheckTemporalConfig(false, tl_config));
      EXPECT_EQ(expected_temporal_idx[j], tl_config.packetizer_temporal_idx);
      EXPECT_EQ(expected_temporal_idx[j], tl_config.encoder_layer_id);
      EXPECT_EQ(expected_layer_sync[j], tl_config.layer_sync);
      timestamp += 3000;
    }

    CodecSpecificInfo info;
    Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp);
    tl.OnEncodeDone(0, timestamp, kDefaultBytesPerFrame, true, kDefaultQp,
                    &info);
    EXPECT_TRUE(info.codecSpecific.VP8.layerSync)
        << "Key frame should be marked layer sync.";
    EXPECT_EQ(0, info.codecSpecific.VP8.temporalIdx)
        << "Key frame should always be packetized as layer 0";
    EXPECT_EQ(0, info.generic_frame_info->temporal_id)
        << "Key frame should always be packetized as layer 0";
    EXPECT_THAT(info.generic_frame_info->decode_target_indications,
                Each(DecodeTargetIndication::kSwitch))
        << "Key frame is universal switch";
    EXPECT_TRUE(checker.CheckTemporalConfig(true, tl_config));
  }
}

TEST_F(TemporalLayersTest, SetsTlCountOnFirstConfigUpdate) {

  constexpr int kNumLayers = 2;
  DefaultTemporalLayers tl(kNumLayers);
  Vp8EncoderConfig config = tl.UpdateConfiguration(0);

  ASSERT_TRUE(config.temporal_layer_config.has_value());
  EXPECT_EQ(config.temporal_layer_config->ts_number_layers,
            uint32_t{kNumLayers});
  std::array<uint32_t, Vp8EncoderConfig::TemporalLayerConfig::kMaxLayers>
      kZeroRate = {};
  EXPECT_EQ(config.temporal_layer_config->ts_target_bitrate, kZeroRate);

  config = tl.UpdateConfiguration(0);
  EXPECT_FALSE(config.temporal_layer_config.has_value());
}

class TemporalLayersReferenceTest : public TemporalLayersTest,
                                    public ::testing::WithParamInterface<int> {
 public:
  TemporalLayersReferenceTest()
      : timestamp_(1),
        last_sync_timestamp_(timestamp_),
        tl0_reference_(nullptr) {}
  virtual ~TemporalLayersReferenceTest() {}

 protected:
  static const int kMaxPatternLength = 32;

  struct BufferState {
    BufferState() : BufferState(-1, 0, false) {}
    BufferState(int temporal_idx, uint32_t timestamp, bool sync)
        : temporal_idx(temporal_idx), timestamp(timestamp), sync(sync) {}
    int temporal_idx;
    uint32_t timestamp;
    bool sync;
  };

  bool UpdateSyncRefState(const BufferFlags& flags, BufferState* buffer_state) {
    if (flags & BufferFlags::kReference) {
      if (buffer_state->temporal_idx == -1)
        return true;
      if (buffer_state->temporal_idx == 0) {

        EXPECT_EQ(nullptr, tl0_reference_);
        tl0_reference_ = buffer_state;
        return true;
      }
      return false;
    }
    return true;
  }

  void ValidateReference(const BufferFlags& flags,
                         const BufferState& buffer_state,
                         int temporal_layer) {
    if (flags & BufferFlags::kReference) {
      if (temporal_layer > 0 && buffer_state.timestamp > 0) {

        EXPECT_GE(buffer_state.timestamp, last_sync_timestamp_);
      }

      EXPECT_LE(buffer_state.temporal_idx, temporal_layer);
    }
  }

  uint32_t timestamp_ = 1;
  uint32_t last_sync_timestamp_ = timestamp_;
  BufferState* tl0_reference_;

  BufferState last_state;
  BufferState golden_state;
  BufferState altref_state;
};

INSTANTIATE_TEST_SUITE_P(DefaultTemporalLayersTest,
                         TemporalLayersReferenceTest,
                         ::testing::Range(1, kMaxTemporalStreams + 1));

TEST_P(TemporalLayersReferenceTest, ValidFrameConfigs) {
  const int num_layers = GetParam();
  DefaultTemporalLayers tl(num_layers);
  tl.OnRatesUpdated(
      0, GetTemporalLayerRates(kDefaultBytesPerFrame, kDefaultFramerate, 1),
      kDefaultFramerate);
  tl.UpdateConfiguration(0);

  std::vector<Vp8FrameConfig> tl_configs(kMaxPatternLength);
  for (int i = 0; i < kMaxPatternLength; ++i) {
    Vp8FrameConfig tl_config = tl.NextFrameConfig(0, timestamp_);
    tl.OnEncodeDone(0, timestamp_, kDefaultBytesPerFrame, i == 0, kDefaultQp,
                    IgnoredCodecSpecificInfo());
    ++timestamp_;
    EXPECT_FALSE(tl_config.drop_frame);
    tl_configs.push_back(tl_config);
    int temporal_idx = tl_config.encoder_layer_id;

    EXPECT_EQ(tl_config.packetizer_temporal_idx, temporal_idx);

    bool is_sync_frame = true;
    tl0_reference_ = nullptr;
    if (temporal_idx <= 0) {
      is_sync_frame = false;
    } else if (!UpdateSyncRefState(tl_config.last_buffer_flags, &last_state)) {
      is_sync_frame = false;
    } else if (!UpdateSyncRefState(tl_config.golden_buffer_flags,
                                   &golden_state)) {
      is_sync_frame = false;
    } else if (!UpdateSyncRefState(tl_config.arf_buffer_flags, &altref_state)) {
      is_sync_frame = false;
    }
    if (is_sync_frame) {

      ASSERT_TRUE(tl0_reference_);
      last_sync_timestamp_ = tl0_reference_->timestamp;
    }
    EXPECT_EQ(tl_config.layer_sync, is_sync_frame);

    ValidateReference(tl_config.last_buffer_flags, last_state, temporal_idx);
    ValidateReference(tl_config.golden_buffer_flags, golden_state,
                      temporal_idx);
    ValidateReference(tl_config.arf_buffer_flags, altref_state, temporal_idx);

    BufferState state = {temporal_idx, timestamp_, is_sync_frame};
    if (tl_config.last_buffer_flags & BufferFlags::kUpdate)
      last_state = state;
    if (tl_config.golden_buffer_flags & BufferFlags::kUpdate)
      golden_state = state;
    if (tl_config.arf_buffer_flags & BufferFlags::kUpdate)
      altref_state = state;
  }
}
}
}
