// Copyright 2024 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <openssl/aead.h>
#include <openssl/ssl.h>

#include "internal.h"

#if !defined(BORINGSSL_SHARED_LIBRARY)
BSSL_NAMESPACE_BEGIN
namespace {

TEST(ArrayTest, InitValueConstructs) {
  Array<uint8_t> array;
  ASSERT_TRUE(array.Init(10));
  EXPECT_EQ(array.size(), 10u);
  for (size_t i = 0; i < 10u; i++) {
    EXPECT_EQ(0u, array[i]);
  }
}

TEST(ArrayDeathTest, BoundsChecks) {
  Array<int> array;
  const int v[] = {1, 2, 3, 4};
  ASSERT_TRUE(array.CopyFrom(v));
  EXPECT_DEATH_IF_SUPPORTED(array[4], "");
}

TEST(VectorTest, Resize) {
  Vector<size_t> vec;
  ASSERT_TRUE(vec.empty());
  EXPECT_EQ(vec.size(), 0u);

  ASSERT_TRUE(vec.Push(42));
  ASSERT_TRUE(!vec.empty());
  EXPECT_EQ(vec.size(), 1u);

  for (size_t i = 0; i < 16; i++) {
    ASSERT_TRUE(vec.Push(i + 1));
  }

  EXPECT_EQ(vec.size(), 17u);

  for (size_t i = 0; i < vec.size(); i++) {
    EXPECT_EQ(vec[i], i == 0 ? 42 : i);
  }

  vec.clear();
  ASSERT_TRUE(vec.empty());
  EXPECT_EQ(vec.size(), 0u);

  ASSERT_TRUE(vec.Push(42));
  ASSERT_TRUE(!vec.empty());
  EXPECT_EQ(vec.size(), 1u);
  EXPECT_EQ(vec[0], 42u);
}

TEST(VectorTest, MoveConstructor) {
  Vector<size_t> vec;
  for (size_t i = 0; i < 100; i++) {
    ASSERT_TRUE(vec.Push(i));
  }

  Vector<size_t> vec_moved(std::move(vec));
  for (size_t i = 0; i < 100; i++) {
    EXPECT_EQ(vec_moved[i], i);
  }
}

TEST(VectorTest, VectorContainingVectors) {

  struct TagAndArray {
    size_t tag;
    Vector<size_t> vec;
  };

  Vector<TagAndArray> vec;
  for (size_t i = 0; i < 100; i++) {
    TagAndArray elem;
    elem.tag = i;
    for (size_t j = 0; j < i; j++) {
      ASSERT_TRUE(elem.vec.Push(j));
    }
    ASSERT_TRUE(vec.Push(std::move(elem)));
  }
  EXPECT_EQ(vec.size(), static_cast<size_t>(100));

  Vector<TagAndArray> vec_moved(std::move(vec));
  EXPECT_EQ(vec_moved.size(), static_cast<size_t>(100));
  size_t count = 0;
  for (const TagAndArray &elem : vec_moved) {

    EXPECT_EQ(&elem, &vec_moved[count]);

    EXPECT_EQ(elem.tag, count);
    EXPECT_EQ(elem.vec.size(), count);
    for (size_t j = 0; j < count; j++) {
      EXPECT_EQ(elem.vec[j], j);
    }
    count++;
  }
}

TEST(VectorTest, NotDefaultConstructible) {
  struct NotDefaultConstructible {
    explicit NotDefaultConstructible(size_t n) { BSSL_CHECK(array.Init(n)); }
    Array<int> array;
  };

  Vector<NotDefaultConstructible> vec;
  ASSERT_TRUE(vec.Push(NotDefaultConstructible(0)));
  ASSERT_TRUE(vec.Push(NotDefaultConstructible(1)));
  ASSERT_TRUE(vec.Push(NotDefaultConstructible(2)));
  ASSERT_TRUE(vec.Push(NotDefaultConstructible(3)));
  EXPECT_EQ(vec.size(), 4u);
  EXPECT_EQ(0u, vec[0].array.size());
  EXPECT_EQ(1u, vec[1].array.size());
  EXPECT_EQ(2u, vec[2].array.size());
  EXPECT_EQ(3u, vec[3].array.size());
}

TEST(VectorDeathTest, BoundsChecks) {
  Vector<int> vec;
  ASSERT_TRUE(vec.Push(1));

  EXPECT_DEATH_IF_SUPPORTED(vec[1], "");

  EXPECT_DEATH_IF_SUPPORTED(vec[10000], "");
}

TEST(InplaceVector, Basic) {
  InplaceVector<int, 4> vec;
  EXPECT_TRUE(vec.empty());
  EXPECT_EQ(0u, vec.size());
  EXPECT_EQ(vec.begin(), vec.end());

  int data3[] = {1, 2, 3};
  ASSERT_TRUE(vec.TryCopyFrom(data3));
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(3u, vec.size());
  auto iter = vec.begin();
  EXPECT_EQ(1, vec[0]);
  EXPECT_EQ(1, *iter);
  iter++;
  EXPECT_EQ(2, vec[1]);
  EXPECT_EQ(2, *iter);
  iter++;
  EXPECT_EQ(3, vec[2]);
  EXPECT_EQ(3, *iter);
  iter++;
  EXPECT_EQ(iter, vec.end());
  EXPECT_EQ(Span(vec), Span(data3));

  InplaceVector<int, 4> vec2 = vec;
  EXPECT_EQ(Span(vec), Span(vec2));

  InplaceVector<int, 4> vec3;
  vec3 = vec;
  EXPECT_EQ(Span(vec), Span(vec2));

  int data4[] = {1, 2, 3, 4};
  ASSERT_TRUE(vec.TryCopyFrom(data4));
  EXPECT_EQ(Span(vec), Span(data4));

  int data5[] = {1, 2, 3, 4, 5};
  EXPECT_FALSE(vec.TryCopyFrom(data5));
  EXPECT_FALSE(vec.TryResize(5));

  ASSERT_TRUE(vec.TryResize(3));
  EXPECT_EQ(Span(vec), Span(data3));

  ASSERT_TRUE(vec.TryResize(4));
  EXPECT_EQ(vec[3], 0);

  vec.CopyFrom(data4);
  const auto *ptr = &vec;
  vec = *ptr;
  EXPECT_EQ(Span(vec), Span(data4));
}

TEST(InplaceVectorTest, ComplexType) {
  InplaceVector<std::vector<int>, 4> vec_of_vecs;
  const std::vector<int> data[] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  vec_of_vecs.CopyFrom(data);
  EXPECT_EQ(Span(vec_of_vecs), Span(data));

  vec_of_vecs.Resize(2);
  EXPECT_EQ(Span(vec_of_vecs), Span(data, 2));

  vec_of_vecs.Resize(4);
  EXPECT_EQ(4u, vec_of_vecs.size());
  EXPECT_EQ(vec_of_vecs[0], data[0]);
  EXPECT_EQ(vec_of_vecs[1], data[1]);
  EXPECT_TRUE(vec_of_vecs[2].empty());
  EXPECT_TRUE(vec_of_vecs[3].empty());

  InplaceVector<std::vector<int>, 4> vec_of_vecs2 = vec_of_vecs;
  EXPECT_EQ(4u, vec_of_vecs2.size());
  EXPECT_EQ(vec_of_vecs2[0], data[0]);
  EXPECT_EQ(vec_of_vecs2[1], data[1]);
  EXPECT_TRUE(vec_of_vecs2[2].empty());
  EXPECT_TRUE(vec_of_vecs2[3].empty());

  InplaceVector<std::vector<int>, 4> vec_of_vecs3;
  vec_of_vecs3 = vec_of_vecs;
  EXPECT_EQ(4u, vec_of_vecs3.size());
  EXPECT_EQ(vec_of_vecs3[0], data[0]);
  EXPECT_EQ(vec_of_vecs3[1], data[1]);
  EXPECT_TRUE(vec_of_vecs3[2].empty());
  EXPECT_TRUE(vec_of_vecs3[3].empty());

  InplaceVector<std::vector<int>, 4> vec_of_vecs4 = std::move(vec_of_vecs);
  EXPECT_EQ(4u, vec_of_vecs4.size());
  EXPECT_EQ(vec_of_vecs4[0], data[0]);
  EXPECT_EQ(vec_of_vecs4[1], data[1]);
  EXPECT_TRUE(vec_of_vecs4[2].empty());
  EXPECT_TRUE(vec_of_vecs4[3].empty());

  EXPECT_EQ(4u, vec_of_vecs.size());
  for (const auto &vec : vec_of_vecs) {
    EXPECT_TRUE(vec.empty());
  }

  InplaceVector<std::vector<int>, 4> vec_of_vecs5;
  vec_of_vecs5 = std::move(vec_of_vecs4);
  EXPECT_EQ(4u, vec_of_vecs5.size());
  EXPECT_EQ(vec_of_vecs5[0], data[0]);
  EXPECT_EQ(vec_of_vecs5[1], data[1]);
  EXPECT_TRUE(vec_of_vecs5[2].empty());
  EXPECT_TRUE(vec_of_vecs5[3].empty());

  EXPECT_EQ(4u, vec_of_vecs4.size());
  for (const auto &vec : vec_of_vecs4) {
    EXPECT_TRUE(vec.empty());
  }

  std::vector<int> v = {42};
  vec_of_vecs5.Resize(3);
  EXPECT_TRUE(vec_of_vecs5.TryPushBack(v));
  EXPECT_EQ(v, vec_of_vecs5[3]);
  EXPECT_FALSE(vec_of_vecs5.TryPushBack(v));
}

TEST(InplaceVectorTest, EraseIf) {

  class NoSelfMove {
   public:
    explicit NoSelfMove(int v) : v_(std::make_unique<int>(v)) {}
    NoSelfMove(NoSelfMove &&other) { *this = std::move(other); }
    NoSelfMove &operator=(NoSelfMove &&other) {
      BSSL_CHECK(this != &other);
      v_ = std::move(other.v_);
      return *this;
    }

    int value() const { return *v_; }

   private:
    std::unique_ptr<int> v_;
  };

  InplaceVector<NoSelfMove, 8> vec;
  auto reset = [&] {
    vec.clear();
    for (int i = 0; i < 8; i++) {
      vec.PushBack(NoSelfMove(i));
    }
  };
  auto expect = [&](const std::vector<int> &expected) {
    ASSERT_EQ(vec.size(), expected.size());
    for (size_t i = 0; i < vec.size(); i++) {
      SCOPED_TRACE(i);
      EXPECT_EQ(vec[i].value(), expected[i]);
    }
  };

  reset();
  vec.EraseIf([](const auto &) { return false; });
  expect({0, 1, 2, 3, 4, 5, 6, 7});

  reset();
  vec.EraseIf([](const auto &) { return true; });
  expect({});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() < 4; });
  expect({4, 5, 6, 7});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() >= 4; });
  expect({0, 1, 2, 3});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() % 2 == 0; });
  expect({1, 3, 5, 7});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() % 2 == 1; });
  expect({0, 2, 4, 6});

  reset();
  vec.EraseIf([](const auto &v) { return 2 <= v.value() && v.value() <= 5; });
  expect({0, 1, 6, 7});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() == 0; });
  expect({1, 2, 3, 4, 5, 6, 7});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() == 4; });
  expect({0, 1, 2, 3, 5, 6, 7});

  reset();
  vec.EraseIf([](const auto &v) { return v.value() == 7; });
  expect({0, 1, 2, 3, 4, 5, 6});
}

TEST(InplaceVectorDeathTest, BoundsChecks) {
  InplaceVector<int, 4> vec;

  EXPECT_DEATH_IF_SUPPORTED(vec[0], "");
  int data[] = {1, 2, 3};
  vec.CopyFrom(data);

  EXPECT_DEATH_IF_SUPPORTED(vec[3], "");
  EXPECT_DEATH_IF_SUPPORTED(vec[4], "");
  EXPECT_DEATH_IF_SUPPORTED(vec[1000], "");

  EXPECT_DEATH_IF_SUPPORTED(vec.Resize(5), "");
  EXPECT_DEATH_IF_SUPPORTED(vec.ResizeForOverwrite(5), "");
  int too_much_data[] = {1, 2, 3, 4, 5};
  EXPECT_DEATH_IF_SUPPORTED(vec.CopyFrom(too_much_data), "");
  vec.Resize(4);
  EXPECT_DEATH_IF_SUPPORTED(vec.PushBack(42), "");
}

TEST(ReconstructSeqnumTest, Increment) {

  EXPECT_EQ(reconstruct_seqnum(0, 0xff, 0), 0u);
  EXPECT_EQ(reconstruct_seqnum(1, 0xff, 0), 1u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xff, 0), 2u);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, 0), 0u);
  EXPECT_EQ(reconstruct_seqnum(1, 0xffff, 0), 1u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xffff, 0), 2u);

  EXPECT_EQ(reconstruct_seqnum(0xff, 0xff, 0), 0xffu);
  EXPECT_EQ(reconstruct_seqnum(0xfe, 0xff, 0), 0xfeu);
  EXPECT_EQ(reconstruct_seqnum(0xffff, 0xffff, 0), 0xffffu);
  EXPECT_EQ(reconstruct_seqnum(0xfffe, 0xffff, 0), 0xfffeu);

  EXPECT_EQ(reconstruct_seqnum(0, 0xff, 0xff), 0x100u);
  EXPECT_EQ(reconstruct_seqnum(1, 0xff, 0xff), 0x101u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xff, 0xff), 0x102u);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, 0xffff), 0x10000u);
  EXPECT_EQ(reconstruct_seqnum(1, 0xffff, 0xffff), 0x10001u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xffff, 0xffff), 0x10002u);

  EXPECT_EQ(reconstruct_seqnum(0xff, 0xff, 0x2f0), 0x2ffu);
  EXPECT_EQ(reconstruct_seqnum(0xfe, 0xff, 0x2f0), 0x2feu);
  EXPECT_EQ(reconstruct_seqnum(0xffff, 0xffff, 0x2f000), 0x2ffffu);
  EXPECT_EQ(reconstruct_seqnum(0xfffe, 0xffff, 0x2f000), 0x2fffeu);

  constexpr uint64_t kMaxSequence = (uint64_t{1} << 48) - 1;
  EXPECT_EQ(reconstruct_seqnum(0xff, 0xff, kMaxSequence), kMaxSequence);
  EXPECT_EQ(reconstruct_seqnum(0xff, 0xff, kMaxSequence - 1), kMaxSequence);
  EXPECT_EQ(reconstruct_seqnum(0xffff, 0xffff, kMaxSequence), kMaxSequence);
  EXPECT_EQ(reconstruct_seqnum(0xffff, 0xffff, kMaxSequence - 1), kMaxSequence);
}

TEST(ReconstructSeqnumTest, Decrement) {

  EXPECT_EQ(reconstruct_seqnum(0, 0xff, 0x10), 0u);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, 0x1000), 0u);

  EXPECT_EQ(reconstruct_seqnum(0, 0xff, 0x210), 0x200u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xff, 0x210), 0x202u);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, 0x43210), 0x40000u);
  EXPECT_EQ(reconstruct_seqnum(2, 0xffff, 0x43210), 0x40002u);

  EXPECT_EQ(reconstruct_seqnum(0xff, 0xff, 0x200), 0x1ffu);
  EXPECT_EQ(reconstruct_seqnum(0xfe, 0xff, 0x200), 0x1feu);
  EXPECT_EQ(reconstruct_seqnum(0xffff, 0xffff, 0x20000), 0x1ffffu);
  EXPECT_EQ(reconstruct_seqnum(0xfffe, 0xffff, 0x20000), 0x1fffeu);

  constexpr uint64_t kMaxSequence = (uint64_t{1} << 48) - 1;

  constexpr uint64_t kMaxSequence00 = kMaxSequence - 0xff;

  constexpr uint64_t kMaxSequence0000 = kMaxSequence - 0xffff;

  EXPECT_EQ(reconstruct_seqnum(0, 0xff, kMaxSequence), kMaxSequence00);
  EXPECT_EQ(reconstruct_seqnum(0, 0xff, kMaxSequence - 1), kMaxSequence00);
  EXPECT_EQ(reconstruct_seqnum(1, 0xff, kMaxSequence), kMaxSequence00 + 0x01);
  EXPECT_EQ(reconstruct_seqnum(1, 0xff, kMaxSequence - 1),
            kMaxSequence00 + 0x01);
  EXPECT_EQ(reconstruct_seqnum(0xfe, 0xff, kMaxSequence),
            kMaxSequence00 + 0xfe);
  EXPECT_EQ(reconstruct_seqnum(0xfd, 0xff, kMaxSequence - 1),
            kMaxSequence00 + 0xfd);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, kMaxSequence), kMaxSequence0000);
  EXPECT_EQ(reconstruct_seqnum(0, 0xffff, kMaxSequence - 1), kMaxSequence0000);
  EXPECT_EQ(reconstruct_seqnum(1, 0xffff, kMaxSequence),
            kMaxSequence0000 + 0x0001);
  EXPECT_EQ(reconstruct_seqnum(1, 0xffff, kMaxSequence - 1),
            kMaxSequence0000 + 0x0001);
  EXPECT_EQ(reconstruct_seqnum(0xfffe, 0xffff, kMaxSequence),
            kMaxSequence0000 + 0xfffe);
  EXPECT_EQ(reconstruct_seqnum(0xfffd, 0xffff, kMaxSequence - 1),
            kMaxSequence0000 + 0xfffd);
}

TEST(ReconstructSeqnumTest, Halfway) {

  EXPECT_EQ(reconstruct_seqnum(0x80, 0xff, 0x100), 0x180u);
  EXPECT_EQ(reconstruct_seqnum(0x81, 0xff, 0x100), 0x181u);
  EXPECT_EQ(reconstruct_seqnum(0x82, 0xff, 0x100), 0x82u);

  EXPECT_EQ(reconstruct_seqnum(0x8000, 0xffff, 0x10000), 0x18000u);
  EXPECT_EQ(reconstruct_seqnum(0x8001, 0xffff, 0x10000), 0x18001u);
  EXPECT_EQ(reconstruct_seqnum(0x8002, 0xffff, 0x10000), 0x8002u);
}

TEST(DTLSMessageBitmapTest, Basic) {

  auto expect_bitmap = [](const DTLSMessageBitmap &b,
                          const std::vector<DTLSMessageBitmap::Range> &ranges) {
    EXPECT_EQ(ranges.empty(), b.IsComplete());
    size_t start = 0;
    for (const auto &r : ranges) {
      for (; start < r.start; start++) {
        SCOPED_TRACE(start);
        EXPECT_EQ(b.NextUnmarkedRange(start), r);
      }
      for (; start < r.end; start++) {
        SCOPED_TRACE(start);
        EXPECT_EQ(b.NextUnmarkedRange(start),
                  (DTLSMessageBitmap::Range{start, r.end}));
      }
    }
    EXPECT_TRUE(b.NextUnmarkedRange(start).empty());
    EXPECT_TRUE(b.NextUnmarkedRange(start + 1).empty());
    EXPECT_TRUE(b.NextUnmarkedRange(start + 42).empty());

    std::vector<DTLSMessageBitmap::Range> got_ranges;
    for (auto r = b.NextUnmarkedRange(0); !r.empty();
         r = b.NextUnmarkedRange(r.end)) {
      got_ranges.push_back(r);
    }
    EXPECT_EQ(ranges, got_ranges);
  };

  DTLSMessageBitmap bitmap;
  expect_bitmap(bitmap, {});

  ASSERT_TRUE(bitmap.Init(0));
  expect_bitmap(bitmap, {});
  bitmap.MarkRange(0, 0);
  expect_bitmap(bitmap, {});

  ASSERT_TRUE(bitmap.Init(100));
  expect_bitmap(bitmap, {{0, 100}});
  for (size_t i = 0; i < 100; i++) {
    SCOPED_TRACE(i);
    bitmap.MarkRange(i, i + 1);
    if (i < 99) {
      expect_bitmap(bitmap, {{i + 1, 100}});
    } else {
      expect_bitmap(bitmap, {});
    }
  }

  ASSERT_TRUE(bitmap.Init(100));
  expect_bitmap(bitmap, {{0, 100}});
  for (size_t i = 100; i > 0; i--) {
    SCOPED_TRACE(i);
    bitmap.MarkRange(i - 1, i);
    if (i > 1) {
      expect_bitmap(bitmap, {{0, i - 1}});
    } else {
      expect_bitmap(bitmap, {});
    }
  }

  ASSERT_TRUE(bitmap.Init(100));
  expect_bitmap(bitmap, {{0, 100}});
  for (size_t i = 0; i < 100; i++) {
    SCOPED_TRACE(i);
    bitmap.MarkRange(i / 2, i + 1);
    if (i < 99) {
      expect_bitmap(bitmap, {{i + 1, 100}});
    } else {
      expect_bitmap(bitmap, {});
    }
  }

  ASSERT_TRUE(bitmap.Init(100));
  bitmap.MarkRange(1, 2);
  bitmap.MarkRange(3, 6);
  bitmap.MarkRange(9, 18);
  bitmap.MarkRange(27, 54);
  bitmap.MarkRange(81, 162);
  expect_bitmap(bitmap, {{0, 1}, {2, 3}, {6, 9}, {18, 27}, {54, 81}});

  bitmap.MarkRange(6 - 2, 9 - 2);
  bitmap.MarkRange(18 - 4, 27 - 4);
  bitmap.MarkRange(54 - 8, 81 - 8);
  expect_bitmap(bitmap,
                {{0, 1}, {2, 3}, {9 - 2, 9}, {27 - 4, 27}, {81 - 8, 81}});

  bitmap.MarkRange(1, 2);
  bitmap.MarkRange(3, 6);
  bitmap.MarkRange(9, 18);
  bitmap.MarkRange(27, 54);
  bitmap.MarkRange(81, 162);
  expect_bitmap(bitmap,
                {{0, 1}, {2, 3}, {9 - 2, 9}, {27 - 4, 27}, {81 - 8, 81}});

  DTLSMessageBitmap bitmap2 = std::move(bitmap);
  expect_bitmap(bitmap, {});
  expect_bitmap(bitmap2,
                {{0, 1}, {2, 3}, {9 - 2, 9}, {27 - 4, 27}, {81 - 8, 81}});

  bitmap2.MarkRange(27 - 2, 100);
  expect_bitmap(bitmap2, {{0, 1}, {2, 3}, {9 - 2, 9}, {27 - 4, 27 - 2}});
  bitmap2.MarkRange(0, 50);
  expect_bitmap(bitmap2, {});

  ASSERT_TRUE(bitmap.Init(10));
  expect_bitmap(bitmap, {{0, 10}});
  bitmap.MarkRange(5, SIZE_MAX);
  expect_bitmap(bitmap, {{0, 5}});
  bitmap.MarkRange(0, SIZE_MAX);
  expect_bitmap(bitmap, {});
}

TEST(MRUQueueTest, Basic) {

  MRUQueue<std::unique_ptr<int>, 8> queue;
  auto expect_queue = [&](const std::vector<int> &expected) {
    EXPECT_EQ(queue.size(), expected.size());
    EXPECT_EQ(queue.empty(), expected.empty());
    std::vector<int> queue_values;
    for (size_t i = 0; i < queue.size(); i++) {
      queue_values.push_back(*queue[i]);
    }
    EXPECT_EQ(queue_values, expected);
  };

  expect_queue({});
  queue.PushBack(std::make_unique<int>(1));
  expect_queue({1});
  queue.PushBack(std::make_unique<int>(2));
  expect_queue({1, 2});
  queue.PushBack(std::make_unique<int>(3));
  expect_queue({1, 2, 3});
  queue.PushBack(std::make_unique<int>(4));
  expect_queue({1, 2, 3, 4});
  queue.PushBack(std::make_unique<int>(5));
  expect_queue({1, 2, 3, 4, 5});
  queue.PushBack(std::make_unique<int>(6));
  expect_queue({1, 2, 3, 4, 5, 6});
  queue.PushBack(std::make_unique<int>(7));
  expect_queue({1, 2, 3, 4, 5, 6, 7});
  queue.PushBack(std::make_unique<int>(8));
  expect_queue({1, 2, 3, 4, 5, 6, 7, 8});

  queue.PushBack(std::make_unique<int>(9));
  expect_queue({2, 3, 4, 5, 6, 7, 8, 9});
  queue.PushBack(std::make_unique<int>(10));
  expect_queue({3, 4, 5, 6, 7, 8, 9, 10});
  queue.PushBack(std::make_unique<int>(11));
  expect_queue({4, 5, 6, 7, 8, 9, 10, 11});
  queue.PushBack(std::make_unique<int>(12));
  expect_queue({5, 6, 7, 8, 9, 10, 11, 12});
  queue.PushBack(std::make_unique<int>(13));
  expect_queue({6, 7, 8, 9, 10, 11, 12, 13});
  queue.PushBack(std::make_unique<int>(14));
  expect_queue({7, 8, 9, 10, 11, 12, 13, 14});
  queue.PushBack(std::make_unique<int>(15));
  expect_queue({8, 9, 10, 11, 12, 13, 14, 15});
  queue.PushBack(std::make_unique<int>(16));
  expect_queue({9, 10, 11, 12, 13, 14, 15, 16});
  queue.PushBack(std::make_unique<int>(17));
  expect_queue({10, 11, 12, 13, 14, 15, 16, 17});

  queue.Clear();
  expect_queue({});
  queue.PushBack(std::make_unique<int>(1));
  expect_queue({1});
  queue.PushBack(std::make_unique<int>(2));
  expect_queue({1, 2});
  queue.PushBack(std::make_unique<int>(3));
  expect_queue({1, 2, 3});
}

TEST(SSLAEADContextTest, Lengths) {
  struct LengthTest {

    size_t min_plaintext_len;
    size_t max_plaintext_len;
    size_t ciphertext_len;
  };

  struct CipherLengthTest {

    uint16_t cipher;
    uint16_t version;
    size_t enc_key_len, mac_key_len, fixed_iv_len;
    size_t block_size;
    std::vector<LengthTest> length_tests;
  };

  const CipherLengthTest kTests[] = {

      {
                     SSL3_CK_RSA_DES_192_CBC3_SHA & 0xffff,
                      TLS1_2_VERSION,
                          24,
                          20,
                           0,
                         8,
          {
              {                      0,
                                     3,
                                  32},
              {                      4,
                                     11,
                                  40},
              {                      12,
                                     19,
                                  48},
          },
      },

      {
                     TLS1_CK_RSA_WITH_AES_128_SHA & 0xffff,
                      TLS1_2_VERSION,
                          16,
                          20,
                           0,
                         16,
          {
              {                      0,
                                     11,
                                  48},
              {                      12,
                                     27,
                                  64},
              {                      38,
                                     43,
                                  80},
          },
      },

      {
                     TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA256 & 0xffff,
                      TLS1_2_VERSION,
                          16,
                          32,
                           0,
                         16,
          {
              {                      0,
                                     15,
                                  64},
              {                      16,
                                     31,
                                  80},
              {                      32,
                                     47,
                                  96},
          },
      },

      {
                     TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256 & 0xffff,
                      TLS1_2_VERSION,
                          16,
                          0,
                           4,
                         1,
          {
              {                      0,
                                     0,
                                  24},
              {                      1,
                                     1,
                                  25},
              {                      2,
                                     2,
                                  26},
              {                      42,
                                     42,
                                  66},
          },
      },

      {
                     TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 & 0xffff,
                      TLS1_2_VERSION,
                          32,
                          0,
                           12,
                         1,
          {
              {                      0,
                                     0,
                                  16},
              {                      1,
                                     1,
                                  17},
              {                      2,
                                     2,
                                  18},
              {                      42,
                                     42,
                                  58},
          },
      },
      {
                     TLS1_CK_AES_128_GCM_SHA256 & 0xffff,
                      TLS1_3_VERSION,
                          16,
                          0,
                           12,
                         1,
          {
              {                      0,
                                     0,
                                  16},
              {                      1,
                                     1,
                                  17},
              {                      2,
                                     2,
                                  18},
              {                      42,
                                     42,
                                  58},
          },
      },
      {
                     TLS1_CK_CHACHA20_POLY1305_SHA256 & 0xffff,
                      TLS1_3_VERSION,
                          32,
                          0,
                           12,
                         1,
          {
              {                      0,
                                     0,
                                  16},
              {                      1,
                                     1,
                                  17},
              {                      2,
                                     2,
                                  18},
              {                      42,
                                     42,
                                  58},
          },
      },
  };

  for (const auto &cipher_test : kTests) {
    const SSL_CIPHER *cipher =
        SSL_get_cipher_by_value(static_cast<uint16_t>(cipher_test.cipher));
    ASSERT_TRUE(cipher) << "Could not find cipher " << cipher_test.cipher;
    SCOPED_TRACE(SSL_CIPHER_standard_name(cipher));

    const uint8_t kZeros[EVP_AEAD_MAX_KEY_LENGTH] = {0};
    UniquePtr<SSLAEADContext> aead =
        SSLAEADContext::Create(evp_aead_seal, cipher_test.version, cipher,
                               Span(kZeros).first(cipher_test.enc_key_len),
                               Span(kZeros).first(cipher_test.mac_key_len),
                               Span(kZeros).first(cipher_test.fixed_iv_len));
    ASSERT_TRUE(aead);

    for (const auto &t : cipher_test.length_tests) {
      SCOPED_TRACE(t.ciphertext_len);

      for (size_t plaintext_len = t.min_plaintext_len;
           plaintext_len <= t.max_plaintext_len; plaintext_len++) {
        SCOPED_TRACE(plaintext_len);
        size_t out_len;
        ASSERT_TRUE(aead->CiphertextLen(&out_len, plaintext_len, 0));
        EXPECT_EQ(out_len, t.ciphertext_len);
      }

      EXPECT_EQ(aead->MaxSealInputLen(t.ciphertext_len), t.max_plaintext_len);
      for (size_t extra = 0; extra < cipher_test.block_size; extra++) {

        SCOPED_TRACE(extra);
        EXPECT_EQ(aead->MaxSealInputLen(t.ciphertext_len + extra),
                  t.max_plaintext_len);
      }
    }
  }
}

}
BSSL_NAMESPACE_END
#endif
