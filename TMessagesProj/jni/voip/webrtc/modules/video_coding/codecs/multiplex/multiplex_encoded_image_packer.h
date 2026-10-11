/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_MULTIPLEX_MULTIPLEX_ENCODED_IMAGE_PACKER_H_
#define MODULES_VIDEO_CODING_CODECS_MULTIPLEX_MULTIPLEX_ENCODED_IMAGE_PACKER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "api/video/encoded_image.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

struct MultiplexImageHeader {

  uint8_t component_count;

  uint16_t image_index;

  uint32_t first_component_header_offset;

  uint32_t augmenting_data_offset;

  uint16_t augmenting_data_size;
};
const int kMultiplexImageHeaderSize =
    sizeof(uint8_t) + 2 * sizeof(uint16_t) + 2 * sizeof(uint32_t);

struct MultiplexImageComponentHeader {

  uint32_t next_component_header_offset;

  uint8_t component_index;

  uint32_t bitstream_offset;

  uint32_t bitstream_length;

  VideoCodecType codec_type;

  VideoFrameType frame_type;
};
const int kMultiplexImageComponentHeaderSize =
    sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) +
    sizeof(uint8_t) + sizeof(uint8_t);

struct MultiplexImageComponent {

  VideoCodecType codec_type;

  uint8_t component_index;

  EncodedImage encoded_image;
};

struct MultiplexImage {
  uint16_t image_index;
  uint8_t component_count;
  uint16_t augmenting_data_size;
  std::unique_ptr<uint8_t[]> augmenting_data;
  std::vector<MultiplexImageComponent> image_components;

  MultiplexImage(uint16_t picture_index,
                 uint8_t component_count,
                 std::unique_ptr<uint8_t[]> augmenting_data,
                 uint16_t augmenting_data_size);
};

class MultiplexEncodedImagePacker {
 public:

  static EncodedImage PackAndRelease(const MultiplexImage& image);

  static MultiplexImage Unpack(const EncodedImage& combined_image);
};

}

#endif
