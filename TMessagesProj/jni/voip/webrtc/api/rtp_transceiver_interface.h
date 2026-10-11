/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_TRANSCEIVER_INTERFACE_H_
#define API_RTP_TRANSCEIVER_INTERFACE_H_

#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/media_types.h"
#include "api/ref_count.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT RtpTransceiverInit final {
  RtpTransceiverInit();
  RtpTransceiverInit(const RtpTransceiverInit&);
  ~RtpTransceiverInit();

  RtpTransceiverDirection direction = RtpTransceiverDirection::kSendRecv;

  std::vector<std::string> stream_ids;

  std::vector<RtpEncodingParameters> send_encodings;
};

class RTC_EXPORT RtpTransceiverInterface : public webrtc::RefCountInterface {
 public:

  virtual cricket::MediaType media_type() const = 0;

  virtual absl::optional<std::string> mid() const = 0;

  virtual rtc::scoped_refptr<RtpSenderInterface> sender() const = 0;

  virtual rtc::scoped_refptr<RtpReceiverInterface> receiver() const = 0;

  virtual bool stopped() const = 0;

  virtual bool stopping() const = 0;

  virtual RtpTransceiverDirection direction() const = 0;

  ABSL_DEPRECATED("Use SetDirectionWithError instead")
  virtual void SetDirection(RtpTransceiverDirection new_direction);
  virtual RTCError SetDirectionWithError(RtpTransceiverDirection new_direction);

  virtual absl::optional<RtpTransceiverDirection> current_direction() const = 0;

  virtual absl::optional<RtpTransceiverDirection> fired_direction() const;

  virtual RTCError StopStandard();

  virtual void StopInternal();
  ABSL_DEPRECATED("Use StopStandard instead") virtual void Stop();

  virtual RTCError SetCodecPreferences(
      rtc::ArrayView<RtpCodecCapability> codecs) = 0;
  virtual std::vector<RtpCodecCapability> codec_preferences() const = 0;

  virtual std::vector<RtpHeaderExtensionCapability>
  GetHeaderExtensionsToNegotiate() const = 0;

  virtual std::vector<RtpHeaderExtensionCapability>
  GetNegotiatedHeaderExtensions() const = 0;

  virtual webrtc::RTCError SetHeaderExtensionsToNegotiate(
      rtc::ArrayView<const RtpHeaderExtensionCapability> header_extensions) = 0;

 protected:
  ~RtpTransceiverInterface() override = default;
};

}

#endif
