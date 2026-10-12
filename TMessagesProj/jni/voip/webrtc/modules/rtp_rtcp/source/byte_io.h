/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_BYTE_IO_H_
#define MODULES_RTP_RTCP_SOURCE_BYTE_IO_H_

#include <stdint.h>

#include <limits>

namespace webrtc {

static_assert(
    (-1 & 0x03) == 0x03,
    "Only two's complement representation of signed integers supported.");

#define kSizeErrorMsg "Byte size must be less than or equal to data type size."

template <typename T>
struct UnsignedOf;

template <typename T,
          unsigned int B = sizeof(T),
          bool is_signed = std::numeric_limits<T>::is_signed>
class ByteReader;

template <typename T, unsigned int B>
class ByteReader<T, B, false> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    static_assert(B <= sizeof(T), kSizeErrorMsg);
    return InternalReadBigEndian(data);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    static_assert(B <= sizeof(T), kSizeErrorMsg);
    return InternalReadLittleEndian(data);
  }

 private:
  static T InternalReadBigEndian(const uint8_t* data) {
    T val(0);
    for (unsigned int i = 0; i < B; ++i)
      val |= static_cast<T>(data[i]) << ((B - 1 - i) * 8);
    return val;
  }

  static T InternalReadLittleEndian(const uint8_t* data) {
    T val(0);
    for (unsigned int i = 0; i < B; ++i)
      val |= static_cast<T>(data[i]) << (i * 8);
    return val;
  }
};

template <typename T, unsigned int B>
class ByteReader<T, B, true> {
 public:
  typedef typename UnsignedOf<T>::Type U;

  static T ReadBigEndian(const uint8_t* data) {
    U unsigned_val = ByteReader<T, B, false>::ReadBigEndian(data);
    if (B < sizeof(T))
      unsigned_val = SignExtend(unsigned_val);
    return ReinterpretAsSigned(unsigned_val);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    U unsigned_val = ByteReader<T, B, false>::ReadLittleEndian(data);
    if (B < sizeof(T))
      unsigned_val = SignExtend(unsigned_val);
    return ReinterpretAsSigned(unsigned_val);
  }

 private:

  static T ReinterpretAsSigned(U unsigned_val) {

    const U kUnsignedHighestBitMask = static_cast<U>(1)
                                      << ((sizeof(U) * 8) - 1);

    const T kSignedHighestBitMask = std::numeric_limits<T>::min();

    T val;
    if ((unsigned_val & kUnsignedHighestBitMask) != 0) {

      val = static_cast<T>(unsigned_val & ~kUnsignedHighestBitMask);
      val |= kSignedHighestBitMask;
    } else {
      val = static_cast<T>(unsigned_val);
    }
    return val;
  }

  static U SignExtend(const U val) {
    const uint8_t kMsb = static_cast<uint8_t>(val >> ((B - 1) * 8));
    if ((kMsb & 0x80) != 0) {

      const U kUsedBitsMask = (1 << ((B % sizeof(T)) * 8)) - 1;
      return ~kUsedBitsMask | val;
    }
    return val;
  }
};

template <typename T,
          unsigned int B = sizeof(T),
          bool is_signed = std::numeric_limits<T>::is_signed>
class ByteWriter;

template <typename T, unsigned int B>
class ByteWriter<T, B, false> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    static_assert(B <= sizeof(T), kSizeErrorMsg);
    for (unsigned int i = 0; i < B; ++i) {
      data[i] = val >> ((B - 1 - i) * 8);
    }
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    static_assert(B <= sizeof(T), kSizeErrorMsg);
    for (unsigned int i = 0; i < B; ++i) {
      data[i] = val >> (i * 8);
    }
  }
};

template <typename T, unsigned int B>
class ByteWriter<T, B, true> {
 public:
  typedef typename UnsignedOf<T>::Type U;

  static void WriteBigEndian(uint8_t* data, T val) {
    ByteWriter<U, B, false>::WriteBigEndian(data, ReinterpretAsUnsigned(val));
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    ByteWriter<U, B, false>::WriteLittleEndian(data,
                                               ReinterpretAsUnsigned(val));
  }

 private:
  static U ReinterpretAsUnsigned(T val) {

    return static_cast<U>(val);
  }
};

template <>
struct UnsignedOf<int8_t> {
  typedef uint8_t Type;
};
template <>
struct UnsignedOf<int16_t> {
  typedef uint16_t Type;
};
template <>
struct UnsignedOf<int32_t> {
  typedef uint32_t Type;
};
template <>
struct UnsignedOf<int64_t> {
  typedef uint64_t Type;
};

template <typename T>
class ByteReader<T, 1, false> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    static_assert(sizeof(T) == 1, kSizeErrorMsg);
    return data[0];
  }

  static T ReadLittleEndian(const uint8_t* data) {
    static_assert(sizeof(T) == 1, kSizeErrorMsg);
    return data[0];
  }
};

template <typename T>
class ByteWriter<T, 1, false> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) == 1, kSizeErrorMsg);
    data[0] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) == 1, kSizeErrorMsg);
    data[0] = val;
  }
};

template <typename T>
class ByteReader<T, 2, false> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 2, kSizeErrorMsg);
    return (data[0] << 8) | data[1];
  }

  static T ReadLittleEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 2, kSizeErrorMsg);
    return data[0] | (data[1] << 8);
  }
};

template <typename T>
class ByteWriter<T, 2, false> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 2, kSizeErrorMsg);
    data[0] = val >> 8;
    data[1] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 2, kSizeErrorMsg);
    data[0] = val;
    data[1] = val >> 8;
  }
};

template <typename T>
class ByteReader<T, 4, false> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 4, kSizeErrorMsg);
    return (Get(data, 0) << 24) | (Get(data, 1) << 16) | (Get(data, 2) << 8) |
           Get(data, 3);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 4, kSizeErrorMsg);
    return Get(data, 0) | (Get(data, 1) << 8) | (Get(data, 2) << 16) |
           (Get(data, 3) << 24);
  }

 private:
  inline static T Get(const uint8_t* data, unsigned int index) {
    return static_cast<T>(data[index]);
  }
};

template <typename T>
class ByteWriter<T, 4, false> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 4, kSizeErrorMsg);
    data[0] = val >> 24;
    data[1] = val >> 16;
    data[2] = val >> 8;
    data[3] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 4, kSizeErrorMsg);
    data[0] = val;
    data[1] = val >> 8;
    data[2] = val >> 16;
    data[3] = val >> 24;
  }
};

template <typename T>
class ByteReader<T, 8, false> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 8, kSizeErrorMsg);
    return (Get(data, 0) << 56) | (Get(data, 1) << 48) | (Get(data, 2) << 40) |
           (Get(data, 3) << 32) | (Get(data, 4) << 24) | (Get(data, 5) << 16) |
           (Get(data, 6) << 8) | Get(data, 7);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    static_assert(sizeof(T) >= 8, kSizeErrorMsg);
    return Get(data, 0) | (Get(data, 1) << 8) | (Get(data, 2) << 16) |
           (Get(data, 3) << 24) | (Get(data, 4) << 32) | (Get(data, 5) << 40) |
           (Get(data, 6) << 48) | (Get(data, 7) << 56);
  }

 private:
  inline static T Get(const uint8_t* data, unsigned int index) {
    return static_cast<T>(data[index]);
  }
};

template <typename T>
class ByteWriter<T, 8, false> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 8, kSizeErrorMsg);
    data[0] = val >> 56;
    data[1] = val >> 48;
    data[2] = val >> 40;
    data[3] = val >> 32;
    data[4] = val >> 24;
    data[5] = val >> 16;
    data[6] = val >> 8;
    data[7] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    static_assert(sizeof(T) >= 8, kSizeErrorMsg);
    data[0] = val;
    data[1] = val >> 8;
    data[2] = val >> 16;
    data[3] = val >> 24;
    data[4] = val >> 32;
    data[5] = val >> 40;
    data[6] = val >> 48;
    data[7] = val >> 56;
  }
};

}

#endif
