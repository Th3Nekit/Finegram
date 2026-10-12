/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/audio_multi_vector.h"

#include <stdlib.h>

#include <string>
#include <vector>

#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

class AudioMultiVectorTest : public ::testing::TestWithParam<size_t> {
 protected:
  AudioMultiVectorTest()
      : num_channels_(GetParam()),
        array_interleaved_(num_channels_ * array_length()) {}

  ~AudioMultiVectorTest() = default;

  virtual void SetUp() {

    for (size_t i = 0; i < array_length(); ++i) {
      array_[i] = static_cast<int16_t>(i);
    }
    int16_t* ptr = array_interleaved_.data();

    for (size_t i = 0; i < array_length(); ++i) {
      for (size_t j = 1; j <= num_channels_; ++j) {
        *ptr = rtc::checked_cast<int16_t>(j * 100 + i);
        ++ptr;
      }
    }
  }

  size_t array_length() const { return sizeof(array_) / sizeof(array_[0]); }

  const size_t num_channels_;
  int16_t array_[10];
  std::vector<int16_t> array_interleaved_;
};

TEST_P(AudioMultiVectorTest, CreateAndDestroy) {
  AudioMultiVector vec1(num_channels_);
  EXPECT_TRUE(vec1.Empty());
  EXPECT_EQ(num_channels_, vec1.Channels());
  EXPECT_EQ(0u, vec1.Size());

  size_t initial_size = 17;
  AudioMultiVector vec2(num_channels_, initial_size);
  EXPECT_FALSE(vec2.Empty());
  EXPECT_EQ(num_channels_, vec2.Channels());
  EXPECT_EQ(initial_size, vec2.Size());
}

TEST_P(AudioMultiVectorTest, SubscriptOperator) {
  AudioMultiVector vec(num_channels_, array_length());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < array_length(); ++i) {
      vec[channel][i] = static_cast<int16_t>(i);

      const AudioVector& audio_vec = vec[channel];
      EXPECT_EQ(static_cast<int16_t>(i), audio_vec[i]);
    }
  }
}

TEST_P(AudioMultiVectorTest, PushBackInterleavedAndCopy) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);
  AudioMultiVector vec_copy(num_channels_);
  vec.CopyTo(&vec_copy);
  ASSERT_EQ(num_channels_, vec.Channels());
  ASSERT_EQ(array_length(), vec.Size());
  ASSERT_EQ(num_channels_, vec_copy.Channels());
  ASSERT_EQ(array_length(), vec_copy.Size());
  for (size_t channel = 0; channel < vec.Channels(); ++channel) {
    for (size_t i = 0; i < array_length(); ++i) {
      EXPECT_EQ(static_cast<int16_t>((channel + 1) * 100 + i), vec[channel][i]);
      EXPECT_EQ(vec[channel][i], vec_copy[channel][i]);
    }
  }

  vec.Clear();
  EXPECT_TRUE(vec.Empty());

  vec.CopyTo(&vec_copy);
  EXPECT_TRUE(vec_copy.Empty());
}

TEST_P(AudioMultiVectorTest, CopyToNull) {
  AudioMultiVector vec(num_channels_);
  AudioMultiVector* vec_copy = NULL;
  vec.PushBackInterleaved(array_interleaved_);
  vec.CopyTo(vec_copy);
}

TEST_P(AudioMultiVectorTest, PushBackVector) {
  AudioMultiVector vec1(num_channels_, array_length());
  AudioMultiVector vec2(num_channels_, array_length());

  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < array_length(); ++i) {
      vec1[channel][i] = static_cast<int16_t>(i + 100 * channel);
      vec2[channel][i] =
          static_cast<int16_t>(i + 100 * channel + array_length());
    }
  }

  vec1.PushBack(vec2);
  ASSERT_EQ(2u * array_length(), vec1.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2 * array_length(); ++i) {
      EXPECT_EQ(static_cast<int16_t>(i + 100 * channel), vec1[channel][i]);
    }
  }
}

TEST_P(AudioMultiVectorTest, PushBackFromIndex) {
  AudioMultiVector vec1(num_channels_);
  vec1.PushBackInterleaved(array_interleaved_);
  AudioMultiVector vec2(num_channels_);

  vec2.PushBackFromIndex(vec1, array_length() - 2);
  ASSERT_EQ(2u, vec2.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2; ++i) {
      EXPECT_EQ(array_interleaved_[channel +
                                   num_channels_ * (array_length() - 2 + i)],
                vec2[channel][i]);
    }
  }
}

TEST_P(AudioMultiVectorTest, Zeros) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);
  vec.Zeros(2 * array_length());
  ASSERT_EQ(num_channels_, vec.Channels());
  ASSERT_EQ(2u * array_length(), vec.Size());
  for (size_t channel = 0; channel < num_channels_; ++channel) {
    for (size_t i = 0; i < 2 * array_length(); ++i) {
      EXPECT_EQ(0, vec[channel][i]);
    }
  }
}

TEST_P(AudioMultiVectorTest, ReadInterleaved) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);
  int16_t* output = new int16_t[array_interleaved_.size()];

  size_t read_samples = 5;
  EXPECT_EQ(num_channels_ * read_samples,
            vec.ReadInterleaved(read_samples, output));
  EXPECT_EQ(0, memcmp(array_interleaved_.data(), output,
                      read_samples * sizeof(int16_t)));

  EXPECT_EQ(array_interleaved_.size(),
            vec.ReadInterleaved(array_length() + 1, output));
  EXPECT_EQ(0, memcmp(array_interleaved_.data(), output,
                      read_samples * sizeof(int16_t)));

  delete[] output;
}

TEST_P(AudioMultiVectorTest, PopFront) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);
  vec.PopFront(1);
  ASSERT_EQ(array_length() - 1u, vec.Size());

  int16_t* ptr = &array_interleaved_[num_channels_];
  for (size_t i = 0; i < array_length() - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      EXPECT_EQ(*ptr, vec[channel][i]);
      ++ptr;
    }
  }
  vec.PopFront(array_length());
  EXPECT_EQ(0u, vec.Size());
}

TEST_P(AudioMultiVectorTest, PopBack) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);
  vec.PopBack(1);
  ASSERT_EQ(array_length() - 1u, vec.Size());

  int16_t* ptr = array_interleaved_.data();
  for (size_t i = 0; i < array_length() - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      EXPECT_EQ(*ptr, vec[channel][i]);
      ++ptr;
    }
  }
  vec.PopBack(array_length());
  EXPECT_EQ(0u, vec.Size());
}

TEST_P(AudioMultiVectorTest, AssertSize) {
  AudioMultiVector vec(num_channels_, array_length());
  EXPECT_EQ(array_length(), vec.Size());

  vec.AssertSize(0);
  vec.AssertSize(array_length() - 1);

  EXPECT_EQ(array_length(), vec.Size());

  vec.AssertSize(array_length() + 1);

  EXPECT_EQ(array_length() + 1, vec.Size());

  for (size_t channel = 0; channel < vec.Channels(); ++channel) {
    EXPECT_EQ(array_length() + 1u, vec[channel].Size());
  }
}

TEST_P(AudioMultiVectorTest, OverwriteAt) {
  AudioMultiVector vec1(num_channels_);
  vec1.PushBackInterleaved(array_interleaved_);
  AudioMultiVector vec2(num_channels_);
  vec2.Zeros(3);

  vec1.OverwriteAt(vec2, 3, 5);

  ASSERT_EQ(array_length(), vec1.Size());
  int16_t* ptr = array_interleaved_.data();
  for (size_t i = 0; i < array_length() - 1; ++i) {
    for (size_t channel = 0; channel < num_channels_; ++channel) {
      if (i >= 5 && i <= 7) {

        EXPECT_EQ(0, vec1[channel][i]);
      } else {
        EXPECT_EQ(*ptr, vec1[channel][i]);
      }
      ++ptr;
    }
  }
}

TEST_P(AudioMultiVectorTest, CopyChannel) {
  if (num_channels_ < 2)
    return;

  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved(array_interleaved_);

  AudioMultiVector ref(num_channels_);
  ref.PushBack(vec);

  vec.CopyChannel(0, num_channels_ - 1);

  for (size_t i = 0; i < array_length(); ++i) {

    for (size_t channel = 0; channel < num_channels_ - 1; ++channel) {
      EXPECT_EQ(ref[channel][i], vec[channel][i]);
    }

    EXPECT_EQ(vec[0][i], vec[num_channels_ - 1][i]);
  }
}

TEST_P(AudioMultiVectorTest, PushBackEmptyArray) {
  AudioMultiVector vec(num_channels_);
  vec.PushBackInterleaved({});
  EXPECT_TRUE(vec.Empty());
}

INSTANTIATE_TEST_SUITE_P(TestNumChannels,
                         AudioMultiVectorTest,
                         ::testing::Values(static_cast<size_t>(1),
                                           static_cast<size_t>(2),
                                           static_cast<size_t>(5)));
}
