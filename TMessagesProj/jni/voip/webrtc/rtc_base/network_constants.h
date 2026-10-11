/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NETWORK_CONSTANTS_H_
#define RTC_BASE_NETWORK_CONSTANTS_H_

#include <stdint.h>

#include <string>

namespace rtc {

constexpr uint16_t kNetworkCostMax = 999;
constexpr uint16_t kNetworkCostCellular2G = 980;
constexpr uint16_t kNetworkCostCellular3G = 910;
constexpr uint16_t kNetworkCostCellular = 900;
constexpr uint16_t kNetworkCostCellular4G = 500;
constexpr uint16_t kNetworkCostCellular5G = 250;
constexpr uint16_t kNetworkCostUnknown = 50;
constexpr uint16_t kNetworkCostLow = 10;
constexpr uint16_t kNetworkCostMin = 0;

constexpr uint16_t kNetworkCostVpn = 1;

constexpr uint16_t kNetworkCostHigh = kNetworkCostCellular;

enum AdapterType {

  ADAPTER_TYPE_UNKNOWN = 0,
  ADAPTER_TYPE_ETHERNET = 1 << 0,
  ADAPTER_TYPE_WIFI = 1 << 1,
  ADAPTER_TYPE_CELLULAR = 1 << 2,
  ADAPTER_TYPE_VPN = 1 << 3,
  ADAPTER_TYPE_LOOPBACK = 1 << 4,

  ADAPTER_TYPE_ANY = 1 << 5,
  ADAPTER_TYPE_CELLULAR_2G = 1 << 6,
  ADAPTER_TYPE_CELLULAR_3G = 1 << 7,
  ADAPTER_TYPE_CELLULAR_4G = 1 << 8,
  ADAPTER_TYPE_CELLULAR_5G = 1 << 9
};

std::string AdapterTypeToString(AdapterType type);

constexpr AdapterType kAllAdapterTypes[] = {
    ADAPTER_TYPE_UNKNOWN,     ADAPTER_TYPE_ETHERNET,
    ADAPTER_TYPE_WIFI,        ADAPTER_TYPE_CELLULAR,
    ADAPTER_TYPE_VPN,         ADAPTER_TYPE_LOOPBACK,
    ADAPTER_TYPE_ANY,         ADAPTER_TYPE_CELLULAR_2G,
    ADAPTER_TYPE_CELLULAR_3G, ADAPTER_TYPE_CELLULAR_4G,
    ADAPTER_TYPE_CELLULAR_5G,
};

}

#endif
