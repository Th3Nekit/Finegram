/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_EXTERNAL_HMAC_H_
#define PC_EXTERNAL_HMAC_H_

#include <stdint.h>

#include "third_party/libsrtp/crypto/include/crypto_types.h"
#include "third_party/libsrtp/include/srtp.h"
#include "third_party/libsrtp/include/srtp_priv.h"

#define EXTERNAL_HMAC_SHA1 SRTP_HMAC_SHA1 + 1
#define HMAC_KEY_LENGTH 20

typedef struct {
  uint8_t key[HMAC_KEY_LENGTH];
  int key_length;
} ExternalHmacContext;

srtp_err_status_t external_hmac_alloc(srtp_auth_t** a,
                                      int key_len,
                                      int out_len);

srtp_err_status_t external_hmac_dealloc(srtp_auth_t* a);

srtp_err_status_t external_hmac_init(void* state,
                                     const uint8_t* key,
                                     int key_len);

srtp_err_status_t external_hmac_start(void* state);

srtp_err_status_t external_hmac_update(void* state,
                                       const uint8_t* message,
                                       int msg_octets);

srtp_err_status_t external_hmac_compute(void* state,
                                        const uint8_t* message,
                                        int msg_octets,
                                        int tag_len,
                                        uint8_t* result);

srtp_err_status_t external_crypto_init();

#endif
