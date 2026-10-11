/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpReceivers
// http://w3c.github.io/webrtc-pc/#rtcrtpreceiver-interface

#ifndef API_RTP_RECEIVER_INTERFACE_H_
#define API_RTP_RECEIVER_INTERFACE_H_

#include <string>
#include <vector>

#include "api/crypto/frame_decryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/ref_count.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/transport/rtp/rtp_source.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RtpReceiverObserverInterface {
 public:

  virtual void OnFirstPacketReceived(cricket::MediaType media_type) = 0;

 protected:
  virtual ~RtpReceiverObserverInterface() {}
};

class RTC_EXPORT RtpReceiverInterface : public webrtc::RefCountInterface {
 public:
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;

  virtual rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const;

  virtual std::vector<std::string> stream_ids() const;
  virtual std::vector<rtc::scoped_refptr<MediaStreamInterface>> streams() const;

  virtual cricket::MediaType media_type() const = 0;

  virtual std::string id() const = 0;

  virtual RtpParameters GetParameters() const = 0;

  virtual bool SetParameters(const RtpParameters& parameters) { return false; }

  virtual void SetObserver(RtpReceiverObserverInterface* observer) = 0;

  virtual void SetJitterBufferMinimumDelay(
      absl::optional<double> delay_seconds) = 0;

  virtual std::vector<RtpSource> GetSources() const;

  virtual void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor);

  virtual rtc::scoped_refptr<FrameDecryptorInterface> GetFrameDecryptor() const;

  virtual void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);

 protected:
  ~RtpReceiverInterface() override = default;
};

}

#endif
