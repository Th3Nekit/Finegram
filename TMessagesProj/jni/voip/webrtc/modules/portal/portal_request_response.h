/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PORTAL_PORTAL_REQUEST_RESPONSE_H_
#define MODULES_PORTAL_PORTAL_REQUEST_RESPONSE_H_

namespace webrtc {
namespace xdg_portal {

enum class RequestResponse {

  kUnknown,

  kSuccess,

  kUserCancelled,

  kError,

  kMaxValue = kError,
};

}
}
#endif
