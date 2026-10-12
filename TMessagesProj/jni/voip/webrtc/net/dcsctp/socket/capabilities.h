/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_CAPABILITIES_H_
#define NET_DCSCTP_SOCKET_CAPABILITIES_H_

#include <cstdint>
namespace dcsctp {

struct Capabilities {

  bool partial_reliability = false;

  bool message_interleaving = false;

  bool reconfig = false;

  bool zero_checksum = false;

  uint16_t negotiated_maximum_incoming_streams = 0;
  uint16_t negotiated_maximum_outgoing_streams = 0;
};
}

#endif
