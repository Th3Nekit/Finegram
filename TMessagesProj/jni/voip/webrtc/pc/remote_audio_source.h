/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_REMOTE_AUDIO_SOURCE_H_
#define PC_REMOTE_AUDIO_SOURCE_H_

#include <stdint.h>

#include <list>
#include <string>

#include "absl/types/optional.h"
#include "api/call/audio_sink.h"
#include "api/media_stream_interface.h"
#include "api/notifier.h"
#include "api/task_queue/task_queue_base.h"
#include "media/base/media_channel.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

class RemoteAudioSource : public Notifier<AudioSourceInterface> {
 public:

  enum class OnAudioChannelGoneAction {
    kSurvive,
    kEnd,
  };

  explicit RemoteAudioSource(
      TaskQueueBase* worker_thread,
      OnAudioChannelGoneAction on_audio_channel_gone_action);

  void Start(cricket::VoiceMediaReceiveChannelInterface* media_channel,
             absl::optional<uint32_t> ssrc);
  void Stop(cricket::VoiceMediaReceiveChannelInterface* media_channel,
            absl::optional<uint32_t> ssrc);
  void SetState(SourceState new_state);

  MediaSourceInterface::SourceState state() const override;
  bool remote() const override;

  void SetVolume(double volume) override;
  void RegisterAudioObserver(AudioObserver* observer) override;
  void UnregisterAudioObserver(AudioObserver* observer) override;

  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;

 protected:
  ~RemoteAudioSource() override;

 private:

  class AudioDataProxy;

  void OnData(const AudioSinkInterface::Data& audio);
  void OnAudioChannelGone();

  TaskQueueBase* const main_thread_;
  TaskQueueBase* const worker_thread_;
  const OnAudioChannelGoneAction on_audio_channel_gone_action_;
  std::list<AudioObserver*> audio_observers_;
  Mutex sink_lock_;
  std::list<AudioTrackSinkInterface*> sinks_;
  SourceState state_;
};

}

#endif
