/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CRYPTO_FRAME_DECRYPTOR_INTERFACE_H_
#define API_CRYPTO_FRAME_DECRYPTOR_INTERFACE_H_

#include <vector>

#include "api/array_view.h"
#include "api/media_types.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class FrameDecryptorInterface : public rtc::RefCountInterface {
 public:

  enum class Status { kOk, kRecoverable, kFailedToDecrypt, kUnknown };

  struct Result {
    Result(Status status, size_t bytes_written)
        : status(status), bytes_written(bytes_written) {}

    bool IsOk() const { return status == Status::kOk; }

    const Status status;
    const size_t bytes_written;
  };

  ~FrameDecryptorInterface() override {}

  virtual Result Decrypt(cricket::MediaType media_type,
                         const std::vector<uint32_t>& csrcs,
                         rtc::ArrayView<const uint8_t> additional_data,
                         rtc::ArrayView<const uint8_t> encrypted_frame,
                         rtc::ArrayView<uint8_t> frame) = 0;

  virtual size_t GetMaxPlaintextByteSize(cricket::MediaType media_type,
                                         size_t encrypted_frame_size) = 0;
};

}

#endif
