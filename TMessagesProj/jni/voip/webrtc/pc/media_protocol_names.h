/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_MEDIA_PROTOCOL_NAMES_H_
#define PC_MEDIA_PROTOCOL_NAMES_H_

#include "absl/strings/string_view.h"

namespace cricket {

extern const char kMediaProtocolSctp[];
extern const char kMediaProtocolUdpDtlsSctp[];
extern const char kMediaProtocolDtlsSavpf[];
extern const char kMediaProtocolSavpf[];
extern const char kMediaProtocolAvpf[];

extern const char kMediaProtocolTcpDtlsSctp[];
extern const char kMediaProtocolDtlsSctp[];

bool IsRtpProtocol(absl::string_view protocol);

bool IsSctpProtocol(absl::string_view protocol);

bool IsPlainSctp(absl::string_view protocol);

bool IsDtlsSctp(absl::string_view protocol);

bool IsPlainRtp(absl::string_view protocol);

bool IsDtlsRtp(absl::string_view protocol);

}

#endif
