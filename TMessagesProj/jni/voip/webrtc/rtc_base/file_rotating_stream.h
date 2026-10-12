/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FILE_ROTATING_STREAM_H_
#define RTC_BASE_FILE_ROTATING_STREAM_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/system/file_wrapper.h"

namespace rtc {

class FileRotatingStream {
 public:

  FileRotatingStream(absl::string_view dir_path,
                     absl::string_view file_prefix,
                     size_t max_file_size,
                     size_t num_files);

  virtual ~FileRotatingStream();

  FileRotatingStream(const FileRotatingStream&) = delete;
  FileRotatingStream& operator=(const FileRotatingStream&) = delete;

  bool IsOpen() const;

  bool Write(const void* data, size_t data_len);
  bool Flush();
  void Close();

  bool Open();

  bool DisableBuffering();

  std::string GetFilePath(size_t index) const;

  size_t GetNumFiles() const { return file_names_.size(); }

 protected:
  void SetMaxFileSize(size_t size) { max_file_size_ = size; }

  size_t GetRotationIndex() const { return rotation_index_; }

  void SetRotationIndex(size_t index) { rotation_index_ = index; }

  virtual void OnRotation() {}

 private:
  bool OpenCurrentFile();
  void CloseCurrentFile();

  void RotateFiles();

  std::string GetFilePath(size_t index, size_t num_files) const;

  const std::string dir_path_;
  const std::string file_prefix_;

  webrtc::FileWrapper file_;

  std::vector<std::string> file_names_;
  size_t max_file_size_;
  size_t current_file_index_;

  size_t rotation_index_;

  size_t current_bytes_written_;
  bool disable_buffering_;
};

class CallSessionFileRotatingStream : public FileRotatingStream {
 public:

  CallSessionFileRotatingStream(absl::string_view dir_path,
                                size_t max_total_log_size);
  ~CallSessionFileRotatingStream() override {}

  CallSessionFileRotatingStream(const CallSessionFileRotatingStream&) = delete;
  CallSessionFileRotatingStream& operator=(
      const CallSessionFileRotatingStream&) = delete;

 protected:
  void OnRotation() override;

 private:
  static size_t GetRotatingLogSize(size_t max_total_log_size);
  static size_t GetNumRotatingLogFiles(size_t max_total_log_size);
  static const size_t kRotatingLogFileDefaultSize;

  const size_t max_total_log_size_;
  size_t num_rotations_;
};

class FileRotatingStreamReader {
 public:
  FileRotatingStreamReader(absl::string_view dir_path,
                           absl::string_view file_prefix);
  ~FileRotatingStreamReader();
  size_t GetSize() const;
  size_t ReadAll(void* buffer, size_t size) const;

 private:
  std::vector<std::string> file_names_;
};

class CallSessionFileRotatingStreamReader : public FileRotatingStreamReader {
 public:
  CallSessionFileRotatingStreamReader(absl::string_view dir_path);
};

}

#endif
