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
// This file contains common implementations of the interfaces defined in
// zero_copy_stream.h which are included in the "lite" protobuf library.
// These implementations cover I/O on raw arrays and strings, as well as
// adaptors which make it easy to implement streams based on traditional
// streams.  Of course, many users will probably want to write their own
// implementations of these interfaces specific to the particular I/O
// abstractions they prefer to use, but these should cover the most common
// cases.

#ifndef GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_LITE_H__
#define GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_LITE_H__

#include <string>
#include <iosfwd>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

class LIBPROTOBUF_EXPORT ArrayInputStream : public ZeroCopyInputStream {
 public:

  ArrayInputStream(const void* data, int size, int block_size = -1);
  ~ArrayInputStream();

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:
  const uint8* const data_;
  const int size_;
  const int block_size_;

  int position_;
  int last_returned_size_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArrayInputStream);
};

class LIBPROTOBUF_EXPORT ArrayOutputStream : public ZeroCopyOutputStream {
 public:

  ArrayOutputStream(void* data, int size, int block_size = -1);
  ~ArrayOutputStream();

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:
  uint8* const data_;
  const int size_;
  const int block_size_;

  int position_;
  int last_returned_size_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ArrayOutputStream);
};

class LIBPROTOBUF_EXPORT StringOutputStream : public ZeroCopyOutputStream {
 public:

  explicit StringOutputStream(string* target);
  ~StringOutputStream();

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:
  static const int kMinimumSize = 16;

  string* target_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(StringOutputStream);
};

class LIBPROTOBUF_EXPORT CopyingInputStream {
 public:
  virtual ~CopyingInputStream();

  virtual int Read(void* buffer, int size) = 0;

  virtual int Skip(int count);
};

class LIBPROTOBUF_EXPORT CopyingInputStreamAdaptor : public ZeroCopyInputStream {
 public:

  explicit CopyingInputStreamAdaptor(CopyingInputStream* copying_stream,
                                     int block_size = -1);
  ~CopyingInputStreamAdaptor();

  void SetOwnsCopyingStream(bool value) { owns_copying_stream_ = value; }

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:

  void AllocateBufferIfNeeded();

  void FreeBuffer();

  CopyingInputStream* copying_stream_;
  bool owns_copying_stream_;

  bool failed_;

  int64 position_;

  scoped_array<uint8> buffer_;
  const int buffer_size_;

  int buffer_used_;

  int backup_bytes_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingInputStreamAdaptor);
};

class LIBPROTOBUF_EXPORT CopyingOutputStream {
 public:
  virtual ~CopyingOutputStream();

  virtual bool Write(const void* buffer, int size) = 0;
};

class LIBPROTOBUF_EXPORT CopyingOutputStreamAdaptor : public ZeroCopyOutputStream {
 public:

  explicit CopyingOutputStreamAdaptor(CopyingOutputStream* copying_stream,
                                      int block_size = -1);
  ~CopyingOutputStreamAdaptor();

  bool Flush();

  void SetOwnsCopyingStream(bool value) { owns_copying_stream_ = value; }

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:

  bool WriteBuffer();

  void AllocateBufferIfNeeded();

  void FreeBuffer();

  CopyingOutputStream* copying_stream_;
  bool owns_copying_stream_;

  bool failed_;

  int64 position_;

  scoped_array<uint8> buffer_;
  const int buffer_size_;

  int buffer_used_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingOutputStreamAdaptor);
};

}
}

}
#endif
