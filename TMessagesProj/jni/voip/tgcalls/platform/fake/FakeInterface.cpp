#include "FakeInterface.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/environment/environment.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/media_constants.h"

namespace tgcalls {

namespace {

bool gUseBuiltinCodecOrder = false;

std::vector<webrtc::SdpVideoFormat> iosLikeFormatOrder(std::vector<webrtc::SdpVideoFormat> formats) {
  std::vector<webrtc::SdpVideoFormat> h264;
  std::vector<webrtc::SdpVideoFormat> vp8;
  std::vector<webrtc::SdpVideoFormat> vp9;
  for (const auto& format : formats) {
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
      auto mode = format.parameters.find(cricket::kH264FmtpPacketizationMode);
      if (mode != format.parameters.end() && mode->second != "1") continue;
      h264.push_back(format);
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)) {
      vp8.push_back(format);
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName)) {
      auto profile = format.parameters.find("profile-id");
      if (profile != format.parameters.end() && profile->second != "0") continue;
      vp9.push_back(format);
    }
  }

  std::vector<webrtc::SdpVideoFormat> result;

  for (const auto& format : h264) {
    auto profile = format.parameters.find(cricket::kH264FmtpProfileLevelId);
    if (profile != format.parameters.end() && absl::StartsWithIgnoreCase(profile->second, "42e0")) {
      result.push_back(format);
      break;
    }
  }

  for (const auto& format : h264) {
    if (result.size() >= 2) break;
    if (!result.empty() && result[0] == format) continue;
    result.push_back(format);
  }
  if (!vp8.empty()) result.push_back(vp8[0]);
  if (!vp9.empty()) result.push_back(vp9[0]);
  return result;
}

class IosOrderVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  explicit IosOrderVideoEncoderFactory(std::unique_ptr<webrtc::VideoEncoderFactory> inner) : _inner(std::move(inner)) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    return iosLikeFormatOrder(_inner->GetSupportedFormats());
  }
  std::vector<webrtc::SdpVideoFormat> GetImplementations() const override {
    return iosLikeFormatOrder(_inner->GetImplementations());
  }
  CodecSupport QueryCodecSupport(const webrtc::SdpVideoFormat& format,
                                 absl::optional<std::string> scalability_mode) const override {
    return _inner->QueryCodecSupport(format, scalability_mode);
  }
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override {
    return _inner->CreateVideoEncoder(format);
  }
  std::unique_ptr<EncoderSelectorInterface> GetEncoderSelector() const override {
    return _inner->GetEncoderSelector();
  }

 private:
  std::unique_ptr<webrtc::VideoEncoderFactory> _inner;
};

class IosOrderVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  explicit IosOrderVideoDecoderFactory(std::unique_ptr<webrtc::VideoDecoderFactory> inner) : _inner(std::move(inner)) {}

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    return iosLikeFormatOrder(_inner->GetSupportedFormats());
  }
  CodecSupport QueryCodecSupport(const webrtc::SdpVideoFormat& format, bool reference_scaling) const override {
    return _inner->QueryCodecSupport(format, reference_scaling);
  }
  std::unique_ptr<webrtc::VideoDecoder> Create(const webrtc::Environment& env,
                                               const webrtc::SdpVideoFormat& format) override {
    return _inner->Create(env, format);
  }
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override {
    return _inner->CreateVideoDecoder(format);
  }

 private:
  std::unique_ptr<webrtc::VideoDecoderFactory> _inner;
};

}

void setFakePlatformBuiltinCodecOrder(bool useBuiltinOrder) {
  gUseBuiltinCodecOrder = useBuiltinOrder;
}

std::unique_ptr<webrtc::VideoEncoderFactory> FakeInterface::makeVideoEncoderFactory(bool preferHardwareEncoding, bool isScreencast) {
  auto factory = webrtc::CreateBuiltinVideoEncoderFactory();
  if (gUseBuiltinCodecOrder) return factory;
  return std::make_unique<IosOrderVideoEncoderFactory>(std::move(factory));
}

std::unique_ptr<webrtc::VideoDecoderFactory> FakeInterface::makeVideoDecoderFactory() {
  auto factory = webrtc::CreateBuiltinVideoDecoderFactory();
  if (gUseBuiltinCodecOrder) return factory;
  return std::make_unique<IosOrderVideoDecoderFactory>(std::move(factory));
}

webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> FakeInterface::makeVideoSource(rtc::Thread *signalingThread,
                                                                                     rtc::Thread *workerThread) {
  return nullptr;
}

bool FakeInterface::supportsEncoding(const std::string &codecName) {
  return false;

}

void FakeInterface::adaptVideoSource(webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource, int width,
                                     int height, int fps) {
}

std::unique_ptr<VideoCapturerInterface> FakeInterface::makeVideoCapturer(
    webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source, std::string deviceId,
    std::function<void(VideoState)> stateUpdated, std::function<void(PlatformCaptureInfo)> captureInfoUpdated,
    std::shared_ptr<PlatformContext> platformContext, std::pair<int, int> &outResolution) {
  return nullptr;

}

std::unique_ptr<PlatformInterface> CreatePlatformInterface() {
  return std::make_unique<FakeInterface>();
}

}
