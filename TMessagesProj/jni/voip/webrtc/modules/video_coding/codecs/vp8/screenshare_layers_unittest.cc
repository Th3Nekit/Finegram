/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/screenshare_layers.h"

#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "api/video_codecs/vp8_frame_config.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp8/libvpx_vp8_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "vpx/vp8cx.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::NiceMock;

namespace webrtc {
namespace {

const uint32_t kTimestampDelta5Fps = 90000 / 5;
const int kDefaultQp = 54;
const int kDefaultTl0BitrateKbps = 200;
const int kDefaultTl1BitrateKbps = 2000;
const int kFrameRate = 5;
const int kSyncPeriodSeconds = 2;
const int kMaxSyncPeriodSeconds = 4;

const int kTl0Flags = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
                      VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
const int kTl1Flags =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
const int kTl1SyncFlags = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_GF |
                          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
const std::vector<uint32_t> kDefault2TlBitratesBps = {
    kDefaultTl0BitrateKbps * 1000,
    (kDefaultTl1BitrateKbps - kDefaultTl0BitrateKbps) * 1000};

}

class ScreenshareLayerTest : public ::testing::Test {
 protected:
  ScreenshareLayerTest()
      : min_qp_(2),
        max_qp_(kDefaultQp),
        frame_size_(-1),
        timestamp_(90),
        config_updated_(false) {}
  virtual ~ScreenshareLayerTest() {}

  void SetUp() override {
    layers_.reset(new ScreenshareLayers(2));
    cfg_ = ConfigureBitrates();
  }

  int EncodeFrame(bool base_sync, CodecSpecificInfo* info = nullptr) {
    CodecSpecificInfo ignored_info;
    if (!info) {
      info = &ignored_info;
    }

    int flags = ConfigureFrame(base_sync);
    if (flags != -1)
      layers_->OnEncodeDone(0, timestamp_, frame_size_, base_sync, kDefaultQp,
                            info);
    return flags;
  }

  int ConfigureFrame(bool key_frame) {
    tl_config_ = NextFrameConfig(0, timestamp_);
    EXPECT_EQ(0, tl_config_.encoder_layer_id)
        << "ScreenshareLayers always encodes using the bitrate allocator for "
           "layer 0, but may reference different buffers and packetize "
           "differently.";
    if (tl_config_.drop_frame) {
      return -1;
    }
    const uint32_t prev_rc_target_bitrate = cfg_.rc_target_bitrate.value_or(-1);
    const uint32_t prev_rc_max_quantizer = cfg_.rc_max_quantizer.value_or(-1);

    cfg_ = layers_->UpdateConfiguration(0);

    config_updated_ =
        cfg_.temporal_layer_config.has_value() ||
        (cfg_.rc_target_bitrate.has_value() &&
         cfg_.rc_target_bitrate.value() != prev_rc_target_bitrate) ||
        (cfg_.rc_max_quantizer.has_value() &&
         cfg_.rc_max_quantizer.value() != prev_rc_max_quantizer) ||
        cfg_.g_error_resilient.has_value();

    int flags = LibvpxVp8Encoder::EncodeFlags(tl_config_);
    EXPECT_NE(-1, frame_size_);
    return flags;
  }

  Vp8FrameConfig NextFrameConfig(size_t stream_index, uint32_t timestamp) {
    int64_t timestamp_ms = timestamp / 90;
    clock_.AdvanceTime(TimeDelta::Millis(timestamp_ms - rtc::TimeMillis()));
    return layers_->NextFrameConfig(stream_index, timestamp);
  }

  int FrameSizeForBitrate(int bitrate_kbps) {
    return ((bitrate_kbps * 1000) / 8) / kFrameRate;
  }

  Vp8EncoderConfig ConfigureBitrates() {
    layers_->SetQpLimits(0, min_qp_, max_qp_);
    layers_->OnRatesUpdated(0, kDefault2TlBitratesBps, kFrameRate);
    const Vp8EncoderConfig vp8_cfg = layers_->UpdateConfiguration(0);
    EXPECT_TRUE(vp8_cfg.rc_target_bitrate.has_value());
    frame_size_ = FrameSizeForBitrate(vp8_cfg.rc_target_bitrate.value());
    return vp8_cfg;
  }

  void WithQpLimits(int min_qp, int max_qp) {
    min_qp_ = min_qp;
    max_qp_ = max_qp;
  }

  bool RunGracePeriod() {
    bool got_tl0 = false;
    bool got_tl1 = false;
    bool got_tl1_sync = false;
    for (int i = 0; i < 10; ++i) {
      CodecSpecificInfo info;
      EXPECT_NE(-1, EncodeFrame(false, &info));
      timestamp_ += kTimestampDelta5Fps;
      if (info.codecSpecific.VP8.temporalIdx == 0) {
        got_tl0 = true;
      } else if (info.codecSpecific.VP8.layerSync) {
        got_tl1_sync = true;
      } else {
        got_tl1 = true;
      }
      if (got_tl0 && got_tl1 && got_tl1_sync)
        return true;
    }
    return false;
  }

  int SkipUntilTl(int layer) {
    return SkipUntilTlAndSync(layer, absl::nullopt);
  }

  int SkipUntilTlAndSync(int layer, absl::optional<bool> sync) {
    int flags = 0;
    const int kMaxFramesToSkip =
        1 + (sync.value_or(false) ? kMaxSyncPeriodSeconds : 1) * kFrameRate;
    for (int i = 0; i < kMaxFramesToSkip; ++i) {
      flags = ConfigureFrame(false);
      if (tl_config_.packetizer_temporal_idx != layer ||
          (sync && *sync != tl_config_.layer_sync)) {
        if (flags != -1) {

          CodecSpecificInfo info;
          layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                                &info);
        }
        timestamp_ += kTimestampDelta5Fps;
      } else {

        return flags;
      }
    }
    ADD_FAILURE() << "Did not get a frame of TL" << layer << " in time.";
    return -1;
  }

  int min_qp_;
  uint32_t max_qp_;
  int frame_size_;
  rtc::ScopedFakeClock clock_;
  std::unique_ptr<ScreenshareLayers> layers_;

  uint32_t timestamp_;
  Vp8FrameConfig tl_config_;
  Vp8EncoderConfig cfg_;
  bool config_updated_;

  CodecSpecificInfo* IgnoredCodecSpecificInfo() {
    ignored_codec_specific_info_ = std::make_unique<CodecSpecificInfo>();
    return ignored_codec_specific_info_.get();
  }

 private:
  std::unique_ptr<CodecSpecificInfo> ignored_codec_specific_info_;
};

TEST_F(ScreenshareLayerTest, 1Layer) {
  layers_.reset(new ScreenshareLayers(1));
  ConfigureBitrates();

  const int kSingleLayerFlags = 0;
  auto info = std::make_unique<CodecSpecificInfo>();
  int flags = EncodeFrame(              false, info.get());
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx),
            info->codecSpecific.VP8.temporalIdx);
  EXPECT_FALSE(info->codecSpecific.VP8.layerSync);
  EXPECT_EQ(info->generic_frame_info->temporal_id, 0);

  info = std::make_unique<CodecSpecificInfo>();
  flags = EncodeFrame(              false, info.get());
  EXPECT_EQ(kSingleLayerFlags, flags);
  EXPECT_EQ(static_cast<uint8_t>(kNoTemporalIdx),
            info->codecSpecific.VP8.temporalIdx);
  EXPECT_FALSE(info->codecSpecific.VP8.layerSync);
  EXPECT_EQ(info->generic_frame_info->temporal_id, 0);
}

TEST_F(ScreenshareLayerTest, 2LayersPeriodicSync) {
  std::vector<int> sync_times;
  const int kNumFrames = kSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    CodecSpecificInfo info;
    EncodeFrame(false, &info);
    timestamp_ += kTimestampDelta5Fps;
    if (info.codecSpecific.VP8.temporalIdx == 1 &&
        info.codecSpecific.VP8.layerSync) {
      sync_times.push_back(timestamp_);
    }
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterTimeout) {
  std::vector<int> sync_times;
  const int kNumFrames = kMaxSyncPeriodSeconds * kFrameRate * 2 - 1;
  for (int i = 0; i < kNumFrames; ++i) {
    CodecSpecificInfo info;

    tl_config_ = NextFrameConfig(0, timestamp_);
    cfg_ = layers_->UpdateConfiguration(0);

    if (tl_config_.packetizer_temporal_idx == 0) {
      layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                            &info);
    } else {
      layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp - 8,
                            &info);
    }

    if (info.codecSpecific.VP8.temporalIdx == 1 &&
        info.codecSpecific.VP8.layerSync)
      sync_times.push_back(timestamp_);

    timestamp_ += kTimestampDelta5Fps;
  }

  ASSERT_EQ(2u, sync_times.size());
  EXPECT_GE(sync_times[1] - sync_times[0], 90000 * kMaxSyncPeriodSeconds);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAfterSimilarQP) {
  std::vector<int> sync_times;

  const int kNumFrames = (kSyncPeriodSeconds +
                          ((kMaxSyncPeriodSeconds - kSyncPeriodSeconds) / 2)) *
                         kFrameRate;
  for (int i = 0; i < kNumFrames; ++i) {
    CodecSpecificInfo info;

    ConfigureFrame(false);

    if (tl_config_.packetizer_temporal_idx == 0) {
      layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                            &info);
    } else {
      layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp - 8,
                            &info);
    }

    if (info.codecSpecific.VP8.temporalIdx == 1 &&
        info.codecSpecific.VP8.layerSync)
      sync_times.push_back(timestamp_);

    timestamp_ += kTimestampDelta5Fps;
  }

  ASSERT_EQ(1u, sync_times.size());

  bool bumped_tl0_quality = false;
  for (int i = 0; i < 3; ++i) {
    CodecSpecificInfo info;

    int flags = ConfigureFrame(false);
    layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp - 8,
                          &info);
    if (info.codecSpecific.VP8.temporalIdx == 0) {

      bumped_tl0_quality = true;
    } else {
      if (bumped_tl0_quality) {
        EXPECT_TRUE(info.codecSpecific.VP8.layerSync);
        EXPECT_EQ(kTl1SyncFlags, flags);
        return;
      }
    }
    timestamp_ += kTimestampDelta5Fps;
  }
  ADD_FAILURE() << "No TL1 frame arrived within time limit.";
}

TEST_F(ScreenshareLayerTest, 2LayersToggling) {
  EXPECT_TRUE(RunGracePeriod());

  int tl0_frames = 0;
  int tl1_frames = 0;
  for (int i = 0; i < 50; ++i) {
    CodecSpecificInfo info;
    EncodeFrame(              false, &info);
    EXPECT_EQ(info.codecSpecific.VP8.temporalIdx,
              info.generic_frame_info->temporal_id);
    timestamp_ += kTimestampDelta5Fps;
    switch (info.codecSpecific.VP8.temporalIdx) {
      case 0:
        ++tl0_frames;
        break;
      case 1:
        ++tl1_frames;
        break;
      default:
        abort();
    }
  }
  EXPECT_EQ(20, tl0_frames);
  EXPECT_EQ(30, tl1_frames);
}

TEST_F(ScreenshareLayerTest, AllFitsLayer0) {
  frame_size_ = FrameSizeForBitrate(kDefaultTl0BitrateKbps);

  for (int i = 0; i < 50; ++i) {
    CodecSpecificInfo info;
    int flags = EncodeFrame(false, &info);
    timestamp_ += kTimestampDelta5Fps;
    EXPECT_EQ(kTl0Flags, flags);
    EXPECT_EQ(0, info.codecSpecific.VP8.temporalIdx);
  }
}

TEST_F(ScreenshareLayerTest, TooHighBitrate) {
  frame_size_ = 2 * FrameSizeForBitrate(kDefaultTl1BitrateKbps);

  int tl0_frames = 0;
  int tl1_frames = 0;
  int dropped_frames = 0;
  for (int i = 0; i < 100; ++i) {
    CodecSpecificInfo info;
    int flags = EncodeFrame(false, &info);
    timestamp_ += kTimestampDelta5Fps;
    if (flags == -1) {
      ++dropped_frames;
    } else {
      switch (info.codecSpecific.VP8.temporalIdx) {
        case 0:
          ++tl0_frames;
          break;
        case 1:
          ++tl1_frames;
          break;
        default:
          ADD_FAILURE() << "Unexpected temporal id";
      }
    }
  }

  EXPECT_NEAR(50, tl0_frames + tl1_frames, 1);
  EXPECT_NEAR(50, dropped_frames, 1);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL0) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 1000;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000,
                                             (kTl1_kbps - kTl0_kbps) * 1000};
  layers_->OnRatesUpdated(0, layer_rates, kFrameRate);
  cfg_ = layers_->UpdateConfiguration(0);

  EXPECT_EQ(static_cast<unsigned int>(
                ScreenshareLayers::kMaxTL0FpsReduction * kTl0_kbps + 0.5),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateCappedByTL1) {
  const int kTl0_kbps = 100;
  const int kTl1_kbps = 450;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000,
                                             (kTl1_kbps - kTl0_kbps) * 1000};
  layers_->OnRatesUpdated(0, layer_rates, kFrameRate);
  cfg_ = layers_->UpdateConfiguration(0);

  EXPECT_EQ(static_cast<unsigned int>(
                kTl1_kbps / ScreenshareLayers::kAcceptableTargetOvershoot),
            cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, TargetBitrateBelowTL0) {
  const int kTl0_kbps = 100;
  const std::vector<uint32_t> layer_rates = {kTl0_kbps * 1000};
  layers_->OnRatesUpdated(0, layer_rates, kFrameRate);
  cfg_ = layers_->UpdateConfiguration(0);

  EXPECT_EQ(static_cast<uint32_t>(kTl0_kbps), cfg_.rc_target_bitrate);
}

TEST_F(ScreenshareLayerTest, EncoderDrop) {
  EXPECT_TRUE(RunGracePeriod());
  SkipUntilTl(0);

  layers_->OnEncodeDone(0, timestamp_, 0, false, 0, IgnoredCodecSpecificInfo());

  int flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl0Flags, flags);

  SkipUntilTl(0);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        IgnoredCodecSpecificInfo());
  timestamp_ += kTimestampDelta5Fps;

  SkipUntilTl(0);
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        IgnoredCodecSpecificInfo());
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));

  SkipUntilTl(1);
  layers_->OnEncodeDone(0, timestamp_, 0, false, 0, IgnoredCodecSpecificInfo());

  flags = EncodeFrame(false);
  timestamp_ += kTimestampDelta5Fps;
  EXPECT_FALSE(config_updated_);
  EXPECT_EQ(kTl1Flags, flags);

  SkipUntilTl(1);
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        IgnoredCodecSpecificInfo());
  timestamp_ += kTimestampDelta5Fps;

  SkipUntilTl(1);
  EXPECT_EQ(cfg_.rc_max_quantizer, static_cast<unsigned int>(kDefaultQp));
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        IgnoredCodecSpecificInfo());
  timestamp_ += kTimestampDelta5Fps;
}

TEST_F(ScreenshareLayerTest, RespectsMaxIntervalBetweenFrames) {
  const int kLowBitrateKbps = 50;
  const int kLargeFrameSizeBytes = 100000;
  const uint32_t kStartTimestamp = 1234;

  const std::vector<uint32_t> layer_rates = {kLowBitrateKbps * 1000};
  layers_->OnRatesUpdated(0, layer_rates, kFrameRate);
  cfg_ = layers_->UpdateConfiguration(0);

  EXPECT_EQ(kTl0Flags,
            LibvpxVp8Encoder::EncodeFlags(NextFrameConfig(0, kStartTimestamp)));
  layers_->OnEncodeDone(0, kStartTimestamp, kLargeFrameSizeBytes, false,
                        kDefaultQp, IgnoredCodecSpecificInfo());

  const uint32_t kTwoSecondsLater =
      kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90);

  ASSERT_GT(kStartTimestamp + 90 * (kLargeFrameSizeBytes * 8) / kLowBitrateKbps,
            kStartTimestamp + (ScreenshareLayers::kMaxFrameIntervalMs * 90));

  EXPECT_TRUE(
      NextFrameConfig(0, kTwoSecondsLater - kTimestampDelta5Fps).drop_frame);

  EXPECT_EQ(kTl0Flags, LibvpxVp8Encoder::EncodeFlags(
                           NextFrameConfig(0, kTwoSecondsLater + 90)));
}

TEST_F(ScreenshareLayerTest, UpdatesHistograms) {
  metrics::Reset();
  bool trigger_drop = false;
  bool dropped_frame = false;
  bool overshoot = false;
  const int kTl0Qp = 35;
  const int kTl1Qp = 30;
  for (int64_t timestamp = 0;
       timestamp < kTimestampDelta5Fps * 5 * metrics::kMinRunTimeInSeconds;
       timestamp += kTimestampDelta5Fps) {
    tl_config_ = NextFrameConfig(0, timestamp);
    if (tl_config_.drop_frame) {
      dropped_frame = true;
      continue;
    }
    int flags = LibvpxVp8Encoder::EncodeFlags(tl_config_);
    if (flags != -1)
      cfg_ = layers_->UpdateConfiguration(0);

    if (timestamp >= kTimestampDelta5Fps * 5 && !overshoot && flags != -1) {

      layers_->OnEncodeDone(0, timestamp, 0, false, 0, nullptr);
      overshoot = true;
    }

    if (flags == kTl0Flags) {
      if (timestamp >= kTimestampDelta5Fps * 20 && !trigger_drop) {

        layers_->OnEncodeDone(0, timestamp, frame_size_ * 10, false, kTl0Qp,
                              IgnoredCodecSpecificInfo());
        trigger_drop = true;
      } else {
        layers_->OnEncodeDone(0, timestamp, frame_size_, false, kTl0Qp,
                              IgnoredCodecSpecificInfo());
      }
    } else if (flags == kTl1Flags || flags == kTl1SyncFlags) {
      layers_->OnEncodeDone(0, timestamp, frame_size_, false, kTl1Qp,
                            IgnoredCodecSpecificInfo());
    } else if (flags == -1) {
      dropped_frame = true;
    } else {
      RTC_DCHECK_NOTREACHED() << "Unexpected flags";
    }
    clock_.AdvanceTime(TimeDelta::Millis(1000 / 5));
  }

  EXPECT_TRUE(overshoot);
  EXPECT_TRUE(dropped_frame);

  layers_.reset();

  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.FrameRate"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.FrameRate"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.FramesPerDrop"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.FramesPerOvershoot"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.Qp"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.Qp"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer0.TargetBitrate"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.Layer1.TargetBitrate"));

  EXPECT_METRIC_GT(
      metrics::MinSample("WebRTC.Video.Screenshare.Layer0.FrameRate"), 1);
  EXPECT_METRIC_GT(
      metrics::MinSample("WebRTC.Video.Screenshare.Layer1.FrameRate"), 1);
  EXPECT_METRIC_GT(metrics::MinSample("WebRTC.Video.Screenshare.FramesPerDrop"),
                   1);
  EXPECT_METRIC_GT(
      metrics::MinSample("WebRTC.Video.Screenshare.FramesPerOvershoot"), 1);
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.Screenshare.Layer0.Qp", kTl0Qp));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.Screenshare.Layer1.Qp", kTl1Qp));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.Screenshare.Layer0.TargetBitrate",
                            kDefaultTl0BitrateKbps));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.Screenshare.Layer1.TargetBitrate",
                            kDefaultTl1BitrateKbps));
}

TEST_F(ScreenshareLayerTest, RespectsConfiguredFramerate) {
  int64_t kTestSpanMs = 2000;
  int64_t kFrameIntervalsMs = 1000 / kFrameRate;

  uint32_t timestamp = 1234;
  int num_input_frames = 0;
  int num_discarded_frames = 0;

  for (int64_t i = 0; i < kTestSpanMs; i += kFrameIntervalsMs) {
    if (NextFrameConfig(0, timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->OnEncodeDone(0, timestamp, frame_size_bytes, false, kDefaultQp,
                            IgnoredCodecSpecificInfo());
    }
    timestamp += kFrameIntervalsMs * 90;
    clock_.AdvanceTime(TimeDelta::Millis(kFrameIntervalsMs));

    ++num_input_frames;
  }
  EXPECT_EQ(0, num_discarded_frames);

  num_input_frames = 0;
  num_discarded_frames = 0;
  for (int64_t i = 0; i < kTestSpanMs; i += kFrameIntervalsMs / 2) {
    if (NextFrameConfig(0, timestamp).drop_frame) {
      ++num_discarded_frames;
    } else {
      size_t frame_size_bytes = kDefaultTl0BitrateKbps * kFrameIntervalsMs / 8;
      layers_->OnEncodeDone(0, timestamp, frame_size_bytes, false, kDefaultQp,
                            IgnoredCodecSpecificInfo());
    }
    timestamp += kFrameIntervalsMs * 90 / 2;
    clock_.AdvanceTime(TimeDelta::Millis(kFrameIntervalsMs));
    ++num_input_frames;
  }

  EXPECT_NEAR(num_discarded_frames, num_input_frames / 2, 2);
}

TEST_F(ScreenshareLayerTest, 2LayersSyncAtOvershootDrop) {

  EXPECT_TRUE(RunGracePeriod());

  EXPECT_EQ(kTl1SyncFlags, SkipUntilTlAndSync(1, true));
  ASSERT_TRUE(tl_config_.layer_sync);

  layers_->OnEncodeDone(0, timestamp_, 0, false, 0, nullptr);

  cfg_ = layers_->UpdateConfiguration(0);
  EXPECT_EQ(kTl1SyncFlags, LibvpxVp8Encoder::EncodeFlags(tl_config_));

  CodecSpecificInfo new_info;
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        &new_info);
  EXPECT_TRUE(new_info.codecSpecific.VP8.layerSync);
}

TEST_F(ScreenshareLayerTest, DropOnTooShortFrameInterval) {

  EXPECT_TRUE(RunGracePeriod());

  timestamp_ += kTimestampDelta5Fps * 3;
  EXPECT_FALSE(NextFrameConfig(0, timestamp_).drop_frame);
  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, kDefaultQp,
                        IgnoredCodecSpecificInfo());

  const int64_t kMinFrameInterval = (kTimestampDelta5Fps * 85) / 100;
  timestamp_ += kMinFrameInterval - 90;
  EXPECT_TRUE(NextFrameConfig(0, timestamp_).drop_frame);

  timestamp_ += 90;
  EXPECT_FALSE(NextFrameConfig(0, timestamp_).drop_frame);
}

TEST_F(ScreenshareLayerTest, AdjustsBitrateWhenDroppingFrames) {
  const uint32_t kTimestampDelta10Fps = kTimestampDelta5Fps / 2;
  const int kNumFrames = 30;
  ASSERT_TRUE(cfg_.rc_target_bitrate.has_value());
  const uint32_t default_bitrate = cfg_.rc_target_bitrate.value();
  layers_->OnRatesUpdated(0, kDefault2TlBitratesBps, 10);

  int num_dropped_frames = 0;
  for (int i = 0; i < kNumFrames; ++i) {
    if (EncodeFrame(false) == -1)
      ++num_dropped_frames;
    timestamp_ += kTimestampDelta10Fps;
  }
  cfg_ = layers_->UpdateConfiguration(0);

  EXPECT_EQ(num_dropped_frames, kNumFrames / 2);
  EXPECT_EQ(cfg_.rc_target_bitrate, default_bitrate * 2);
}

TEST_F(ScreenshareLayerTest, UpdatesConfigurationAfterRateChange) {

  layers_->OnRatesUpdated(0, kDefault2TlBitratesBps, kFrameRate);
  cfg_ = layers_->UpdateConfiguration(0);

  std::vector<uint32_t> bitrates = kDefault2TlBitratesBps;
  bitrates[1] -= 100000;
  layers_->OnRatesUpdated(0, bitrates, 5);
  cfg_ = layers_->UpdateConfiguration(0);

  bitrates[1] -= 100000;
  layers_->OnRatesUpdated(0, bitrates, 5);
  layers_->OnRatesUpdated(0, bitrates, 5);
  cfg_ = layers_->UpdateConfiguration(0);
}

TEST_F(ScreenshareLayerTest, MaxQpRestoredAfterDoubleDrop) {

  EXPECT_TRUE(RunGracePeriod());

  EXPECT_EQ(kTl1SyncFlags, SkipUntilTlAndSync(1, true));
  ASSERT_TRUE(tl_config_.layer_sync);

  layers_->OnEncodeDone(0, timestamp_, 0, false, -1, nullptr);

  layers_->OnEncodeDone(0, timestamp_, 1, false, max_qp_,
                        IgnoredCodecSpecificInfo());

  std::vector<uint32_t> kDefault2TlBitratesBpsAlt = kDefault2TlBitratesBps;
  kDefault2TlBitratesBpsAlt[1] += 4000;
  layers_->OnRatesUpdated(0, kDefault2TlBitratesBpsAlt, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, max_qp_);
  ASSERT_TRUE(cfg_.rc_max_quantizer.has_value());
  const uint32_t adjusted_qp = cfg_.rc_max_quantizer.value();

  layers_->OnEncodeDone(0, timestamp_, 0, false, -1, nullptr);

  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, max_qp_,
                        IgnoredCodecSpecificInfo());

  layers_->OnRatesUpdated(0, kDefault2TlBitratesBps, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_TRUE(config_updated_);
  EXPECT_LT(cfg_.rc_max_quantizer, max_qp_);
  EXPECT_EQ(adjusted_qp, cfg_.rc_max_quantizer);

  layers_->OnEncodeDone(0, timestamp_, frame_size_, false, max_qp_,
                        IgnoredCodecSpecificInfo());

  layers_->OnRatesUpdated(0, kDefault2TlBitratesBpsAlt, kFrameRate);
  EXPECT_EQ(kTl1Flags, SkipUntilTlAndSync(1, false));
  EXPECT_EQ(cfg_.rc_max_quantizer, max_qp_);
}

}
