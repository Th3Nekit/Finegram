#ifndef TGCALLS_GROUP_FRAME_TRANSFORMER_H
#define TGCALLS_GROUP_FRAME_TRANSFORMER_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/frame_transformer_interface.h"
#include "rtc_base/synchronization/mutex.h"

namespace tgcalls {

using GroupEncryptDecryptFunction =
    std::function<std::vector<uint8_t>(std::vector<uint8_t> const &, int64_t, bool, int32_t)>;

class AudioLevelAndSpeechHolder {
public:
    AudioLevelAndSpeechHolder() {
    }

    void set(uint8_t audioLevel, bool hasSpeech) {
        webrtc::MutexLock lock(&_mutex);
        _audioLevel = audioLevel;
        _hasSpeech = hasSpeech;
    }

    std::pair<uint8_t, bool> get() {
        webrtc::MutexLock lock(&_mutex);
        return std::make_pair(_audioLevel, _hasSpeech);
    }

private:
    webrtc::Mutex _mutex;
    uint8_t _audioLevel = 0;
    bool _hasSpeech = false;
};

enum class FrameTransformerPayloadType {
    Unknown,
    Opus,
    H264,
    VP8
};

std::vector<uint8_t> calculateH264FramePlaintextHeaderSize(
    rtc::ArrayView<const uint8_t> frame, uint32_t &headerSize);

std::vector<uint8_t> calculateVp8FramePlaintextHeaderSize(
    rtc::ArrayView<const uint8_t> frame, uint32_t &headerSize);

bool ValidateEncryptedFrame(FrameTransformerPayloadType payloadType,
                            rtc::ArrayView<uint8_t> frame,
                            int plaintextPrefix);

std::vector<uint8_t> encryptGroupAudioFrame(
    GroupEncryptDecryptFunction const &transform,
    int64_t userId,
    rtc::ArrayView<const uint8_t> frame,
    uint8_t audioLevel,
    bool hasSpeech);

std::vector<uint8_t> encryptGroupVideoFrame(
    GroupEncryptDecryptFunction const &transform,
    int64_t userId,
    FrameTransformerPayloadType payloadType,
    rtc::ArrayView<const uint8_t> frame);

std::vector<uint8_t> decryptGroupAudioFrame(
    GroupEncryptDecryptFunction const &transform,
    int64_t userId,
    rtc::ArrayView<const uint8_t> frame,
    uint8_t *audioLevel,
    bool *hasSpeech,
    bool *hadExtension);

std::vector<uint8_t> decryptGroupVideoFrame(
    GroupEncryptDecryptFunction const &transform,
    int64_t userId,
    rtc::ArrayView<const uint8_t> frame);

class FrameTransformer : public webrtc::FrameTransformerInterface {
public:
    FrameTransformer(bool isEncryptor,
                     GroupEncryptDecryptFunction transform,
                     int64_t userId,
                     std::map<int32_t, FrameTransformerPayloadType> const &payloadTypeMapping,
                     std::function<std::pair<uint8_t, bool>()> getAudioLevelAndSpeech,
                     std::function<void(uint8_t, bool)> setAudioLevelAndSpeech);

    FrameTransformer(bool isEncryptor,
                     GroupEncryptDecryptFunction transform,
                     std::function<int64_t(uint32_t ssrc)> userIdForSsrc,
                     std::map<int32_t, FrameTransformerPayloadType> const &payloadTypeMapping,
                     std::function<std::pair<uint8_t, bool>()> getAudioLevelAndSpeech,
                     std::function<void(uint8_t, bool)> setAudioLevelAndSpeech);

    void RegisterTransformedFrameCallback(
        rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) override;
    void RegisterTransformedFrameSinkCallback(
        rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback, uint32_t ssrc) override;
    void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;
    void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> frame) override;

private:
    bool _isEncryptor = false;
    GroupEncryptDecryptFunction _transform;
    int64_t _userId = 0;
    std::function<int64_t(uint32_t)> _userIdForSsrc;
    std::map<int32_t, FrameTransformerPayloadType> _payloadTypeMapping;
    std::function<std::pair<uint8_t, bool>()> _getAudioLevelAndSpeech;
    std::function<void(uint8_t, bool)> _setAudioLevelAndSpeech;
    webrtc::Mutex _mutex;
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> _sinkCallback;
    std::map<uint32_t, rtc::scoped_refptr<webrtc::TransformedFrameCallback>> _sinkCallbackBySsrc;
};

}

#endif
