/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_CHUNK_VALIDATORS_H_
#define NET_DCSCTP_PACKET_CHUNK_VALIDATORS_H_

#include "net/dcsctp/packet/chunk/sack_chunk.h"

namespace dcsctp {

class ChunkValidators {
 public:

  static bool Validate(const SackChunk& sack);

  static SackChunk Clean(SackChunk&& sack);
};
}

#endif
