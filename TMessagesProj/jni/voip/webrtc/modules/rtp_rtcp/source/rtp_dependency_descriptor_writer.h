/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_DEPENDENCY_DESCRIPTOR_WRITER_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "api/array_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/bit_buffer.h"

namespace webrtc {
class RtpDependencyDescriptorWriter {
 public:

  RtpDependencyDescriptorWriter(rtc::ArrayView<uint8_t> data,
                                const FrameDependencyStructure& structure,
                                std::bitset<32> active_chains,
                                const DependencyDescriptor& descriptor);

  bool Write();

  int ValueSizeBits() const;

 private:

  using TemplateIterator = std::vector<FrameDependencyTemplate>::const_iterator;
  struct TemplateMatch {
    TemplateIterator template_position;
    bool need_custom_dtis;
    bool need_custom_fdiffs;
    bool need_custom_chains;

    int extra_size_bits;
  };
  int StructureSizeBits() const;
  TemplateMatch CalculateMatch(TemplateIterator frame_template) const;
  void FindBestTemplate();
  bool ShouldWriteActiveDecodeTargetsBitmask() const;
  bool HasExtendedFields() const;
  uint64_t TemplateId() const;

  void WriteBits(uint64_t val, size_t bit_count);
  void WriteNonSymmetric(uint32_t value, uint32_t num_values);

  void WriteTemplateDependencyStructure();
  void WriteTemplateLayers();
  void WriteTemplateDtis();
  void WriteTemplateFdiffs();
  void WriteTemplateChains();
  void WriteResolutions();

  void WriteMandatoryFields();
  void WriteExtendedFields();
  void WriteFrameDependencyDefinition();

  void WriteFrameDtis();
  void WriteFrameFdiffs();
  void WriteFrameChains();

  bool build_failed_ = false;
  const DependencyDescriptor& descriptor_;
  const FrameDependencyStructure& structure_;
  std::bitset<32> active_chains_;
  rtc::BitBufferWriter bit_writer_;
  TemplateMatch best_template_;
};

}

#endif
