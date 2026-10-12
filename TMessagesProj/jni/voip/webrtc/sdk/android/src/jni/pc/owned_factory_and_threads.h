/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_OWNED_FACTORY_AND_THREADS_H_
#define SDK_ANDROID_SRC_JNI_PC_OWNED_FACTORY_AND_THREADS_H_

#include <jni.h>

#include <memory>
#include <utility>

#include "api/peer_connection_interface.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace jni {

class OwnedFactoryAndThreads {
 public:
  OwnedFactoryAndThreads(
      std::unique_ptr<rtc::SocketFactory> socket_factory,
      std::unique_ptr<rtc::Thread> network_thread,
      std::unique_ptr<rtc::Thread> worker_thread,
      std::unique_ptr<rtc::Thread> signaling_thread,
      const rtc::scoped_refptr<PeerConnectionFactoryInterface>& factory);

  ~OwnedFactoryAndThreads() = default;

  PeerConnectionFactoryInterface* factory() { return factory_.get(); }
  rtc::SocketFactory* socket_factory() { return socket_factory_.get(); }
  rtc::Thread* network_thread() { return network_thread_.get(); }
  rtc::Thread* signaling_thread() { return signaling_thread_.get(); }
  rtc::Thread* worker_thread() { return worker_thread_.get(); }

 private:

  const std::unique_ptr<rtc::SocketFactory> socket_factory_;
  const std::unique_ptr<rtc::Thread> network_thread_;
  const std::unique_ptr<rtc::Thread> worker_thread_;
  const std::unique_ptr<rtc::Thread> signaling_thread_;
  const rtc::scoped_refptr<PeerConnectionFactoryInterface> factory_;
};

}
}

#endif
