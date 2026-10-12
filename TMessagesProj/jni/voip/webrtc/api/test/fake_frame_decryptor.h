/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_FRAME_DECRYPTOR_H_
#define API_TEST_FAKE_FRAME_DECRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/array_view.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/media_types.h"

namespace webrtc {

class FakeFrameDecryptor : public FrameDecryptorInterface {
 public:

  explicit FakeFrameDecryptor(uint8_t fake_key = 0xAA,
                              uint8_t expected_postfix_byte = 255);

  Result Decrypt(cricket::MediaType media_type,
                 const std::vector<uint32_t>& csrcs,
                 rtc::ArrayView<const uint8_t> additional_data,
                 rtc::ArrayView<const uint8_t> encrypted_frame,
                 rtc::ArrayView<uint8_t> frame) override;

  size_t GetMaxPlaintextByteSize(cricket::MediaType media_type,
                                 size_t encrypted_frame_size) override;

  void SetFakeKey(uint8_t fake_key);

  uint8_t GetFakeKey() const;

  void SetExpectedPostfixByte(uint8_t expected_postfix_byte);

  uint8_t GetExpectedPostfixByte() const;

  void SetFailDecryption(bool fail_decryption);

  enum class FakeDecryptStatus : int {
    OK = 0,
    FORCED_FAILURE = 1,
    INVALID_POSTFIX = 2
  };

 private:
  uint8_t fake_key_ = 0;
  uint8_t expected_postfix_byte_ = 0;
  bool fail_decryption_ = false;
};

}

#endif
