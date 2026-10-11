/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_HDR_METADATA_H_
#define API_VIDEO_HDR_METADATA_H_

namespace webrtc {

struct HdrMasteringMetadata {
  struct Chromaticity {
    Chromaticity();

    bool operator==(const Chromaticity& rhs) const {
      return x == rhs.x && y == rhs.y;
    }

    bool Validate() const {
      return x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0;
    }

    float x = 0.0f;
    float y = 0.0f;
  };

  HdrMasteringMetadata();

  bool operator==(const HdrMasteringMetadata& rhs) const {
    return ((primary_r == rhs.primary_r) && (primary_g == rhs.primary_g) &&
            (primary_b == rhs.primary_b) && (white_point == rhs.white_point) &&
            (luminance_max == rhs.luminance_max) &&
            (luminance_min == rhs.luminance_min));
  }

  bool Validate() const {
    return luminance_max >= 0.0 && luminance_max <= 20000.0 &&
           luminance_min >= 0.0 && luminance_min <= 5.0 &&
           primary_r.Validate() && primary_g.Validate() &&
           primary_b.Validate() && white_point.Validate();
  }

  Chromaticity primary_r;
  Chromaticity primary_g;
  Chromaticity primary_b;

  Chromaticity white_point;

  float luminance_max = 0.0f;

  float luminance_min = 0.0f;
};

struct HdrMetadata {
  HdrMetadata();

  bool operator==(const HdrMetadata& rhs) const {
    return (
        (max_content_light_level == rhs.max_content_light_level) &&
        (max_frame_average_light_level == rhs.max_frame_average_light_level) &&
        (mastering_metadata == rhs.mastering_metadata));
  }

  bool Validate() const {
    return max_content_light_level >= 0 && max_content_light_level <= 20000 &&
           max_frame_average_light_level >= 0 &&
           max_frame_average_light_level <= 20000 &&
           mastering_metadata.Validate();
  }

  HdrMasteringMetadata mastering_metadata;

  int max_content_light_level = 0;

  int max_frame_average_light_level = 0;
};

}

#endif
