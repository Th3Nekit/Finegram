/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SRTP_SESSION_H_
#define PC_SRTP_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "api/field_trials_view.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "rtc_base/synchronization/mutex.h"

struct srtp_event_data_t;
struct srtp_ctx_t_;

namespace cricket {

void ProhibitLibsrtpInitialization();

class SrtpSession {
 public:
  SrtpSession();
  explicit SrtpSession(const webrtc::FieldTrialsView& field_trials);
  ~SrtpSession();

  SrtpSession(const SrtpSession&) = delete;
  SrtpSession& operator=(const SrtpSession&) = delete;

  bool SetSend(int crypto_suite,
               const uint8_t* key,
               size_t len,
               const std::vector<int>& extension_ids);
  bool UpdateSend(int crypto_suite,
                  const uint8_t* key,
                  size_t len,
                  const std::vector<int>& extension_ids);

  bool SetRecv(int crypto_suite,
               const uint8_t* key,
               size_t len,
               const std::vector<int>& extension_ids);
  bool UpdateRecv(int crypto_suite,
                  const uint8_t* key,
                  size_t len,
                  const std::vector<int>& extension_ids);

  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);

  bool ProtectRtp(void* data,
                  int in_len,
                  int max_len,
                  int* out_len,
                  int64_t* index);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);

  bool UnprotectRtp(void* data, int in_len, int* out_len);
  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  bool GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len);

  int GetSrtpOverhead() const;

  void EnableExternalAuth();
  bool IsExternalAuthEnabled() const;

  bool IsExternalAuthActive() const;

  bool RemoveSsrcFromSession(uint32_t ssrc);

 private:
  bool DoSetKey(int type,
                int crypto_suite,
                const uint8_t* key,
                size_t len,
                const std::vector<int>& extension_ids);
  bool SetKey(int type,
              int crypto_suite,
              const uint8_t* key,
              size_t len,
              const std::vector<int>& extension_ids);
  bool UpdateKey(int type,
                 int crypto_suite,
                 const uint8_t* key,
                 size_t len,
                 const std::vector<int>& extension_ids);

  bool GetSendStreamPacketIndex(void* data, int in_len, int64_t* index);

  void DumpPacket(const void* buf, int len, bool outbound);

  void HandleEvent(const srtp_event_data_t* ev);
  static void HandleEventThunk(srtp_event_data_t* ev);

  webrtc::SequenceChecker thread_checker_;
  srtp_ctx_t_* session_ = nullptr;

  int rtp_auth_tag_len_ = 0;
  int rtcp_auth_tag_len_ = 0;

  bool inited_ = false;
  int last_send_seq_num_ = -1;
  bool external_auth_active_ = false;
  bool external_auth_enabled_ = false;
  int decryption_failure_count_ = 0;
  bool dump_plain_rtp_ = false;
};

}

#endif
