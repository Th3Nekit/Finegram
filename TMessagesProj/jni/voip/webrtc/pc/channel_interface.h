/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CHANNEL_INTERFACE_H_
#define PC_CHANNEL_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "media/base/media_channel.h"
#include "pc/rtp_transport_internal.h"

namespace webrtc {
class Call;
class VideoBitrateAllocatorFactory;
}

namespace cricket {

class VoiceChannel;
class VideoChannel;
class MediaContentDescription;
struct MediaConfig;

class ChannelInterface {
 public:
  virtual ~ChannelInterface() = default;
  virtual cricket::MediaType media_type() const = 0;

  virtual VideoChannel* AsVideoChannel() = 0;
  virtual VoiceChannel* AsVoiceChannel() = 0;

  virtual MediaSendChannelInterface* media_send_channel() = 0;

  virtual VideoMediaSendChannelInterface* video_media_send_channel() = 0;
  virtual VoiceMediaSendChannelInterface* voice_media_send_channel() = 0;
  virtual MediaReceiveChannelInterface* media_receive_channel() = 0;

  virtual VideoMediaReceiveChannelInterface* video_media_receive_channel() = 0;
  virtual VoiceMediaReceiveChannelInterface* voice_media_receive_channel() = 0;

  virtual absl::string_view transport_name() const = 0;

  virtual const std::string& mid() const = 0;

  virtual void Enable(bool enable) = 0;

  virtual void SetFirstPacketReceivedCallback(
      std::function<void()> callback) = 0;

  virtual bool SetLocalContent(const MediaContentDescription* content,
                               webrtc::SdpType type,
                               std::string& error_desc) = 0;
  virtual bool SetRemoteContent(const MediaContentDescription* content,
                                webrtc::SdpType type,
                                std::string& error_desc) = 0;
  virtual bool SetPayloadTypeDemuxingEnabled(bool enabled) = 0;

  virtual const std::vector<StreamParams>& local_streams() const = 0;
  virtual const std::vector<StreamParams>& remote_streams() const = 0;

  virtual bool SetRtpTransport(webrtc::RtpTransportInternal* rtp_transport) = 0;
};

}

#endif
