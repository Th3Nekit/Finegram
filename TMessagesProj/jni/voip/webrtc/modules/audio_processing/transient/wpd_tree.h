/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_WPD_TREE_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_WPD_TREE_H_

#include <stddef.h>

#include <memory>

#include "modules/audio_processing/transient/wpd_node.h"

namespace webrtc {

class WPDTree {
 public:

  WPDTree(size_t data_length,
          const float* high_pass_coefficients,
          const float* low_pass_coefficients,
          size_t coefficients_length,
          int levels);
  ~WPDTree();

  static int NumberOfNodesAtLevel(int level) { return 1 << level; }

  WPDNode* NodeAt(int level, int index);

  int Update(const float* data, size_t data_length);

  int levels() const { return levels_; }

  int num_nodes() const { return num_nodes_; }

  int num_leaves() const { return 1 << levels_; }

 private:
  size_t data_length_;
  int levels_;
  int num_nodes_;
  std::unique_ptr<std::unique_ptr<WPDNode>[]> nodes_;
};

}

#endif
