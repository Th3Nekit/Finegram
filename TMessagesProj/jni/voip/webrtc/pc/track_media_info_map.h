/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TRACK_MEDIA_INFO_MAP_H_
#define PC_TRACK_MEDIA_INFO_MAP_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "media/base/media_channel.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_sender.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

class TrackMediaInfoMap {
 public:
  TrackMediaInfoMap();

  void Initialize(
      absl::optional<cricket::VoiceMediaInfo> voice_media_info,
      absl::optional<cricket::VideoMediaInfo> video_media_info,
      rtc::ArrayView<rtc::scoped_refptr<RtpSenderInternal>> rtp_senders,
      rtc::ArrayView<rtc::scoped_refptr<RtpReceiverInternal>> rtp_receivers);

  const absl::optional<cricket::VoiceMediaInfo>& voice_media_info() const {
    RTC_DCHECK(is_initialized_);
    return voice_media_info_;
  }
  const absl::optional<cricket::VideoMediaInfo>& video_media_info() const {
    RTC_DCHECK(is_initialized_);
    return video_media_info_;
  }

  const cricket::VoiceSenderInfo* GetVoiceSenderInfoBySsrc(uint32_t ssrc) const;
  const cricket::VoiceReceiverInfo* GetVoiceReceiverInfoBySsrc(
      uint32_t ssrc) const;
  const cricket::VideoSenderInfo* GetVideoSenderInfoBySsrc(uint32_t ssrc) const;
  const cricket::VideoReceiverInfo* GetVideoReceiverInfoBySsrc(
      uint32_t ssrc) const;

  rtc::scoped_refptr<AudioTrackInterface> GetAudioTrack(
      const cricket::VoiceSenderInfo& voice_sender_info) const;
  rtc::scoped_refptr<AudioTrackInterface> GetAudioTrack(
      const cricket::VoiceReceiverInfo& voice_receiver_info) const;
  rtc::scoped_refptr<VideoTrackInterface> GetVideoTrack(
      const cricket::VideoSenderInfo& video_sender_info) const;
  rtc::scoped_refptr<VideoTrackInterface> GetVideoTrack(
      const cricket::VideoReceiverInfo& video_receiver_info) const;

  absl::optional<int> GetAttachmentIdByTrack(
      const MediaStreamTrackInterface* track) const;

 private:
  bool is_initialized_ = false;
  absl::optional<cricket::VoiceMediaInfo> voice_media_info_;
  absl::optional<cricket::VideoMediaInfo> video_media_info_;

  std::map<const cricket::VoiceSenderInfo*,
           rtc::scoped_refptr<AudioTrackInterface>>
      audio_track_by_sender_info_;
  std::map<const cricket::VoiceReceiverInfo*,
           rtc::scoped_refptr<AudioTrackInterface>>
      audio_track_by_receiver_info_;
  std::map<const cricket::VideoSenderInfo*,
           rtc::scoped_refptr<VideoTrackInterface>>
      video_track_by_sender_info_;
  std::map<const cricket::VideoReceiverInfo*,
           rtc::scoped_refptr<VideoTrackInterface>>
      video_track_by_receiver_info_;

  std::map<const MediaStreamTrackInterface*, int> attachment_id_by_track_;

  std::map<uint32_t, cricket::VoiceSenderInfo*> voice_info_by_sender_ssrc_;
  std::map<uint32_t, cricket::VoiceReceiverInfo*> voice_info_by_receiver_ssrc_;
  std::map<uint32_t, cricket::VideoSenderInfo*> video_info_by_sender_ssrc_;
  std::map<uint32_t, cricket::VideoReceiverInfo*> video_info_by_receiver_ssrc_;
};

}

#endif
