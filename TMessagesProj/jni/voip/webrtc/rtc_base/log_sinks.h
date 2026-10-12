/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_LOG_SINKS_H_
#define RTC_BASE_LOG_SINKS_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/file_rotating_stream.h"
#include "rtc_base/logging.h"

namespace rtc {

class FileRotatingLogSink : public LogSink {
 public:

  FileRotatingLogSink(absl::string_view log_dir_path,
                      absl::string_view log_prefix,
                      size_t max_log_size,
                      size_t num_log_files);
  ~FileRotatingLogSink() override;

  FileRotatingLogSink(const FileRotatingLogSink&) = delete;
  FileRotatingLogSink& operator=(const FileRotatingLogSink&) = delete;

  void OnLogMessage(const std::string& message) override;
  void OnLogMessage(absl::string_view message) override;
  void OnLogMessage(const std::string& message,
                    LoggingSeverity sev,
                    const char* tag) override;
  void OnLogMessage(absl::string_view message,
                    LoggingSeverity sev,
                    const char* tag) override;

  virtual bool Init();

  bool DisableBuffering();

 protected:
  explicit FileRotatingLogSink(FileRotatingStream* stream);

 private:
  std::unique_ptr<FileRotatingStream> stream_;
};

class CallSessionFileRotatingLogSink : public FileRotatingLogSink {
 public:
  CallSessionFileRotatingLogSink(absl::string_view log_dir_path,
                                 size_t max_total_log_size);
  ~CallSessionFileRotatingLogSink() override;

  CallSessionFileRotatingLogSink(const CallSessionFileRotatingLogSink&) =
      delete;
  CallSessionFileRotatingLogSink& operator=(
      const CallSessionFileRotatingLogSink&) = delete;
};

}

#endif
