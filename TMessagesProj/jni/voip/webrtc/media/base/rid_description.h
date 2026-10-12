/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_RID_DESCRIPTION_H_
#define MEDIA_BASE_RID_DESCRIPTION_H_

#include <map>
#include <string>
#include <vector>

namespace cricket {

enum class RidDirection { kSend, kReceive };

struct RidDescription final {
  RidDescription();
  RidDescription(const std::string& rid, RidDirection direction);
  RidDescription(const RidDescription& other);
  ~RidDescription();
  RidDescription& operator=(const RidDescription& other);

  bool operator==(const RidDescription& other) const;
  bool operator!=(const RidDescription& other) const {
    return !(*this == other);
  }

  std::string rid;

  RidDirection direction;

  std::vector<int> payload_types;

  std::map<std::string, std::string> restrictions;
};

}

#endif
