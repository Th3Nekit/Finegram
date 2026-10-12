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
// zero_copy_stream.h which are only included in the full (non-lite)
// protobuf library.  These implementations include Unix file descriptors
// and C++ iostreams.  See also:  zero_copy_stream_impl_lite.h

#ifndef GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_H__
#define GOOGLE_PROTOBUF_IO_ZERO_COPY_STREAM_IMPL_H__

#include <string>
#include <iosfwd>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

class LIBPROTOBUF_EXPORT FileInputStream : public ZeroCopyInputStream {
 public:

  explicit FileInputStream(int file_descriptor, int block_size = -1);
  ~FileInputStream();

  bool Close();

  void SetCloseOnDelete(bool value) { copying_input_.SetCloseOnDelete(value); }

  int GetErrno() { return copying_input_.GetErrno(); }

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:
  class LIBPROTOBUF_EXPORT CopyingFileInputStream : public CopyingInputStream {
   public:
    CopyingFileInputStream(int file_descriptor);
    ~CopyingFileInputStream();

    bool Close();
    void SetCloseOnDelete(bool value) { close_on_delete_ = value; }
    int GetErrno() { return errno_; }

    int Read(void* buffer, int size);
    int Skip(int count);

   private:

    const int file_;
    bool close_on_delete_;
    bool is_closed_;

    int errno_;

    bool previous_seek_failed_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingFileInputStream);
  };

  CopyingFileInputStream copying_input_;
  CopyingInputStreamAdaptor impl_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FileInputStream);
};

class LIBPROTOBUF_EXPORT FileOutputStream : public ZeroCopyOutputStream {
 public:

  explicit FileOutputStream(int file_descriptor, int block_size = -1);
  ~FileOutputStream();

  bool Close();

  bool Flush();

  void SetCloseOnDelete(bool value) { copying_output_.SetCloseOnDelete(value); }

  int GetErrno() { return copying_output_.GetErrno(); }

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:
  class LIBPROTOBUF_EXPORT CopyingFileOutputStream : public CopyingOutputStream {
   public:
    CopyingFileOutputStream(int file_descriptor);
    ~CopyingFileOutputStream();

    bool Close();
    void SetCloseOnDelete(bool value) { close_on_delete_ = value; }
    int GetErrno() { return errno_; }

    bool Write(const void* buffer, int size);

   private:

    const int file_;
    bool close_on_delete_;
    bool is_closed_;

    int errno_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingFileOutputStream);
  };

  CopyingFileOutputStream copying_output_;
  CopyingOutputStreamAdaptor impl_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FileOutputStream);
};

class LIBPROTOBUF_EXPORT IstreamInputStream : public ZeroCopyInputStream {
 public:

  explicit IstreamInputStream(istream* stream, int block_size = -1);
  ~IstreamInputStream();

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:
  class LIBPROTOBUF_EXPORT CopyingIstreamInputStream : public CopyingInputStream {
   public:
    CopyingIstreamInputStream(istream* input);
    ~CopyingIstreamInputStream();

    int Read(void* buffer, int size);

   private:

    istream* input_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingIstreamInputStream);
  };

  CopyingIstreamInputStream copying_input_;
  CopyingInputStreamAdaptor impl_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(IstreamInputStream);
};

class LIBPROTOBUF_EXPORT OstreamOutputStream : public ZeroCopyOutputStream {
 public:

  explicit OstreamOutputStream(ostream* stream, int block_size = -1);
  ~OstreamOutputStream();

  bool Next(void** data, int* size);
  void BackUp(int count);
  int64 ByteCount() const;

 private:
  class LIBPROTOBUF_EXPORT CopyingOstreamOutputStream : public CopyingOutputStream {
   public:
    CopyingOstreamOutputStream(ostream* output);
    ~CopyingOstreamOutputStream();

    bool Write(const void* buffer, int size);

   private:

    ostream* output_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(CopyingOstreamOutputStream);
  };

  CopyingOstreamOutputStream copying_output_;
  CopyingOutputStreamAdaptor impl_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(OstreamOutputStream);
};

class LIBPROTOBUF_EXPORT ConcatenatingInputStream : public ZeroCopyInputStream {
 public:

  ConcatenatingInputStream(ZeroCopyInputStream* const streams[], int count);
  ~ConcatenatingInputStream();

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:

  ZeroCopyInputStream* const* streams_;
  int stream_count_;
  int64 bytes_retired_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ConcatenatingInputStream);
};

class LIBPROTOBUF_EXPORT LimitingInputStream : public ZeroCopyInputStream {
 public:
  LimitingInputStream(ZeroCopyInputStream* input, int64 limit);
  ~LimitingInputStream();

  bool Next(const void** data, int* size);
  void BackUp(int count);
  bool Skip(int count);
  int64 ByteCount() const;

 private:
  ZeroCopyInputStream* input_;
  int64 limit_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(LimitingInputStream);
};

}
}

}
#endif
