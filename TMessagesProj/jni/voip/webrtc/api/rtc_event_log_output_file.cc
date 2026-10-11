/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/rtc_event_log_output_file.h"

#include <limits>
#include <utility>

#include "api/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const size_t RtcEventLogOutputFile::kMaxReasonableFileSize =
    std::numeric_limits<size_t>::max() / 2;

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name)
    : RtcEventLogOutputFile(FileWrapper::OpenWriteOnly(file_name),
                            RtcEventLog::kUnlimitedOutput) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(const std::string& file_name,
                                             size_t max_size_bytes)

    : RtcEventLogOutputFile(FileWrapper::OpenWriteOnly(file_name),
                            max_size_bytes) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(FILE* file, size_t max_size_bytes)
    : RtcEventLogOutputFile(FileWrapper(file), max_size_bytes) {}

RtcEventLogOutputFile::RtcEventLogOutputFile(FileWrapper file,
                                             size_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), file_(std::move(file)) {
  RTC_CHECK_LE(max_size_bytes_, kMaxReasonableFileSize);
  if (!file_.is_open()) {
    RTC_LOG(LS_ERROR) << "Invalid file. WebRTC event log not started.";
  }
}

bool RtcEventLogOutputFile::IsActive() const {
  return IsActiveInternal();
}

bool RtcEventLogOutputFile::Write(absl::string_view output) {
  RTC_DCHECK(IsActiveInternal());

  RTC_DCHECK_LT(output.size(), kMaxReasonableFileSize);

  if (max_size_bytes_ == RtcEventLog::kUnlimitedOutput ||
      written_bytes_ + output.size() <= max_size_bytes_) {
    if (file_.Write(output.data(), output.size())) {
      written_bytes_ += output.size();
      return true;
    } else {
      RTC_LOG(LS_ERROR) << "Write to WebRtcEventLog file failed.";
    }
  } else {
    RTC_LOG(LS_VERBOSE) << "Max file size reached.";
  }

  file_.Close();
  return false;
}

bool RtcEventLogOutputFile::IsActiveInternal() const {
  return file_.is_open();
}

}
