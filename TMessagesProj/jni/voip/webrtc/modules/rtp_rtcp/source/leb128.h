/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_LEB128_H_
#define MODULES_RTP_RTCP_SOURCE_LEB128_H_

#include <cstdint>

namespace webrtc {

int Leb128Size(uint64_t value);

uint64_t ReadLeb128(const uint8_t*& read_at, const uint8_t* end);

int WriteLeb128(uint64_t value, uint8_t* buffer);

}

#endif
