/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_DEFAULT_ICE_TRANSPORT_FACTORY_H_
#define P2P_BASE_DEFAULT_ICE_TRANSPORT_FACTORY_H_

#include <memory>
#include <string>

#include "api/ice_transport_interface.h"
#include "p2p/base/p2p_transport_channel.h"
#include "rtc_base/thread.h"

namespace webrtc {

class DefaultIceTransport : public IceTransportInterface {
 public:
  explicit DefaultIceTransport(
      std::unique_ptr<cricket::P2PTransportChannel> internal);
  ~DefaultIceTransport();

  cricket::IceTransportInternal* internal() override {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    return internal_.get();
  }

 private:
  const SequenceChecker thread_checker_{};
  std::unique_ptr<cricket::P2PTransportChannel> internal_
      RTC_GUARDED_BY(thread_checker_);
};

class DefaultIceTransportFactory : public IceTransportFactory {
 public:
  DefaultIceTransportFactory() = default;
  ~DefaultIceTransportFactory() = default;

  rtc::scoped_refptr<IceTransportInterface> CreateIceTransport(
      const std::string& transport_name,
      int component,
      IceTransportInit init) override;
};

}

#endif
