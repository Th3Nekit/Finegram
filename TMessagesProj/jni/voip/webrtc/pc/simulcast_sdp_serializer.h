/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SIMULCAST_SDP_SERIALIZER_H_
#define PC_SIMULCAST_SDP_SERIALIZER_H_

#include <string>

#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "media/base/rid_description.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"

namespace webrtc {

class SimulcastSdpSerializer {
 public:

  std::string SerializeSimulcastDescription(
      const cricket::SimulcastDescription& simulcast) const;

  RTCErrorOr<cricket::SimulcastDescription> DeserializeSimulcastDescription(
      absl::string_view string) const;

  std::string SerializeRidDescription(
      const cricket::RidDescription& rid_description) const;

  RTCErrorOr<cricket::RidDescription> DeserializeRidDescription(
      absl::string_view string) const;
};

}

#endif
