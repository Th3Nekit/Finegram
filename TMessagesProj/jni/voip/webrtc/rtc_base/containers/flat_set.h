/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This implementation is borrowed from Chromium.

#ifndef RTC_BASE_CONTAINERS_FLAT_SET_H_
#define RTC_BASE_CONTAINERS_FLAT_SET_H_

#include <functional>
#include <vector>

#include "rtc_base/containers/flat_tree.h"
#include "rtc_base/containers/identity.h"

namespace webrtc {

template <class Key,
          class Compare = std::less<>,
          class Container = std::vector<Key>>
using flat_set = typename ::webrtc::flat_containers_internal::
    flat_tree<Key, webrtc::identity, Compare, Container>;

using ::webrtc::flat_containers_internal::EraseIf;

}

#endif
