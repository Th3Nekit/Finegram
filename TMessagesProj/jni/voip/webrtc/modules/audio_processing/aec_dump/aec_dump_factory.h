/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_FACTORY_H_
#define MODULES_AUDIO_PROCESSING_AEC_DUMP_AEC_DUMP_FACTORY_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT AecDumpFactory {
 public:

  static absl::Nullable<std::unique_ptr<AecDump>> Create(
      FileWrapper file,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue);
  static absl::Nullable<std::unique_ptr<AecDump>> Create(
      absl::string_view file_name,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue);
  static absl::Nullable<std::unique_ptr<AecDump>> Create(
      absl::Nonnull<FILE*> handle,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue);
};

}

#endif
