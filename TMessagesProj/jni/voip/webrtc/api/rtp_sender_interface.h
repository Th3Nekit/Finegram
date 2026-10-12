/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for RtpSenders
// http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface

#ifndef API_RTP_SENDER_INTERFACE_H_
#define API_RTP_SENDER_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "api/crypto/frame_encryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/frame_transformer_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

using SetParametersCallback = absl::AnyInvocable<void(RTCError) &&>;

class RTC_EXPORT RtpSenderInterface : public webrtc::RefCountInterface {
 public:

  virtual bool SetTrack(MediaStreamTrackInterface* track) = 0;
  virtual rtc::scoped_refptr<MediaStreamTrackInterface> track() const = 0;

  virtual rtc::scoped_refptr<DtlsTransportInterface> dtls_transport() const = 0;

  virtual uint32_t ssrc() const = 0;

  virtual cricket::MediaType media_type() const = 0;

  virtual std::string id() const = 0;

  virtual std::vector<std::string> stream_ids() const = 0;

  virtual void SetStreams(const std::vector<std::string>& stream_ids) = 0;

  virtual std::vector<RtpEncodingParameters> init_send_encodings() const = 0;

  virtual RtpParameters GetParameters() const = 0;

  virtual RTCError SetParameters(const RtpParameters& parameters) = 0;
  virtual void SetParametersAsync(const RtpParameters& parameters,
                                  SetParametersCallback callback);

  virtual rtc::scoped_refptr<DtmfSenderInterface> GetDtmfSender() const = 0;

  virtual void SetFrameEncryptor(
      rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor) = 0;

  virtual rtc::scoped_refptr<FrameEncryptorInterface> GetFrameEncryptor()
      const = 0;

  virtual void SetEncoderToPacketizerFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) = 0;

  virtual void SetEncoderSelector(
      std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface>
          encoder_selector) = 0;

 protected:
  ~RtpSenderInterface() override = default;
};

}

#endif
