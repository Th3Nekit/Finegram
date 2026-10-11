/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SYSTEM_FILE_WRAPPER_H_
#define RTC_BASE_SYSTEM_FILE_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <string>

#include "absl/strings/string_view.h"

namespace webrtc {

class FileWrapper final {
 public:

  static FileWrapper OpenReadOnly(absl::string_view file_name_utf8);
  static FileWrapper OpenWriteOnly(absl::string_view file_name_utf8,
                                   int* error = nullptr);

  FileWrapper() = default;

  explicit FileWrapper(FILE* file) : file_(file) {}
  ~FileWrapper() { Close(); }

  FileWrapper(const FileWrapper&) = delete;
  FileWrapper& operator=(const FileWrapper&) = delete;

  FileWrapper(FileWrapper&&);
  FileWrapper& operator=(FileWrapper&&);

  bool is_open() const { return file_ != nullptr; }

  bool Close();

  FILE* Release();

  bool Flush();

  bool Rewind() { return SeekTo(0); }

  bool SeekRelative(int64_t offset);

  bool SeekTo(int64_t position);

  long FileSize();

  size_t Read(void* buf, size_t length);

  bool ReadEof() const;

  bool Write(const void* buf, size_t length);

 private:
  FILE* file_ = nullptr;
};

}

#endif
