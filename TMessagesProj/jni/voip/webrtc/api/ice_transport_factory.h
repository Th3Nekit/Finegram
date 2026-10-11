/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ICE_TRANSPORT_FACTORY_H_
#define API_ICE_TRANSPORT_FACTORY_H_

#include "api/ice_transport_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace cricket {
class PortAllocator;
}

namespace webrtc {

RTC_EXPORT rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    cricket::PortAllocator* port_allocator);

RTC_EXPORT rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
    IceTransportInit);

}

#endif
