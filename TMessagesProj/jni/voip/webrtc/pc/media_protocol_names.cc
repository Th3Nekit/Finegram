/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/media_protocol_names.h"

#include <ctype.h>
#include <stddef.h>

#include <string>

namespace cricket {

const char kMediaProtocolRtpPrefix[] = "RTP/";

const char kMediaProtocolSctp[] = "SCTP";
const char kMediaProtocolUdpDtlsSctp[] = "UDP/DTLS/SCTP";
const char kMediaProtocolDtlsSctp[] = "DTLS/SCTP";
const char kMediaProtocolTcpDtlsSctp[] = "TCP/DTLS/SCTP";

const char kMediaProtocolDtlsSavpf[] = "UDP/TLS/RTP/SAVPF";
const char kMediaProtocolSavpf[] = "RTP/SAVPF";
const char kMediaProtocolAvpf[] = "RTP/AVPF";

namespace {

const char kMediaProtocolSavp[] = "RTP/SAVP";
const char kMediaProtocolAvp[] = "RTP/AVP";

const char kMediaProtocolTcpTlsSavpf[] = "TCP/TLS/RTP/SAVPF";
const char kMediaProtocolUdpTlsSavpf[] = "UDP/TLS/RTP/SAVPF";
const char kMediaProtocolTcpTlsSavp[] = "TCP/TLS/RTP/SAVP";
const char kMediaProtocolUdpTlsSavp[] = "UDP/TLS/RTP/SAVP";

}

bool IsDtlsSctp(absl::string_view protocol) {
  return protocol == kMediaProtocolDtlsSctp ||
         protocol == kMediaProtocolUdpDtlsSctp ||
         protocol == kMediaProtocolTcpDtlsSctp;
}

bool IsPlainSctp(absl::string_view protocol) {
  return protocol == kMediaProtocolSctp;
}

bool IsSctpProtocol(absl::string_view protocol) {
  return IsPlainSctp(protocol) || IsDtlsSctp(protocol);
}

bool IsRtpProtocol(absl::string_view protocol) {
  if (protocol.empty()) {
    return true;
  }
  size_t pos = protocol.find(cricket::kMediaProtocolRtpPrefix);
  if (pos == std::string::npos) {
    return false;
  }

  if (pos == 0 || !isalpha(static_cast<unsigned char>(protocol[pos - 1]))) {
    return true;
  }
  return false;
}

bool IsDtlsRtp(absl::string_view protocol) {

  return protocol == kMediaProtocolDtlsSavpf ||
         protocol == kMediaProtocolTcpTlsSavpf ||
         protocol == kMediaProtocolUdpTlsSavpf ||
         protocol == kMediaProtocolUdpTlsSavp ||
         protocol == kMediaProtocolTcpTlsSavp;
}

bool IsPlainRtp(absl::string_view protocol) {

  return protocol == kMediaProtocolSavpf || protocol == kMediaProtocolAvpf ||
         protocol == kMediaProtocolSavp || protocol == kMediaProtocolAvp;
}

}
