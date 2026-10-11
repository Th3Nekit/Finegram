#ifndef TGCALLS_CHANNEL_MANAGER_H_
#define TGCALLS_CHANNEL_MANAGER_H_
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "api/audio_options.h"
#include "api/crypto/crypto_options.h"
#include "api/rtp_parameters.h"
#include "api/video/video_bitrate_allocator_factory.h"
#include "call/call.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/media_config.h"
#include "media/base/media_engine.h"
#include "pc/channel.h"
#include "pc/channel_interface.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/unique_id_generator.h"
namespace tgcalls {

class ChannelManager {
 public:

  static std::unique_ptr<ChannelManager> Create(
      std::unique_ptr<cricket::MediaEngineInterface> media_engine,
      rtc::Thread* worker_thread,
      rtc::Thread* network_thread);
  ChannelManager() = delete;
  ~ChannelManager();
  rtc::Thread* worker_thread() const { return worker_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }
  cricket::MediaEngineInterface* media_engine() { return media_engine_.get(); }
  rtc::UniqueRandomIdGenerator& ssrc_generator() { return ssrc_generator_; }

  cricket::VoiceChannel* CreateVoiceChannel(webrtc::Call* call,
                                   const cricket::MediaConfig& media_config,
                                   const std::string& mid,
                                   bool srtp_required,
                                   const webrtc::CryptoOptions& crypto_options,
                                   const cricket::AudioOptions& options);

  cricket::VideoChannel* CreateVideoChannel(
      webrtc::Call* call,
      const cricket::MediaConfig& media_config,
      const std::string& mid,
      bool srtp_required,
      const webrtc::CryptoOptions& crypto_options,
      const cricket::VideoOptions& options,
      webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory);
  void DestroyChannel(cricket::ChannelInterface* channel);
 protected:
  ChannelManager(std::unique_ptr<cricket::MediaEngineInterface> media_engine,
                 rtc::Thread* worker_thread,
                 rtc::Thread* network_thread);

  void DestroyVoiceChannel(cricket::VoiceChannel* voice_channel);

  void DestroyVideoChannel(cricket::VideoChannel* video_channel);
 private:
  const std::unique_ptr<cricket::MediaEngineInterface> media_engine_;
  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  rtc::Thread* const network_thread_;

  rtc::UniqueRandomIdGenerator ssrc_generator_;

  std::vector<std::unique_ptr<cricket::VoiceChannel>> voice_channels_
      RTC_GUARDED_BY(worker_thread_);
  std::vector<std::unique_ptr<cricket::VideoChannel>> video_channels_
      RTC_GUARDED_BY(worker_thread_);
};
}
#endif