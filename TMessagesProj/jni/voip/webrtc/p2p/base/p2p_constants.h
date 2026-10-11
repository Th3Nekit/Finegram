/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_P2P_CONSTANTS_H_
#define P2P_BASE_P2P_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "rtc_base/system/rtc_export.h"

namespace cricket {

extern const char CN_AUDIO[];
extern const char CN_VIDEO[];
extern const char CN_DATA[];
extern const char CN_OTHER[];

extern const char GROUP_TYPE_BUNDLE[];

RTC_EXPORT extern const int ICE_UFRAG_LENGTH;
RTC_EXPORT extern const int ICE_PWD_LENGTH;
extern const size_t ICE_UFRAG_MIN_LENGTH;
extern const size_t ICE_PWD_MIN_LENGTH;
extern const size_t ICE_UFRAG_MAX_LENGTH;
extern const size_t ICE_PWD_MAX_LENGTH;

RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_RTP;
RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_RTCP;
RTC_EXPORT extern const int ICE_CANDIDATE_COMPONENT_DEFAULT;

extern const char CONNECTIONROLE_ACTIVE_STR[];
extern const char CONNECTIONROLE_PASSIVE_STR[];
extern const char CONNECTIONROLE_ACTPASS_STR[];
extern const char CONNECTIONROLE_HOLDCONN_STR[];

extern const char LOCAL_TLD[];

extern const int RECEIVING_TIMEOUT;

extern const int MIN_CHECK_RECEIVING_INTERVAL;

extern const int STRONG_PING_INTERVAL;

extern const int WEAK_PING_INTERVAL;

extern const int STRONG_AND_STABLE_WRITABLE_CONNECTION_PING_INTERVAL;

extern const int WEAK_OR_STABILIZING_WRITABLE_CONNECTION_PING_INTERVAL;

extern const int BACKUP_CONNECTION_PING_INTERVAL;

extern const int RECEIVING_SWITCHING_DELAY;

extern const int REGATHER_ON_FAILED_NETWORKS_INTERVAL;

extern const int CONNECTION_WRITE_CONNECT_TIMEOUT;

extern const uint32_t CONNECTION_WRITE_CONNECT_FAILURES;

extern const int CONNECTION_WRITE_TIMEOUT;

extern const int STUN_KEEPALIVE_INTERVAL;

static const int MIN_PINGS_AT_WEAK_PING_INTERVAL = 3;

extern const int WEAK_CONNECTION_RECEIVE_TIMEOUT;

extern const int DEAD_CONNECTION_RECEIVE_TIMEOUT;

extern const int CONNECTION_RESPONSE_TIMEOUT;

extern const int MIN_CONNECTION_LIFETIME;

enum IcePriorityValue : uint8_t {
  ICE_TYPE_PREFERENCE_RELAY_TLS = 0,
  ICE_TYPE_PREFERENCE_RELAY_TCP = 1,
  ICE_TYPE_PREFERENCE_RELAY_UDP = 2,
  ICE_TYPE_PREFERENCE_PRFLX_TCP = 80,
  ICE_TYPE_PREFERENCE_HOST_TCP = 90,
  ICE_TYPE_PREFERENCE_SRFLX = 100,
  ICE_TYPE_PREFERENCE_PRFLX = 110,
  ICE_TYPE_PREFERENCE_HOST = 126
};

}

#endif
