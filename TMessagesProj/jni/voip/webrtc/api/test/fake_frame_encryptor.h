/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_FRAME_ENCRYPTOR_H_
#define API_TEST_FAKE_FRAME_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include "api/array_view.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/media_types.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

class FakeFrameEncryptor
    : public rtc::RefCountedObject<FrameEncryptorInterface> {
 public:

  explicit FakeFrameEncryptor(uint8_t fake_key = 0xAA,
                              uint8_t postfix_byte = 255);

  int Encrypt(cricket::MediaType media_type,
              uint32_t ssrc,
              rtc::ArrayView<const uint8_t> additional_data,
              rtc::ArrayView<const uint8_t> frame,
              rtc::ArrayView<uint8_t> encrypted_frame,
              size_t* bytes_written) override;

  size_t GetMaxCiphertextByteSize(cricket::MediaType media_type,
                                  size_t frame_size) override;

  void SetFakeKey(uint8_t fake_key);

  uint8_t GetFakeKey() const;

  void SetPostfixByte(uint8_t expected_postfix_byte);

  uint8_t GetPostfixByte() const;

  void SetFailEncryption(bool fail_encryption);

  enum class FakeEncryptionStatus : int {
    OK = 0,
    FORCED_FAILURE = 1,
  };

 private:
  uint8_t fake_key_ = 0;
  uint8_t postfix_byte_ = 0;
  bool fail_encryption_ = false;
};

}

#endif
