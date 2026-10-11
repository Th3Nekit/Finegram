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

// Author: brianolson@google.com (Brian Olson)
//
// This file contains the definition for classes GzipInputStream and
// GzipOutputStream.
//
// GzipInputStream decompresses data from an underlying
// ZeroCopyInputStream and provides the decompressed data as a
// ZeroCopyInputStream.
//
// GzipOutputStream is an ZeroCopyOutputStream that compresses data to
// an underlying ZeroCopyOutputStream.

#ifndef GOOGLE_PROTOBUF_IO_GZIP_STREAM_H__
#define GOOGLE_PROTOBUF_IO_GZIP_STREAM_H__

#include <zlib.h>

#include <google/protobuf/io/zero_copy_stream.h>

namespace google {
namespace protobuf {
namespace io {

class LIBPROTOBUF_EXPORT GzipInputStream : public ZeroCopyInputStream {
 public:

  enum Format {

    AUTO = 0,

    GZIP = 1,

    ZLIB = 2,
  };

  explicit GzipInputStream(
      ZeroCopyInputStream* sub_stream,
      Format format = AUTO,
      int buffer_size = -1);
  virtual ~GzipInputStream();

  inline const char* ZlibErrorMessage() const {
    return zcontext_.msg;
  }
  inline int ZlibErrorCode() const {
    return zerror_;
  }

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:
  Format format_;

  ZeroCopyInputStream* sub_stream_;

  z_stream zcontext_;
  int zerror_;

  void* output_buffer_;
  void* output_position_;
  size_t output_buffer_length_;

  int Inflate(int flush);
  void DoNextOutput(const void** data, int* size);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(GzipInputStream);
};

class LIBPROTOBUF_EXPORT GzipOutputStream : public ZeroCopyOutputStream {
 public:

  enum Format {

    GZIP = 1,

    ZLIB = 2,
  };

  struct Options {

    Format format;

    int buffer_size;

    int compression_level;

    int compression_strategy;

    Options();
  };

  explicit GzipOutputStream(ZeroCopyOutputStream* sub_stream);

  GzipOutputStream(
      ZeroCopyOutputStream* sub_stream,
      const Options& options);

  GzipOutputStream(
      ZeroCopyOutputStream* sub_stream,
      Format format,
      int buffer_size = -1) GOOGLE_ATTRIBUTE_DEPRECATED;

  virtual ~GzipOutputStream();

  inline const char* ZlibErrorMessage() const {
    return zcontext_.msg;
  }
  inline int ZlibErrorCode() const {
    return zerror_;
  }

  bool Flush();

  bool Close();

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:
  ZeroCopyOutputStream* sub_stream_;

  void* sub_data_;
  int sub_data_size_;

  z_stream zcontext_;
  int zerror_;
  void* input_buffer_;
  size_t input_buffer_length_;

  void Init(ZeroCopyOutputStream* sub_stream, const Options& options);

  int Deflate(int flush);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(GzipOutputStream);
};

}
}

}
#endif
