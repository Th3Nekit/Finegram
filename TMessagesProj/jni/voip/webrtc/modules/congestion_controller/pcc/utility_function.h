/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_PCC_UTILITY_FUNCTION_H_
#define MODULES_CONGESTION_CONTROLLER_PCC_UTILITY_FUNCTION_H_

#include "modules/congestion_controller/pcc/monitor_interval.h"

namespace webrtc {
namespace pcc {

class PccUtilityFunctionInterface {
 public:
  virtual double Compute(const PccMonitorInterval& monitor_interval) const = 0;
  virtual ~PccUtilityFunctionInterface() = default;
};

class VivaceUtilityFunction : public PccUtilityFunctionInterface {
 public:
  VivaceUtilityFunction(double delay_gradient_coefficient,
                        double loss_coefficient,
                        double throughput_coefficient,
                        double throughput_power,
                        double delay_gradient_threshold,
                        double delay_gradient_negative_bound);
  double Compute(const PccMonitorInterval& monitor_interval) const override;
  ~VivaceUtilityFunction() override;

 private:
  const double delay_gradient_coefficient_;
  const double loss_coefficient_;
  const double throughput_power_;
  const double throughput_coefficient_;
  const double delay_gradient_threshold_;
  const double delay_gradient_negative_bound_;
};

class ModifiedVivaceUtilityFunction : public PccUtilityFunctionInterface {
 public:
  ModifiedVivaceUtilityFunction(double delay_gradient_coefficient,
                                double loss_coefficient,
                                double throughput_coefficient,
                                double throughput_power,
                                double delay_gradient_threshold,
                                double delay_gradient_negative_bound);
  double Compute(const PccMonitorInterval& monitor_interval) const override;
  ~ModifiedVivaceUtilityFunction() override;

 private:
  const double delay_gradient_coefficient_;
  const double loss_coefficient_;
  const double throughput_power_;
  const double throughput_coefficient_;
  const double delay_gradient_threshold_;
  const double delay_gradient_negative_bound_;
};

}
}

#endif
