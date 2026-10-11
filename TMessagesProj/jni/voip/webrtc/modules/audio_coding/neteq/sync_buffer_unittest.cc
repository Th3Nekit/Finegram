/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/sync_buffer.h"

#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

TEST(SyncBuffer, CreateAndDestroy) {

  static const size_t kLen = 10;
  static const size_t kChannels = 2;
  SyncBuffer sync_buffer(kChannels, kLen);
  EXPECT_EQ(kChannels, sync_buffer.Channels());
  EXPECT_EQ(kLen, sync_buffer.Size());

  EXPECT_EQ(kLen, sync_buffer.next_index());

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kLen; ++i) {
      EXPECT_EQ(0, sync_buffer[channel][i]);
    }
  }
}

TEST(SyncBuffer, SetNextIndex) {

  static const size_t kLen = 100;
  static const size_t kChannels = 2;
  SyncBuffer sync_buffer(kChannels, kLen);
  sync_buffer.set_next_index(0);
  EXPECT_EQ(0u, sync_buffer.next_index());
  sync_buffer.set_next_index(kLen / 2);
  EXPECT_EQ(kLen / 2, sync_buffer.next_index());
  sync_buffer.set_next_index(kLen);
  EXPECT_EQ(kLen, sync_buffer.next_index());

  sync_buffer.set_next_index(kLen + 1);
  EXPECT_EQ(kLen, sync_buffer.next_index());
}

TEST(SyncBuffer, PushBackAndFlush) {

  static const size_t kLen = 100;
  static const size_t kChannels = 2;
  SyncBuffer sync_buffer(kChannels, kLen);
  static const size_t kNewLen = 10;
  AudioMultiVector new_data(kChannels, kNewLen);

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kNewLen; ++i) {
      new_data[channel][i] = rtc::checked_cast<int16_t>(i);
    }
  }

  sync_buffer.PushBack(new_data);
  ASSERT_EQ(kLen, sync_buffer.Size());

  EXPECT_EQ(kLen - kNewLen, sync_buffer.next_index());

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kNewLen; ++i) {
      EXPECT_EQ(new_data[channel][i],
                sync_buffer[channel][sync_buffer.next_index() + i]);
    }
  }

  sync_buffer.Flush();
  ASSERT_EQ(kLen, sync_buffer.Size());
  EXPECT_EQ(kLen, sync_buffer.next_index());
  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kLen; ++i) {
      EXPECT_EQ(0, sync_buffer[channel][i]);
    }
  }
}

TEST(SyncBuffer, PushFrontZeros) {

  static const size_t kLen = 100;
  static const size_t kChannels = 2;
  SyncBuffer sync_buffer(kChannels, kLen);
  static const size_t kNewLen = 10;
  AudioMultiVector new_data(kChannels, kNewLen);

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kNewLen; ++i) {
      new_data[channel][i] = rtc::checked_cast<int16_t>(1000 + i);
    }
  }
  sync_buffer.PushBack(new_data);
  EXPECT_EQ(kLen, sync_buffer.Size());

  sync_buffer.PushFrontZeros(kNewLen - 1);
  EXPECT_EQ(kLen, sync_buffer.Size());

  EXPECT_EQ(kLen - 1, sync_buffer.next_index());

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kNewLen - 1; ++i) {
      EXPECT_EQ(0, sync_buffer[channel][i]);
    }
  }

  for (size_t channel = 0; channel < kChannels; ++channel) {
    EXPECT_EQ(1000, sync_buffer[channel][sync_buffer.next_index()]);
  }
}

TEST(SyncBuffer, GetNextAudioInterleaved) {

  static const size_t kLen = 100;
  static const size_t kChannels = 2;
  SyncBuffer sync_buffer(kChannels, kLen);
  static const size_t kNewLen = 10;
  AudioMultiVector new_data(kChannels, kNewLen);

  for (size_t channel = 0; channel < kChannels; ++channel) {
    for (size_t i = 0; i < kNewLen; ++i) {
      new_data[channel][i] = rtc::checked_cast<int16_t>(i);
    }
  }

  sync_buffer.PushBack(new_data);

  AudioFrame output1;
  sync_buffer.GetNextAudioInterleaved(kNewLen / 2, &output1);
  EXPECT_EQ(kChannels, output1.num_channels_);
  EXPECT_EQ(kNewLen / 2, output1.samples_per_channel_);

  AudioFrame output2;
  sync_buffer.GetNextAudioInterleaved(kNewLen / 2, &output2);
  EXPECT_EQ(kChannels, output2.num_channels_);
  EXPECT_EQ(kNewLen / 2, output2.samples_per_channel_);

  const int16_t* output_ptr = output1.data();
  for (size_t i = 0; i < kNewLen / 2; ++i) {
    for (size_t channel = 0; channel < kChannels; ++channel) {
      EXPECT_EQ(new_data[channel][i], *output_ptr);
      ++output_ptr;
    }
  }
  output_ptr = output2.data();
  for (size_t i = kNewLen / 2; i < kNewLen; ++i) {
    for (size_t channel = 0; channel < kChannels; ++channel) {
      EXPECT_EQ(new_data[channel][i], *output_ptr);
      ++output_ptr;
    }
  }
}

}
