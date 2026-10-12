/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_IVF_FILE_READER_H_
#define MODULES_VIDEO_CODING_UTILITY_IVF_FILE_READER_H_

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/video/encoded_image.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/system/file_wrapper.h"

namespace webrtc {

class IvfFileReader {
 public:

  static std::unique_ptr<IvfFileReader> Create(FileWrapper file);
  ~IvfFileReader();

  IvfFileReader(const IvfFileReader&) = delete;
  IvfFileReader& operator=(const IvfFileReader&) = delete;

  bool Reset();

  VideoCodecType GetVideoCodecType() const { return codec_type_; }

  size_t GetFramesCount() const { return num_frames_; }

  absl::optional<EncodedImage> NextFrame();
  bool HasMoreFrames() const { return num_read_frames_ < num_frames_; }
  bool HasError() const { return has_error_; }

  uint16_t GetFrameWidth() const { return width_; }
  uint16_t GetFrameHeight() const { return height_; }

  bool Close();

 private:
  struct FrameHeader {
    size_t frame_size;
    int64_t timestamp;
  };

  explicit IvfFileReader(FileWrapper file) : file_(std::move(file)) {}

  absl::optional<VideoCodecType> ParseCodecType(uint8_t* buffer,
                                                size_t start_pos);
  absl::optional<FrameHeader> ReadNextFrameHeader();

  VideoCodecType codec_type_;
  size_t num_frames_;
  size_t num_read_frames_;
  uint16_t width_;
  uint16_t height_;
  uint32_t time_scale_;
  FileWrapper file_;

  absl::optional<FrameHeader> next_frame_header_;
  bool has_error_;
};

}

#endif
