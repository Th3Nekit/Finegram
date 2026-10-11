// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file contains tests and benchmarks.

#include <vector>

#include <google/protobuf/io/coded_stream.h>

#include <limits.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>

#undef ULL
#define ULL(x) GOOGLE_ULONGLONG(x)

namespace google {
namespace protobuf {
namespace io {
namespace {

#define TEST_1D(FIXTURE, NAME, CASES)                                      \
  class FIXTURE##_##NAME##_DD : public FIXTURE {                           \
   protected:                                                              \
    template <typename CaseType>                                           \
    void DoSingleCase(const CaseType& CASES##_case);                       \
  };                                                                       \
                                                                           \
  TEST_F(FIXTURE##_##NAME##_DD, NAME) {                                    \
    for (int i = 0; i < GOOGLE_ARRAYSIZE(CASES); i++) {                           \
      SCOPED_TRACE(testing::Message()                                      \
        << #CASES " case #" << i << ": " << CASES[i]);                     \
      DoSingleCase(CASES[i]);                                              \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename CaseType>                                             \
  void FIXTURE##_##NAME##_DD::DoSingleCase(const CaseType& CASES##_case)

#define TEST_2D(FIXTURE, NAME, CASES1, CASES2)                             \
  class FIXTURE##_##NAME##_DD : public FIXTURE {                           \
   protected:                                                              \
    template <typename CaseType1, typename CaseType2>                      \
    void DoSingleCase(const CaseType1& CASES1##_case,                      \
                      const CaseType2& CASES2##_case);                     \
  };                                                                       \
                                                                           \
  TEST_F(FIXTURE##_##NAME##_DD, NAME) {                                    \
    for (int i = 0; i < GOOGLE_ARRAYSIZE(CASES1); i++) {                          \
      for (int j = 0; j < GOOGLE_ARRAYSIZE(CASES2); j++) {                        \
        SCOPED_TRACE(testing::Message()                                    \
          << #CASES1 " case #" << i << ": " << CASES1[i] << ", "           \
          << #CASES2 " case #" << j << ": " << CASES2[j]);                 \
        DoSingleCase(CASES1[i], CASES2[j]);                                \
      }                                                                    \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename CaseType1, typename CaseType2>                        \
  void FIXTURE##_##NAME##_DD::DoSingleCase(const CaseType1& CASES1##_case, \
                                           const CaseType2& CASES2##_case)

class CodedStreamTest : public testing::Test {
 protected:
  static const int kBufferSize = 1024 * 64;
  static uint8 buffer_[kBufferSize];
};

uint8 CodedStreamTest::buffer_[CodedStreamTest::kBufferSize];

const int kBlockSizes[] = {1, 2, 3, 5, 7, 13, 32, 1024};

struct VarintCase {
  uint8 bytes[10];
  int size;
  uint64 value;
};

inline std::ostream& operator<<(std::ostream& os, const VarintCase& c) {
  return os << c.value;
}

VarintCase kVarintCases[] = {

  {{0x00}      , 1, 0},
  {{0x01}      , 1, 1},
  {{0x7f}      , 1, 127},
  {{0xa2, 0x74}, 2, (0x22 << 0) | (0x74 << 7)},
  {{0xbe, 0xf7, 0x92, 0x84, 0x0b}, 5,
    (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
    (ULL(0x0b) << 28)},

  {{0xbe, 0xf7, 0x92, 0x84, 0x1b}, 5,
    (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
    (ULL(0x1b) << 28)},
  {{0x80, 0xe6, 0xeb, 0x9c, 0xc3, 0xc9, 0xa4, 0x49}, 8,
    (0x00 << 0) | (0x66 << 7) | (0x6b << 14) | (0x1c << 21) |
    (ULL(0x43) << 28) | (ULL(0x49) << 35) | (ULL(0x24) << 42) |
    (ULL(0x49) << 49)},

  {{0x9b, 0xa8, 0xf9, 0xc2, 0xbb, 0xd6, 0x80, 0x85, 0xa6, 0x01}, 10,
    (0x1b << 0) | (0x28 << 7) | (0x79 << 14) | (0x42 << 21) |
    (ULL(0x3b) << 28) | (ULL(0x56) << 35) | (ULL(0x00) << 42) |
    (ULL(0x05) << 49) | (ULL(0x26) << 56) | (ULL(0x01) << 63)},
};

TEST_2D(CodedStreamTest, ReadVarint32, kVarintCases, kBlockSizes) {
  memcpy(buffer_, kVarintCases_case.bytes, kVarintCases_case.size);
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    uint32 value;
    EXPECT_TRUE(coded_input.ReadVarint32(&value));
    EXPECT_EQ(static_cast<uint32>(kVarintCases_case.value), value);
  }

  EXPECT_EQ(kVarintCases_case.size, input.ByteCount());
}

TEST_2D(CodedStreamTest, ReadTag, kVarintCases, kBlockSizes) {
  memcpy(buffer_, kVarintCases_case.bytes, kVarintCases_case.size);
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    uint32 expected_value = static_cast<uint32>(kVarintCases_case.value);
    EXPECT_EQ(expected_value, coded_input.ReadTag());

    EXPECT_TRUE(coded_input.LastTagWas(expected_value));
    EXPECT_FALSE(coded_input.LastTagWas(expected_value + 1));
  }

  EXPECT_EQ(kVarintCases_case.size, input.ByteCount());
}

TEST_F(CodedStreamTest, EmptyInputBeforeEos) {
  class In : public ZeroCopyInputStream {
   public:
    In() : count_(0) {}
   private:
    virtual bool Next(const void** data, int* size) {
      *data = NULL;
      *size = 0;
      return count_++ < 2;
    }
    virtual void BackUp(int count)  {
      GOOGLE_LOG(FATAL) << "Tests never call this.";
    }
    virtual bool Skip(int count) {
      GOOGLE_LOG(FATAL) << "Tests never call this.";
      return false;
    }
    virtual int64 ByteCount() const { return 0; }
    int count_;
  } in;
  CodedInputStream input(&in);
  input.ReadTag();
  EXPECT_TRUE(input.ConsumedEntireMessage());
}

TEST_1D(CodedStreamTest, ExpectTag, kVarintCases) {

  buffer_[0] = '\0';
  memcpy(buffer_ + 1, kVarintCases_case.bytes, kVarintCases_case.size);
  ArrayInputStream input(buffer_, sizeof(buffer_));

  {
    CodedInputStream coded_input(&input);

    uint8 dummy;
    coded_input.ReadRaw(&dummy, 1);
    EXPECT_EQ((uint)'\0', (uint)dummy);

    uint32 expected_value = static_cast<uint32>(kVarintCases_case.value);

    if (kVarintCases_case.size <= 2) {
      EXPECT_FALSE(coded_input.ExpectTag(expected_value + 1));
      EXPECT_TRUE(coded_input.ExpectTag(expected_value));
    } else {
      EXPECT_FALSE(coded_input.ExpectTag(expected_value));
    }
  }

  if (kVarintCases_case.size <= 2) {
    EXPECT_EQ(kVarintCases_case.size + 1, input.ByteCount());
  } else {
    EXPECT_EQ(1, input.ByteCount());
  }
}

TEST_1D(CodedStreamTest, ExpectTagFromArray, kVarintCases) {
  memcpy(buffer_, kVarintCases_case.bytes, kVarintCases_case.size);

  const uint32 expected_value = static_cast<uint32>(kVarintCases_case.value);

  if (kVarintCases_case.size <= 2) {
    EXPECT_TRUE(NULL ==
                CodedInputStream::ExpectTagFromArray(buffer_,
                                                     expected_value + 1));
    EXPECT_TRUE(buffer_ + kVarintCases_case.size ==
                CodedInputStream::ExpectTagFromArray(buffer_, expected_value));
  } else {
    EXPECT_TRUE(NULL ==
                CodedInputStream::ExpectTagFromArray(buffer_, expected_value));
  }
}

TEST_2D(CodedStreamTest, ReadVarint64, kVarintCases, kBlockSizes) {
  memcpy(buffer_, kVarintCases_case.bytes, kVarintCases_case.size);
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    uint64 value;
    EXPECT_TRUE(coded_input.ReadVarint64(&value));
    EXPECT_EQ(kVarintCases_case.value, value);
  }

  EXPECT_EQ(kVarintCases_case.size, input.ByteCount());
}

TEST_2D(CodedStreamTest, WriteVarint32, kVarintCases, kBlockSizes) {
  if (kVarintCases_case.value > ULL(0x00000000FFFFFFFF)) {

    return;
  }

  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteVarint32(static_cast<uint32>(kVarintCases_case.value));
    EXPECT_FALSE(coded_output.HadError());

    EXPECT_EQ(kVarintCases_case.size, coded_output.ByteCount());
  }

  EXPECT_EQ(kVarintCases_case.size, output.ByteCount());
  EXPECT_EQ(0,
    memcmp(buffer_, kVarintCases_case.bytes, kVarintCases_case.size));
}

TEST_2D(CodedStreamTest, WriteVarint64, kVarintCases, kBlockSizes) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteVarint64(kVarintCases_case.value);
    EXPECT_FALSE(coded_output.HadError());

    EXPECT_EQ(kVarintCases_case.size, coded_output.ByteCount());
  }

  EXPECT_EQ(kVarintCases_case.size, output.ByteCount());
  EXPECT_EQ(0,
    memcmp(buffer_, kVarintCases_case.bytes, kVarintCases_case.size));
}

#if !defined(__GNUC__) || __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 3)

int32 kSignExtendedVarintCases[] = {
  0, 1, -1, 1237894, -37895138
};

TEST_2D(CodedStreamTest, WriteVarint32SignExtended,
        kSignExtendedVarintCases, kBlockSizes) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteVarint32SignExtended(kSignExtendedVarintCases_case);
    EXPECT_FALSE(coded_output.HadError());

    if (kSignExtendedVarintCases_case < 0) {
      EXPECT_EQ(10, coded_output.ByteCount());
    } else {
      EXPECT_LE(coded_output.ByteCount(), 5);
    }
  }

  if (kSignExtendedVarintCases_case < 0) {
    EXPECT_EQ(10, output.ByteCount());
  } else {
    EXPECT_LE(output.ByteCount(), 5);
  }

  ArrayInputStream input(buffer_, sizeof(buffer_));

  {
    CodedInputStream coded_input(&input);

    uint64 value;
    EXPECT_TRUE(coded_input.ReadVarint64(&value));

    EXPECT_EQ(kSignExtendedVarintCases_case, static_cast<int64>(value));
  }

  EXPECT_EQ(output.ByteCount(), input.ByteCount());
}

#endif

struct VarintErrorCase {
  uint8 bytes[12];
  int size;
  bool can_parse;
};

inline std::ostream& operator<<(std::ostream& os, const VarintErrorCase& c) {
  return os << "size " << c.size;
}

const VarintErrorCase kVarintErrorCases[] = {

  {{0x00}, 1, true},

  {{}, 0, false},

  {{0xf0, 0xab}, 2, false},

  {{0xf0, 0xab, 0xc9, 0x9a, 0xf8, 0xb2}, 6, false},

  {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01},
   11, false},
};

TEST_2D(CodedStreamTest, ReadVarint32Error, kVarintErrorCases, kBlockSizes) {
  memcpy(buffer_, kVarintErrorCases_case.bytes, kVarintErrorCases_case.size);
  ArrayInputStream input(buffer_, kVarintErrorCases_case.size,
                         kBlockSizes_case);
  CodedInputStream coded_input(&input);

  uint32 value;
  EXPECT_EQ(kVarintErrorCases_case.can_parse, coded_input.ReadVarint32(&value));
}

TEST_2D(CodedStreamTest, ReadVarint64Error, kVarintErrorCases, kBlockSizes) {
  memcpy(buffer_, kVarintErrorCases_case.bytes, kVarintErrorCases_case.size);
  ArrayInputStream input(buffer_, kVarintErrorCases_case.size,
                         kBlockSizes_case);
  CodedInputStream coded_input(&input);

  uint64 value;
  EXPECT_EQ(kVarintErrorCases_case.can_parse, coded_input.ReadVarint64(&value));
}

struct VarintSizeCase {
  uint64 value;
  int size;
};

inline std::ostream& operator<<(std::ostream& os, const VarintSizeCase& c) {
  return os << c.value;
}

VarintSizeCase kVarintSizeCases[] = {
  {0u, 1},
  {1u, 1},
  {127u, 1},
  {128u, 2},
  {758923u, 3},
  {4000000000u, 5},
  {ULL(41256202580718336), 8},
  {ULL(11964378330978735131), 10},
};

TEST_1D(CodedStreamTest, VarintSize32, kVarintSizeCases) {
  if (kVarintSizeCases_case.value > 0xffffffffu) {

    return;
  }

  EXPECT_EQ(kVarintSizeCases_case.size,
    CodedOutputStream::VarintSize32(
      static_cast<uint32>(kVarintSizeCases_case.value)));
}

TEST_1D(CodedStreamTest, VarintSize64, kVarintSizeCases) {
  EXPECT_EQ(kVarintSizeCases_case.size,
    CodedOutputStream::VarintSize64(kVarintSizeCases_case.value));
}

struct Fixed32Case {
  uint8 bytes[sizeof(uint32)];
  uint32 value;
};

struct Fixed64Case {
  uint8 bytes[sizeof(uint64)];
  uint64 value;
};

inline std::ostream& operator<<(std::ostream& os, const Fixed32Case& c) {
  return os << "0x" << hex << c.value << dec;
}

inline std::ostream& operator<<(std::ostream& os, const Fixed64Case& c) {
  return os << "0x" << hex << c.value << dec;
}

Fixed32Case kFixed32Cases[] = {
  {{0xef, 0xcd, 0xab, 0x90}, 0x90abcdefu},
  {{0x12, 0x34, 0x56, 0x78}, 0x78563412u},
};

Fixed64Case kFixed64Cases[] = {
  {{0xef, 0xcd, 0xab, 0x90, 0x12, 0x34, 0x56, 0x78}, ULL(0x7856341290abcdef)},
  {{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}, ULL(0x8877665544332211)},
};

TEST_2D(CodedStreamTest, ReadLittleEndian32, kFixed32Cases, kBlockSizes) {
  memcpy(buffer_, kFixed32Cases_case.bytes, sizeof(kFixed32Cases_case.bytes));
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    uint32 value;
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(kFixed32Cases_case.value, value);
  }

  EXPECT_EQ(sizeof(uint32), input.ByteCount());
}

TEST_2D(CodedStreamTest, ReadLittleEndian64, kFixed64Cases, kBlockSizes) {
  memcpy(buffer_, kFixed64Cases_case.bytes, sizeof(kFixed64Cases_case.bytes));
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    uint64 value;
    EXPECT_TRUE(coded_input.ReadLittleEndian64(&value));
    EXPECT_EQ(kFixed64Cases_case.value, value);
  }

  EXPECT_EQ(sizeof(uint64), input.ByteCount());
}

TEST_2D(CodedStreamTest, WriteLittleEndian32, kFixed32Cases, kBlockSizes) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteLittleEndian32(kFixed32Cases_case.value);
    EXPECT_FALSE(coded_output.HadError());

    EXPECT_EQ(sizeof(uint32), coded_output.ByteCount());
  }

  EXPECT_EQ(sizeof(uint32), output.ByteCount());
  EXPECT_EQ(0, memcmp(buffer_, kFixed32Cases_case.bytes, sizeof(uint32)));
}

TEST_2D(CodedStreamTest, WriteLittleEndian64, kFixed64Cases, kBlockSizes) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteLittleEndian64(kFixed64Cases_case.value);
    EXPECT_FALSE(coded_output.HadError());

    EXPECT_EQ(sizeof(uint64), coded_output.ByteCount());
  }

  EXPECT_EQ(sizeof(uint64), output.ByteCount());
  EXPECT_EQ(0, memcmp(buffer_, kFixed64Cases_case.bytes, sizeof(uint64)));
}

TEST_1D(CodedStreamTest, ReadLittleEndian32FromArray, kFixed32Cases) {
  memcpy(buffer_, kFixed32Cases_case.bytes, sizeof(kFixed32Cases_case.bytes));

  uint32 value;
  const uint8* end = CodedInputStream::ReadLittleEndian32FromArray(
      buffer_, &value);
  EXPECT_EQ(kFixed32Cases_case.value, value);
  EXPECT_TRUE(end == buffer_ + sizeof(value));
}

TEST_1D(CodedStreamTest, ReadLittleEndian64FromArray, kFixed64Cases) {
  memcpy(buffer_, kFixed64Cases_case.bytes, sizeof(kFixed64Cases_case.bytes));

  uint64 value;
  const uint8* end = CodedInputStream::ReadLittleEndian64FromArray(
      buffer_, &value);
  EXPECT_EQ(kFixed64Cases_case.value, value);
  EXPECT_TRUE(end == buffer_ + sizeof(value));
}

const char kRawBytes[] = "Some bytes which will be written and read raw.";

TEST_1D(CodedStreamTest, ReadRaw, kBlockSizes) {
  memcpy(buffer_, kRawBytes, sizeof(kRawBytes));
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);
  char read_buffer[sizeof(kRawBytes)];

  {
    CodedInputStream coded_input(&input);

    EXPECT_TRUE(coded_input.ReadRaw(read_buffer, sizeof(kRawBytes)));
    EXPECT_EQ(0, memcmp(kRawBytes, read_buffer, sizeof(kRawBytes)));
  }

  EXPECT_EQ(sizeof(kRawBytes), input.ByteCount());
}

TEST_1D(CodedStreamTest, WriteRaw, kBlockSizes) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedOutputStream coded_output(&output);

    coded_output.WriteRaw(kRawBytes, sizeof(kRawBytes));
    EXPECT_FALSE(coded_output.HadError());

    EXPECT_EQ(sizeof(kRawBytes), coded_output.ByteCount());
  }

  EXPECT_EQ(sizeof(kRawBytes), output.ByteCount());
  EXPECT_EQ(0, memcmp(buffer_, kRawBytes, sizeof(kRawBytes)));
}

TEST_1D(CodedStreamTest, ReadString, kBlockSizes) {
  memcpy(buffer_, kRawBytes, sizeof(kRawBytes));
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    string str;
    EXPECT_TRUE(coded_input.ReadString(&str, strlen(kRawBytes)));
    EXPECT_EQ(kRawBytes, str);
  }

  EXPECT_EQ(strlen(kRawBytes), input.ByteCount());
}

TEST_1D(CodedStreamTest, ReadStringImpossiblyLarge, kBlockSizes) {
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    string str;

    EXPECT_FALSE(coded_input.ReadString(&str, 1 << 30));
  }
}

TEST_F(CodedStreamTest, ReadStringImpossiblyLargeFromStringOnStack) {

  uint8 buffer[8];
  CodedInputStream coded_input(buffer, 8);
  string str;
  EXPECT_FALSE(coded_input.ReadString(&str, 1 << 30));
}

TEST_F(CodedStreamTest, ReadStringImpossiblyLargeFromStringOnHeap) {
  scoped_array<uint8> buffer(new uint8[8]);
  CodedInputStream coded_input(buffer.get(), 8);
  string str;
  EXPECT_FALSE(coded_input.ReadString(&str, 1 << 30));
}

const char kSkipTestBytes[] =
  "<Before skipping><To be skipped><After skipping>";
const char kSkipOutputTestBytes[] =
  "-----------------<To be skipped>----------------";

TEST_1D(CodedStreamTest, SkipInput, kBlockSizes) {
  memcpy(buffer_, kSkipTestBytes, sizeof(kSkipTestBytes));
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    string str;
    EXPECT_TRUE(coded_input.ReadString(&str, strlen("<Before skipping>")));
    EXPECT_EQ("<Before skipping>", str);
    EXPECT_TRUE(coded_input.Skip(strlen("<To be skipped>")));
    EXPECT_TRUE(coded_input.ReadString(&str, strlen("<After skipping>")));
    EXPECT_EQ("<After skipping>", str);
  }

  EXPECT_EQ(strlen(kSkipTestBytes), input.ByteCount());
}

TEST_F(CodedStreamTest, GetDirectBufferPointerInput) {
  ArrayInputStream input(buffer_, sizeof(buffer_), 8);
  CodedInputStream coded_input(&input);

  const void* ptr;
  int size;

  EXPECT_TRUE(coded_input.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_input.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_input.Skip(3));
  EXPECT_TRUE(coded_input.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_ + 3, ptr);
  EXPECT_EQ(5, size);

  EXPECT_TRUE(coded_input.Skip(5));
  EXPECT_TRUE(coded_input.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_ + 8, ptr);
  EXPECT_EQ(8, size);
}

TEST_F(CodedStreamTest, GetDirectBufferPointerInlineInput) {
  ArrayInputStream input(buffer_, sizeof(buffer_), 8);
  CodedInputStream coded_input(&input);

  const void* ptr;
  int size;

  coded_input.GetDirectBufferPointerInline(&ptr, &size);
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  coded_input.GetDirectBufferPointerInline(&ptr, &size);
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_input.Skip(3));
  coded_input.GetDirectBufferPointerInline(&ptr, &size);
  EXPECT_EQ(buffer_ + 3, ptr);
  EXPECT_EQ(5, size);

  EXPECT_TRUE(coded_input.Skip(5));
  coded_input.GetDirectBufferPointerInline(&ptr, &size);
  EXPECT_EQ(buffer_ + 8, ptr);
  EXPECT_EQ(0, size);
}

TEST_F(CodedStreamTest, GetDirectBufferPointerOutput) {
  ArrayOutputStream output(buffer_, sizeof(buffer_), 8);
  CodedOutputStream coded_output(&output);

  void* ptr;
  int size;

  EXPECT_TRUE(coded_output.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_output.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_output.Skip(3));
  EXPECT_TRUE(coded_output.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_ + 3, ptr);
  EXPECT_EQ(5, size);

  EXPECT_TRUE(coded_output.Skip(5));
  EXPECT_TRUE(coded_output.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_ + 8, ptr);
  EXPECT_EQ(8, size);

  EXPECT_TRUE(coded_output.Skip(22));
  EXPECT_TRUE(coded_output.GetDirectBufferPointer(&ptr, &size));
  EXPECT_EQ(buffer_ + 30, ptr);
  EXPECT_EQ(2, size);
}

TEST_1D(CodedStreamTest, BasicLimit, kBlockSizes) {
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    CodedInputStream::Limit limit = coded_input.PushLimit(8);

    uint32 value;
    EXPECT_EQ(8, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(4, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());
    EXPECT_FALSE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());

    coded_input.PopLimit(limit);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
  }

  EXPECT_EQ(12, input.ByteCount());
}

TEST_1D(CodedStreamTest, SmallLimitOnTopOfBigLimit, kBlockSizes) {
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    CodedInputStream::Limit limit1 = coded_input.PushLimit(8);
    EXPECT_EQ(8, coded_input.BytesUntilLimit());
    CodedInputStream::Limit limit2 = coded_input.PushLimit(4);

    uint32 value;

    EXPECT_EQ(4, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());
    EXPECT_FALSE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());

    coded_input.PopLimit(limit2);

    EXPECT_EQ(4, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());
    EXPECT_FALSE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());

    coded_input.PopLimit(limit1);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
  }

  EXPECT_EQ(12, input.ByteCount());
}

TEST_1D(CodedStreamTest, BigLimitOnTopOfSmallLimit, kBlockSizes) {
  ArrayInputStream input(buffer_, sizeof(buffer_), kBlockSizes_case);

  {
    CodedInputStream coded_input(&input);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    CodedInputStream::Limit limit1 = coded_input.PushLimit(4);
    EXPECT_EQ(4, coded_input.BytesUntilLimit());
    CodedInputStream::Limit limit2 = coded_input.PushLimit(8);

    uint32 value;

    EXPECT_EQ(4, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());
    EXPECT_FALSE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());

    coded_input.PopLimit(limit2);

    EXPECT_EQ(0, coded_input.BytesUntilLimit());
    EXPECT_FALSE(coded_input.ReadLittleEndian32(&value));
    EXPECT_EQ(0, coded_input.BytesUntilLimit());

    coded_input.PopLimit(limit1);

    EXPECT_EQ(-1, coded_input.BytesUntilLimit());
    EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
  }

  EXPECT_EQ(8, input.ByteCount());
}

TEST_F(CodedStreamTest, ExpectAtEnd) {

  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);

  EXPECT_FALSE(coded_input.ExpectAtEnd());

  CodedInputStream::Limit limit = coded_input.PushLimit(4);

  uint32 value;
  EXPECT_TRUE(coded_input.ReadLittleEndian32(&value));
  EXPECT_TRUE(coded_input.ExpectAtEnd());

  coded_input.PopLimit(limit);
  EXPECT_FALSE(coded_input.ExpectAtEnd());
}

TEST_F(CodedStreamTest, NegativeLimit) {

  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);

  CodedInputStream::Limit limit = coded_input.PushLimit(-1234);

  EXPECT_EQ(-1, coded_input.BytesUntilLimit());
  coded_input.PopLimit(limit);
}

TEST_F(CodedStreamTest, NegativeLimitAfterReading) {

  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);
  ASSERT_TRUE(coded_input.Skip(128));

  CodedInputStream::Limit limit = coded_input.PushLimit(-64);

  EXPECT_EQ(-1, coded_input.BytesUntilLimit());
  coded_input.PopLimit(limit);
}

TEST_F(CodedStreamTest, OverflowLimit) {

  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);
  ASSERT_TRUE(coded_input.Skip(128));

  CodedInputStream::Limit limit = coded_input.PushLimit(INT_MAX);

  EXPECT_EQ(-1, coded_input.BytesUntilLimit());
  coded_input.PopLimit(limit);
}

TEST_F(CodedStreamTest, TotalBytesLimit) {
  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);
  coded_input.SetTotalBytesLimit(16, -1);

  string str;
  EXPECT_TRUE(coded_input.ReadString(&str, 16));

  vector<string> errors;

  {
    ScopedMemoryLog error_log;
    EXPECT_FALSE(coded_input.ReadString(&str, 1));
    errors = error_log.GetMessages(ERROR);
  }

  ASSERT_EQ(1, errors.size());
  EXPECT_PRED_FORMAT2(testing::IsSubstring,
    "A protocol message was rejected because it was too big", errors[0]);

  coded_input.SetTotalBytesLimit(32, -1);
  EXPECT_TRUE(coded_input.ReadString(&str, 16));
}

TEST_F(CodedStreamTest, TotalBytesLimitNotValidMessageEnd) {

  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);

  coded_input.SetTotalBytesLimit(16, -1);
  CodedInputStream::Limit limit = coded_input.PushLimit(16);

  string str;
  EXPECT_TRUE(coded_input.ReadString(&str, 16));

  EXPECT_EQ(0, coded_input.ReadTag());
  EXPECT_TRUE(coded_input.ConsumedEntireMessage());

  coded_input.PopLimit(limit);

  EXPECT_EQ(0, coded_input.ReadTag());
  EXPECT_FALSE(coded_input.ConsumedEntireMessage());
}

TEST_F(CodedStreamTest, RecursionLimit) {
  ArrayInputStream input(buffer_, sizeof(buffer_));
  CodedInputStream coded_input(&input);
  coded_input.SetRecursionLimit(4);

  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());
  coded_input.DecrementRecursionDepth();
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  coded_input.DecrementRecursionDepth();
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());

  coded_input.SetRecursionLimit(6);
  EXPECT_TRUE(coded_input.IncrementRecursionDepth());
  EXPECT_FALSE(coded_input.IncrementRecursionDepth());
}

class ReallyBigInputStream : public ZeroCopyInputStream {
 public:
  ReallyBigInputStream() : backup_amount_(0), buffer_count_(0) {}
  ~ReallyBigInputStream() {}

  bool Next(const void** data, int* size) {

    EXPECT_EQ(0, backup_amount_);

    switch (buffer_count_++) {
      case 0:
        *data = buffer_;
        *size = sizeof(buffer_);
        return true;
      case 1:

        *data = buffer_;
        *size = INT_MAX;
        return true;
      default:
        return false;
    }
  }

  void BackUp(int count) {
    backup_amount_ = count;
  }

  bool Skip(int count)    { GOOGLE_LOG(FATAL) << "Not implemented."; return false; }
  int64 ByteCount() const { GOOGLE_LOG(FATAL) << "Not implemented."; return 0; }

  int backup_amount_;

 private:
  char buffer_[1024];
  int64 buffer_count_;
};

TEST_F(CodedStreamTest, InputOver2G) {

  ReallyBigInputStream input;

  vector<string> errors;

  {
    ScopedMemoryLog error_log;
    CodedInputStream coded_input(&input);
    string str;
    EXPECT_TRUE(coded_input.ReadString(&str, 512));
    EXPECT_TRUE(coded_input.ReadString(&str, 1024));
    errors = error_log.GetMessages(ERROR);
  }

  EXPECT_EQ(INT_MAX - 512, input.backup_amount_);
  EXPECT_EQ(0, errors.size());
}

}
}
}
}
