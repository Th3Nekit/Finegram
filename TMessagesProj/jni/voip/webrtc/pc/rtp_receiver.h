/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpReceiverInterface.
// An RtpReceiver associates a MediaStreamTrackInterface with an underlying
// transport (provided by cricket::VoiceChannel/cricket::VideoChannel)

#ifndef PC_RTP_RECEIVER_H_
#define PC_RTP_RECEIVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/dtls_transport_interface.h"
#include "api/media_stream_interface.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "media/base/media_channel.h"
#include "media/base/video_broadcaster.h"
#include "pc/video_track_source.h"
#include "rtc_base/thread.h"

namespace webrtc {

class RtpReceiverInternal : public RtpReceiverInterface {
 public:

  virtual void Stop() = 0;

  virtual void SetMediaChannel(
      cricket::MediaReceiveChannelInterface* media_channel) = 0;

  virtual void SetupMediaChannel(uint32_t ssrc) = 0;

  virtual void SetupUnsignaledMediaChannel() = 0;

  virtual void set_transport(
      rtc::scoped_refptr<DtlsTransportInterface> dtls_transport) = 0;

  virtual absl::optional<uint32_t> ssrc() const = 0;

  virtual void NotifyFirstPacketReceived() = 0;

  virtual void set_stream_ids(std::vector<std::string> stream_ids) = 0;

  virtual void SetStreams(
      const std::vector<rtc::scoped_refptr<MediaStreamInterface>>& streams) = 0;

  virtual int AttachmentId() const = 0;

 protected:
  static int GenerateUniqueId();

  static std::vector<rtc::scoped_refptr<MediaStreamInterface>>
  CreateStreamsFromIds(std::vector<std::string> stream_ids);
};

}

#endif
