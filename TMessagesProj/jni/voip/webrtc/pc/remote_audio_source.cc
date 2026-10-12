/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/remote_audio_source.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

class RemoteAudioSource::AudioDataProxy : public AudioSinkInterface {
 public:
  explicit AudioDataProxy(RemoteAudioSource* source) : source_(source) {
    RTC_DCHECK(source);
  }

  AudioDataProxy() = delete;
  AudioDataProxy(const AudioDataProxy&) = delete;
  AudioDataProxy& operator=(const AudioDataProxy&) = delete;

  ~AudioDataProxy() override { source_->OnAudioChannelGone(); }

  void OnData(const AudioSinkInterface::Data& audio) override {
    source_->OnData(audio);
  }

 private:
  const rtc::scoped_refptr<RemoteAudioSource> source_;
};

RemoteAudioSource::RemoteAudioSource(
    TaskQueueBase* worker_thread,
    OnAudioChannelGoneAction on_audio_channel_gone_action)
    : main_thread_(TaskQueueBase::Current()),
      worker_thread_(worker_thread),
      on_audio_channel_gone_action_(on_audio_channel_gone_action),
      state_(MediaSourceInterface::kInitializing) {
  RTC_DCHECK(main_thread_);
  RTC_DCHECK(worker_thread_);
}

RemoteAudioSource::~RemoteAudioSource() {
  RTC_DCHECK(audio_observers_.empty());
  if (!sinks_.empty()) {
    RTC_LOG(LS_WARNING)
        << "RemoteAudioSource destroyed while sinks_ is non-empty.";
  }
}

void RemoteAudioSource::Start(
    cricket::VoiceMediaReceiveChannelInterface* media_channel,
    absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  RTC_DCHECK(media_channel);
  ssrc ? media_channel->SetRawAudioSink(*ssrc,
                                        std::make_unique<AudioDataProxy>(this))
       : media_channel->SetDefaultRawAudioSink(
             std::make_unique<AudioDataProxy>(this));
}

void RemoteAudioSource::Stop(
    cricket::VoiceMediaReceiveChannelInterface* media_channel,
    absl::optional<uint32_t> ssrc) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(media_channel);
  ssrc ? media_channel->SetRawAudioSink(*ssrc, nullptr)
       : media_channel->SetDefaultRawAudioSink(nullptr);
}

void RemoteAudioSource::SetState(SourceState new_state) {
  RTC_DCHECK_RUN_ON(main_thread_);
  if (state_ != new_state) {
    state_ = new_state;
    FireOnChanged();
  }
}

MediaSourceInterface::SourceState RemoteAudioSource::state() const {
  RTC_DCHECK_RUN_ON(main_thread_);
  return state_;
}

bool RemoteAudioSource::remote() const {
  RTC_DCHECK_RUN_ON(main_thread_);
  return true;
}

void RemoteAudioSource::SetVolume(double volume) {
  RTC_DCHECK_GE(volume, 0);
  RTC_DCHECK_LE(volume, 10);
  RTC_LOG(LS_INFO) << rtc::StringFormat("RAS::%s({volume=%.2f})", __func__,
                                        volume);
  for (auto* observer : audio_observers_) {
    observer->OnSetVolume(volume);
  }
}

void RemoteAudioSource::RegisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  RTC_DCHECK(!absl::c_linear_search(audio_observers_, observer));
  audio_observers_.push_back(observer);
}

void RemoteAudioSource::UnregisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  audio_observers_.remove(observer);
}

void RemoteAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(main_thread_);
  RTC_DCHECK(sink);

  MutexLock lock(&sink_lock_);
  RTC_DCHECK(!absl::c_linear_search(sinks_, sink));
  sinks_.push_back(sink);
}

void RemoteAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK_RUN_ON(main_thread_);
  RTC_DCHECK(sink);

  MutexLock lock(&sink_lock_);
  sinks_.remove(sink);
}

void RemoteAudioSource::OnData(const AudioSinkInterface::Data& audio) {

  TRACE_EVENT0("webrtc", "RemoteAudioSource::OnData");
  MutexLock lock(&sink_lock_);
  for (auto* sink : sinks_) {

    sink->OnData(audio.data, 16, audio.sample_rate, audio.channels,
                 audio.samples_per_channel,
                                                   absl::nullopt);
  }
}

void RemoteAudioSource::OnAudioChannelGone() {
  if (on_audio_channel_gone_action_ != OnAudioChannelGoneAction::kEnd) {
    return;
  }

  rtc::scoped_refptr<RemoteAudioSource> thiz(this);
  main_thread_->PostTask([thiz = std::move(thiz)] {
    thiz->sinks_.clear();
    thiz->SetState(MediaSourceInterface::kEnded);
  });
}

}
