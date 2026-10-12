/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ENABLE_MEDIA_H_
#define API_ENABLE_MEDIA_H_

#include "api/peer_connection_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

RTC_EXPORT void EnableMedia(PeerConnectionFactoryDependencies& deps);

}

#endif
