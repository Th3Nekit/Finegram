/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_CONFIG_H_
#define CALL_RTP_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"

namespace webrtc {

struct RtpPayloadState {
  int16_t picture_id = -1;
  uint8_t tl0_pic_idx = 0;
  int64_t shared_frame_id = 0;
};

struct LntfConfig {
  std::string ToString() const;

  bool enabled{false};
};

struct NackConfig {
  NackConfig() : rtp_history_ms(0) {}
  std::string ToString() const;

  int rtp_history_ms;
};

struct UlpfecConfig {
  UlpfecConfig()
      : ulpfec_payload_type(-1),
        red_payload_type(-1),
        red_rtx_payload_type(-1) {}
  std::string ToString() const;
  bool operator==(const UlpfecConfig& other) const;

  int ulpfec_payload_type;

  int red_payload_type;

  int red_rtx_payload_type;
};

static const size_t kDefaultMaxPacketSize = 1500 - 40;
struct RtpConfig {
  RtpConfig();
  RtpConfig(const RtpConfig&);
  ~RtpConfig();
  std::string ToString() const;

  std::vector<uint32_t> ssrcs;

  std::vector<std::string> rids;

  std::string mid;

  RtcpMode rtcp_mode = RtcpMode::kCompound;

  size_t max_packet_size = kDefaultMaxPacketSize;

  bool extmap_allow_mixed = false;

  std::vector<RtpExtension> extensions;

  std::string payload_name;
  int payload_type = -1;

  bool raw_payload = false;

  LntfConfig lntf;

  NackConfig nack;

  UlpfecConfig ulpfec;

  struct Flexfec {
    Flexfec();
    Flexfec(const Flexfec&);
    ~Flexfec();

    int payload_type = -1;

    uint32_t ssrc = 0;

    std::vector<uint32_t> protected_media_ssrcs;
  } flexfec;

  struct Rtx {
    Rtx();
    Rtx(const Rtx&);
    ~Rtx();
    std::string ToString() const;

    std::vector<uint32_t> ssrcs;

    int payload_type = -1;
  } rtx;

  std::string c_name;

  bool enable_send_packet_batching = false;

  bool IsMediaSsrc(uint32_t ssrc) const;
  bool IsRtxSsrc(uint32_t ssrc) const;
  bool IsFlexfecSsrc(uint32_t ssrc) const;
  absl::optional<uint32_t> GetRtxSsrcAssociatedWithMediaSsrc(
      uint32_t media_ssrc) const;
  uint32_t GetMediaSsrcAssociatedWithRtxSsrc(uint32_t rtx_ssrc) const;
  uint32_t GetMediaSsrcAssociatedWithFlexfecSsrc(uint32_t flexfec_ssrc) const;
  absl::optional<std::string> GetRidForSsrc(uint32_t ssrc) const;
};
}
#endif
