/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/audio_vector.h"

#include <stdlib.h>

#include <string>

#include "rtc_base/numerics/safe_conversions.h"
#include "test/gtest.h"

namespace webrtc {

class AudioVectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {

    for (size_t i = 0; i < array_length(); ++i) {
      array_[i] = rtc::checked_cast<int16_t>(i);
    }
  }

  size_t array_length() const { return sizeof(array_) / sizeof(array_[0]); }

  int16_t array_[10];
};

TEST_F(AudioVectorTest, CreateAndDestroy) {
  AudioVector vec1;
  EXPECT_TRUE(vec1.Empty());
  EXPECT_EQ(0u, vec1.Size());

  size_t initial_size = 17;
  AudioVector vec2(initial_size);
  EXPECT_FALSE(vec2.Empty());
  EXPECT_EQ(initial_size, vec2.Size());
}

TEST_F(AudioVectorTest, SubscriptOperator) {
  AudioVector vec(array_length());
  for (size_t i = 0; i < array_length(); ++i) {
    vec[i] = static_cast<int16_t>(i);
    const int16_t& value = vec[i];
    EXPECT_EQ(static_cast<int16_t>(i), value);
  }
}

TEST_F(AudioVectorTest, PushBackAndCopy) {
  AudioVector vec;
  AudioVector vec_copy;
  vec.PushBack(array_, array_length());
  vec.CopyTo(&vec_copy);
  ASSERT_EQ(array_length(), vec.Size());
  ASSERT_EQ(array_length(), vec_copy.Size());
  for (size_t i = 0; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[i]);
    EXPECT_EQ(array_[i], vec_copy[i]);
  }

  vec.Clear();
  EXPECT_TRUE(vec.Empty());

  vec.CopyTo(&vec_copy);
  EXPECT_TRUE(vec_copy.Empty());
}

TEST_F(AudioVectorTest, PushBackVector) {
  static const size_t kLength = 10;
  AudioVector vec1(kLength);
  AudioVector vec2(kLength);

  for (size_t i = 0; i < kLength; ++i) {
    vec1[i] = static_cast<int16_t>(i);
    vec2[i] = static_cast<int16_t>(i + kLength);
  }

  vec1.PushBack(vec2);
  ASSERT_EQ(2 * kLength, vec1.Size());
  for (size_t i = 0; i < 2 * kLength; ++i) {
    EXPECT_EQ(static_cast<int16_t>(i), vec1[i]);
  }
}

TEST_F(AudioVectorTest, PushFront) {
  AudioVector vec;
  vec.PushFront(array_, array_length());
  ASSERT_EQ(array_length(), vec.Size());
  for (size_t i = 0; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[i]);
  }
}

TEST_F(AudioVectorTest, PushFrontVector) {
  static const size_t kLength = 10;
  AudioVector vec1(kLength);
  AudioVector vec2(kLength);

  for (size_t i = 0; i < kLength; ++i) {
    vec1[i] = static_cast<int16_t>(i);
    vec2[i] = static_cast<int16_t>(i + kLength);
  }

  vec2.PushFront(vec1);
  ASSERT_EQ(2 * kLength, vec2.Size());
  for (size_t i = 0; i < 2 * kLength; ++i) {
    EXPECT_EQ(static_cast<int16_t>(i), vec2[i]);
  }
}

TEST_F(AudioVectorTest, PopFront) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  vec.PopFront(1);
  EXPECT_EQ(array_length() - 1u, vec.Size());
  for (size_t i = 0; i < array_length() - 1; ++i) {
    EXPECT_EQ(static_cast<int16_t>(i + 1), vec[i]);
  }
  vec.PopFront(array_length());
  EXPECT_EQ(0u, vec.Size());
}

TEST_F(AudioVectorTest, PopBack) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  vec.PopBack(1);
  EXPECT_EQ(array_length() - 1u, vec.Size());
  for (size_t i = 0; i < array_length() - 1; ++i) {
    EXPECT_EQ(static_cast<int16_t>(i), vec[i]);
  }
  vec.PopBack(array_length());
  EXPECT_EQ(0u, vec.Size());
}

TEST_F(AudioVectorTest, Extend) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  vec.Extend(5);
  ASSERT_EQ(array_length() + 5u, vec.Size());

  for (size_t i = array_length(); i < array_length() + 5; ++i) {
    EXPECT_EQ(0, vec[i]);
  }
}

TEST_F(AudioVectorTest, InsertAt) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  int insert_position = 5;
  vec.InsertAt(new_array, kNewLength, insert_position);

  size_t pos = 0;
  for (int i = 0; i < insert_position; ++i) {
    EXPECT_EQ(array_[i], vec[pos]);
    ++pos;
  }
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }
  for (size_t i = insert_position; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[pos]);
    ++pos;
  }
}

TEST_F(AudioVectorTest, InsertZerosAt) {
  AudioVector vec;
  AudioVector vec_ref;
  vec.PushBack(array_, array_length());
  vec_ref.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int insert_position = 5;
  vec.InsertZerosAt(kNewLength, insert_position);
  int16_t new_array[kNewLength] = {0};
  vec_ref.InsertAt(new_array, kNewLength, insert_position);

  ASSERT_EQ(vec_ref.Size(), vec.Size());
  for (size_t i = 0; i < vec.Size(); ++i) {
    EXPECT_EQ(vec_ref[i], vec[i]);
  }
}

TEST_F(AudioVectorTest, InsertAtBeginning) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  int insert_position = 0;
  vec.InsertAt(new_array, kNewLength, insert_position);

  size_t pos = 0;
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }
  for (size_t i = insert_position; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[pos]);
    ++pos;
  }
}

TEST_F(AudioVectorTest, InsertAtEnd) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  int insert_position = rtc::checked_cast<int>(array_length());
  vec.InsertAt(new_array, kNewLength, insert_position);

  size_t pos = 0;
  for (size_t i = 0; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[pos]);
    ++pos;
  }
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }
}

TEST_F(AudioVectorTest, InsertBeyondEnd) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  int insert_position =
      rtc::checked_cast<int>(array_length() + 10);
  vec.InsertAt(new_array, kNewLength, insert_position);

  size_t pos = 0;
  for (size_t i = 0; i < array_length(); ++i) {
    EXPECT_EQ(array_[i], vec[pos]);
    ++pos;
  }
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }
}

TEST_F(AudioVectorTest, OverwriteAt) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  size_t insert_position = 2;
  vec.OverwriteAt(new_array, kNewLength, insert_position);

  size_t pos = 0;
  for (pos = 0; pos < insert_position; ++pos) {
    EXPECT_EQ(array_[pos], vec[pos]);
  }
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }
  for (; pos < array_length(); ++pos) {
    EXPECT_EQ(array_[pos], vec[pos]);
  }
}

TEST_F(AudioVectorTest, OverwriteBeyondEnd) {
  AudioVector vec;
  vec.PushBack(array_, array_length());
  static const int kNewLength = 5;
  int16_t new_array[kNewLength];

  for (int i = 0; i < kNewLength; ++i) {
    new_array[i] = 100 + i;
  }
  int insert_position = rtc::checked_cast<int>(array_length() - 2);
  vec.OverwriteAt(new_array, kNewLength, insert_position);
  ASSERT_EQ(array_length() - 2u + kNewLength, vec.Size());

  int pos = 0;
  for (pos = 0; pos < insert_position; ++pos) {
    EXPECT_EQ(array_[pos], vec[pos]);
  }
  for (int i = 0; i < kNewLength; ++i) {
    EXPECT_EQ(new_array[i], vec[pos]);
    ++pos;
  }

  EXPECT_EQ(vec.Size(), static_cast<size_t>(pos));
}

TEST_F(AudioVectorTest, CrossFade) {
  static const size_t kLength = 100;
  static const size_t kFadeLength = 10;
  AudioVector vec1(kLength);
  AudioVector vec2(kLength);

  for (size_t i = 0; i < kLength; ++i) {
    vec1[i] = 0;
    vec2[i] = 100;
  }
  vec1.CrossFade(vec2, kFadeLength);
  ASSERT_EQ(2 * kLength - kFadeLength, vec1.Size());

  for (size_t i = 0; i < kLength - kFadeLength; ++i) {
    EXPECT_EQ(0, vec1[i]);
  }

  for (size_t i = 0; i < kFadeLength; ++i) {
    EXPECT_NEAR((i + 1) * 100 / (kFadeLength + 1),
                vec1[kLength - kFadeLength + i], 1);
  }

  for (size_t i = kLength; i < vec1.Size(); ++i) {
    EXPECT_EQ(100, vec1[i]);
  }
}

}
