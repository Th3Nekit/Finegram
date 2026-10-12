/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_COMMON_HANDOVER_TESTING_H_
#define NET_DCSCTP_COMMON_HANDOVER_TESTING_H_

#include "net/dcsctp/public/dcsctp_handover_state.h"

namespace dcsctp {

extern void (*g_handover_state_transformer_for_test)(
    DcSctpSocketHandoverState*);
}

#endif
