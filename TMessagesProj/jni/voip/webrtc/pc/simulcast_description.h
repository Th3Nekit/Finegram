/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SIMULCAST_DESCRIPTION_H_
#define PC_SIMULCAST_DESCRIPTION_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace cricket {

struct SimulcastLayer final {
  SimulcastLayer(absl::string_view rid, bool is_paused);

  SimulcastLayer(const SimulcastLayer& other) = default;
  SimulcastLayer& operator=(const SimulcastLayer& other) = default;
  bool operator==(const SimulcastLayer& other) const;

  std::string rid;
  bool is_paused;
};

class SimulcastLayerList final {
 public:

  typedef size_t size_type;
  typedef std::vector<SimulcastLayer> value_type;
  typedef std::vector<std::vector<SimulcastLayer>>::const_iterator
      const_iterator;

  void AddLayer(const SimulcastLayer& layer);

  void AddLayerWithAlternatives(const std::vector<SimulcastLayer>& layers);

  const_iterator begin() const { return list_.begin(); }

  const_iterator end() const { return list_.end(); }

  const std::vector<SimulcastLayer>& operator[](size_t index) const;

  size_t size() const { return list_.size(); }
  bool empty() const { return list_.empty(); }

  std::vector<SimulcastLayer> GetAllLayers() const;

 private:

  std::vector<std::vector<SimulcastLayer>> list_;
};

class SimulcastDescription final {
 public:
  const SimulcastLayerList& send_layers() const { return send_layers_; }
  SimulcastLayerList& send_layers() { return send_layers_; }

  const SimulcastLayerList& receive_layers() const { return receive_layers_; }
  SimulcastLayerList& receive_layers() { return receive_layers_; }

  bool empty() const;

 private:

  SimulcastLayerList send_layers_;
  SimulcastLayerList receive_layers_;
};

}

#endif
