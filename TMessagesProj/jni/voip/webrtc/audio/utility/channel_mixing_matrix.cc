/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/utility/channel_mixing_matrix.h"

#include <stddef.h>

#include <algorithm>

#include "audio/utility/channel_mixer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

bool UseChannelMappingAdjustmentsByDefault() {
  return !field_trial::IsEnabled(
      "WebRTC-VoIPChannelRemixingAdjustmentKillSwitch");
}

}

static void ValidateLayout(ChannelLayout layout) {
  RTC_CHECK_NE(layout, CHANNEL_LAYOUT_NONE);
  RTC_CHECK_LE(layout, CHANNEL_LAYOUT_MAX);
  RTC_CHECK_NE(layout, CHANNEL_LAYOUT_UNSUPPORTED);
  RTC_CHECK_NE(layout, CHANNEL_LAYOUT_DISCRETE);
  RTC_CHECK_NE(layout, CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC);

  int channel_count = ChannelLayoutToChannelCount(layout);
  RTC_DCHECK_GT(channel_count, 0);

  if (channel_count > 1) {

    RTC_DCHECK_EQ(ChannelOrder(layout, LEFT) >= 0,
                  ChannelOrder(layout, RIGHT) >= 0);
    RTC_DCHECK_EQ(ChannelOrder(layout, SIDE_LEFT) >= 0,
                  ChannelOrder(layout, SIDE_RIGHT) >= 0);
    RTC_DCHECK_EQ(ChannelOrder(layout, BACK_LEFT) >= 0,
                  ChannelOrder(layout, BACK_RIGHT) >= 0);
    RTC_DCHECK_EQ(ChannelOrder(layout, LEFT_OF_CENTER) >= 0,
                  ChannelOrder(layout, RIGHT_OF_CENTER) >= 0);
  } else {
    RTC_DCHECK_EQ(layout, CHANNEL_LAYOUT_MONO);
  }
}

ChannelMixingMatrix::ChannelMixingMatrix(ChannelLayout input_layout,
                                         int input_channels,
                                         ChannelLayout output_layout,
                                         int output_channels)
    : use_voip_channel_mapping_adjustments_(
          UseChannelMappingAdjustmentsByDefault()),
      input_layout_(input_layout),
      input_channels_(input_channels),
      output_layout_(output_layout),
      output_channels_(output_channels) {

  RTC_CHECK_NE(output_layout, CHANNEL_LAYOUT_STEREO_DOWNMIX);

  if (input_layout != CHANNEL_LAYOUT_DISCRETE)
    ValidateLayout(input_layout);
  if (output_layout != CHANNEL_LAYOUT_DISCRETE)
    ValidateLayout(output_layout);

  if (input_layout_ == CHANNEL_LAYOUT_5_0_BACK &&
      output_layout_ == CHANNEL_LAYOUT_7_0) {
    input_layout_ = CHANNEL_LAYOUT_5_0;
  } else if (input_layout_ == CHANNEL_LAYOUT_5_1_BACK &&
             output_layout_ == CHANNEL_LAYOUT_7_1) {
    input_layout_ = CHANNEL_LAYOUT_5_1;
  }
}

ChannelMixingMatrix::~ChannelMixingMatrix() = default;

bool ChannelMixingMatrix::CreateTransformationMatrix(
    std::vector<std::vector<float>>* matrix) {
  matrix_ = matrix;

  matrix_->reserve(output_channels_);
  for (int output_ch = 0; output_ch < output_channels_; ++output_ch)
    matrix_->push_back(std::vector<float>(input_channels_, 0));

  if (input_layout_ == CHANNEL_LAYOUT_DISCRETE ||
      output_layout_ == CHANNEL_LAYOUT_DISCRETE) {

    int passthrough_channels = std::min(input_channels_, output_channels_);
    for (int i = 0; i < passthrough_channels; ++i)
      (*matrix_)[i][i] = 1;

    return true;
  }

  if (use_voip_channel_mapping_adjustments_ &&
      input_layout_ == CHANNEL_LAYOUT_MONO &&
      ChannelLayoutToChannelCount(output_layout_) >= 2) {

    (*matrix_)[0][0] = 1.f;
    (*matrix_)[1][0] = 1.f;

    for (size_t output_ch = 2; output_ch < matrix_->size(); ++output_ch) {
      (*matrix_)[output_ch][0] = 0.f;
    }
    return true;
  }

  for (Channels ch = LEFT; ch < CHANNELS_MAX + 1;
       ch = static_cast<Channels>(ch + 1)) {
    int input_ch_index = ChannelOrder(input_layout_, ch);
    if (input_ch_index < 0)
      continue;

    int output_ch_index = ChannelOrder(output_layout_, ch);
    if (output_ch_index < 0) {
      unaccounted_inputs_.push_back(ch);
      continue;
    }

    RTC_DCHECK_LT(static_cast<size_t>(output_ch_index), matrix_->size());
    RTC_DCHECK_LT(static_cast<size_t>(input_ch_index),
                  (*matrix_)[output_ch_index].size());
    (*matrix_)[output_ch_index][input_ch_index] = 1;
  }

  if (unaccounted_inputs_.empty()) {

    return true;
  }

  if (IsUnaccounted(LEFT)) {

    float scale =
        (output_layout_ == CHANNEL_LAYOUT_MONO && input_channels_ == 2)
            ? 0.5
            : ChannelMixer::kHalfPower;
    Mix(LEFT, CENTER, scale);
    Mix(RIGHT, CENTER, scale);
  }

  if (IsUnaccounted(CENTER)) {

    float scale =
        (input_layout_ == CHANNEL_LAYOUT_MONO) ? 1 : ChannelMixer::kHalfPower;
    MixWithoutAccounting(CENTER, LEFT, scale);
    Mix(CENTER, RIGHT, scale);
  }

  if (IsUnaccounted(BACK_LEFT)) {
    if (HasOutputChannel(SIDE_LEFT)) {

      float scale = HasInputChannel(SIDE_LEFT) ? ChannelMixer::kHalfPower : 1;
      Mix(BACK_LEFT, SIDE_LEFT, scale);
      Mix(BACK_RIGHT, SIDE_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {

      Mix(BACK_LEFT, BACK_CENTER, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, BACK_CENTER, ChannelMixer::kHalfPower);
    } else if (output_layout_ > CHANNEL_LAYOUT_MONO) {

      Mix(BACK_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {

      Mix(BACK_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(BACK_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  if (IsUnaccounted(SIDE_LEFT)) {
    if (HasOutputChannel(BACK_LEFT)) {

      float scale = HasInputChannel(BACK_LEFT) ? ChannelMixer::kHalfPower : 1;
      Mix(SIDE_LEFT, BACK_LEFT, scale);
      Mix(SIDE_RIGHT, BACK_RIGHT, scale);
    } else if (HasOutputChannel(BACK_CENTER)) {

      Mix(SIDE_LEFT, BACK_CENTER, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, BACK_CENTER, ChannelMixer::kHalfPower);
    } else if (output_layout_ > CHANNEL_LAYOUT_MONO) {

      Mix(SIDE_LEFT, LEFT, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, RIGHT, ChannelMixer::kHalfPower);
    } else {

      Mix(SIDE_LEFT, CENTER, ChannelMixer::kHalfPower);
      Mix(SIDE_RIGHT, CENTER, ChannelMixer::kHalfPower);
    }
  }

  if (IsUnaccounted(BACK_CENTER)) {
    if (HasOutputChannel(BACK_LEFT)) {

      MixWithoutAccounting(BACK_CENTER, BACK_LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, BACK_RIGHT, ChannelMixer::kHalfPower);
    } else if (HasOutputChannel(SIDE_LEFT)) {

      MixWithoutAccounting(BACK_CENTER, SIDE_LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, SIDE_RIGHT, ChannelMixer::kHalfPower);
    } else if (output_layout_ > CHANNEL_LAYOUT_MONO) {

      MixWithoutAccounting(BACK_CENTER, LEFT, ChannelMixer::kHalfPower);
      Mix(BACK_CENTER, RIGHT, ChannelMixer::kHalfPower);
    } else {

      Mix(BACK_CENTER, CENTER, ChannelMixer::kHalfPower);
    }
  }

  if (IsUnaccounted(LEFT_OF_CENTER)) {
    if (HasOutputChannel(LEFT)) {

      Mix(LEFT_OF_CENTER, LEFT, ChannelMixer::kHalfPower);
      Mix(RIGHT_OF_CENTER, RIGHT, ChannelMixer::kHalfPower);
    } else {

      Mix(LEFT_OF_CENTER, CENTER, ChannelMixer::kHalfPower);
      Mix(RIGHT_OF_CENTER, CENTER, ChannelMixer::kHalfPower);
    }
  }

  if (IsUnaccounted(LFE)) {
    if (!HasOutputChannel(CENTER)) {

      MixWithoutAccounting(LFE, LEFT, ChannelMixer::kHalfPower);
      Mix(LFE, RIGHT, ChannelMixer::kHalfPower);
    } else {

      Mix(LFE, CENTER, ChannelMixer::kHalfPower);
    }
  }

  RTC_DCHECK(unaccounted_inputs_.empty());

  for (int output_ch = 0; output_ch < output_channels_; ++output_ch) {
    int input_mappings = 0;
    for (int input_ch = 0; input_ch < input_channels_; ++input_ch) {

      if ((*matrix_)[output_ch][input_ch] != 1 || ++input_mappings > 1)
        return false;
    }
  }

  return true;
}

void ChannelMixingMatrix::AccountFor(Channels ch) {
  unaccounted_inputs_.erase(
      std::find(unaccounted_inputs_.begin(), unaccounted_inputs_.end(), ch));
}

bool ChannelMixingMatrix::IsUnaccounted(Channels ch) const {
  return std::find(unaccounted_inputs_.begin(), unaccounted_inputs_.end(),
                   ch) != unaccounted_inputs_.end();
}

bool ChannelMixingMatrix::HasInputChannel(Channels ch) const {
  return ChannelOrder(input_layout_, ch) >= 0;
}

bool ChannelMixingMatrix::HasOutputChannel(Channels ch) const {
  return ChannelOrder(output_layout_, ch) >= 0;
}

void ChannelMixingMatrix::Mix(Channels input_ch,
                              Channels output_ch,
                              float scale) {
  MixWithoutAccounting(input_ch, output_ch, scale);
  AccountFor(input_ch);
}

void ChannelMixingMatrix::MixWithoutAccounting(Channels input_ch,
                                               Channels output_ch,
                                               float scale) {
  int input_ch_index = ChannelOrder(input_layout_, input_ch);
  int output_ch_index = ChannelOrder(output_layout_, output_ch);

  RTC_DCHECK(IsUnaccounted(input_ch));
  RTC_DCHECK_GE(input_ch_index, 0);
  RTC_DCHECK_GE(output_ch_index, 0);

  RTC_DCHECK_EQ((*matrix_)[output_ch_index][input_ch_index], 0);
  (*matrix_)[output_ch_index][input_ch_index] = scale;
}

}
