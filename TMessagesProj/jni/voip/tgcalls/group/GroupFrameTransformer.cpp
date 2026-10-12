#include "group/GroupFrameTransformer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <optional>

#include "common_video/h264/h264_common.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/logging.h"

namespace tgcalls {
namespace {

static constexpr uint8_t kTypeMask = 0x1F;
static constexpr uint8_t kFuA = 28;
static constexpr uint8_t kIdr = 5;
static constexpr uint8_t kSps = 7;
static constexpr uint8_t kPps = 8;
static constexpr uint8_t kSei = 6;
static constexpr uint8_t kStapA = 24;
static constexpr size_t kNalShortStartCode = 3;
static constexpr size_t kNalHeaderSize = 1;
static constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;
constexpr size_t kStapAHeaderSize = kNalHeaderSize + kLengthFieldSize;

size_t calculateSliceHeaderBytesForPpsId(const uint8_t* data, size_t size) {
    if (size < 2)
        return 0;

    std::vector<uint8_t> rbsp = webrtc::H264::ParseRbsp(data, size);
    if (rbsp.size() < 2)
        return 0;

    rtc::ArrayView<const uint8_t> rbspView(rbsp.data() + 1, rbsp.size() - 1);
    webrtc::BitstreamReader reader(rbspView);

    reader.ReadExponentialGolomb();
    if (!reader.Ok()) {
        return 4;
    }

    reader.ReadExponentialGolomb();
    if (!reader.Ok()) {
        return 4;
    }

    reader.ReadExponentialGolomb();
    if (!reader.Ok()) {
        return 4;
    }

    size_t bitsConsumed = rbspView.size() * 8 - reader.RemainingBitCount();
    size_t bytesRead = 1 + (bitsConsumed + 7) / 8;

    return bytesRead + 1;
}

constexpr uint8_t P_BIT = 0x01;

constexpr uint8_t kH26XNaluShortStartSequenceSize = 3;

using IndexStartCodeSizePair = std::pair<size_t, size_t>;

std::optional<IndexStartCodeSizePair> FindNextH26XNaluIndex(const uint8_t* buffer,
                                                            const size_t bufferSize,
                                                            const size_t searchStartIndex = 0)
{
    constexpr uint8_t kH26XStartCodeHighestPossibleValue = 1;
    constexpr uint8_t kH26XStartCodeEndByteValue = 1;
    constexpr uint8_t kH26XStartCodeLeadingBytesValue = 0;

    if (bufferSize < kH26XNaluShortStartSequenceSize) {
        return std::nullopt;
    }

    for (size_t i = searchStartIndex; i < bufferSize - kH26XNaluShortStartSequenceSize;) {
        if (buffer[i + 2] > kH26XStartCodeHighestPossibleValue) {

            i += kH26XNaluShortStartSequenceSize;
        }
        else if (buffer[i + 2] == kH26XStartCodeEndByteValue) {

            if (buffer[i + 1] == kH26XStartCodeLeadingBytesValue &&
                buffer[i] == kH26XStartCodeLeadingBytesValue) {

                auto nalUnitStartIndex = i + kH26XNaluShortStartSequenceSize;

                if (i >= 1 && buffer[i - 1] == kH26XStartCodeLeadingBytesValue) {

                    return std::optional<IndexStartCodeSizePair>({nalUnitStartIndex, 4});
                }
                else {

                    return std::optional<IndexStartCodeSizePair>({nalUnitStartIndex, 3});
                }
            }

            i += kH26XNaluShortStartSequenceSize;
        }
        else {

            ++i;
        }
    }

    return std::nullopt;
}

struct UnencryptedRange {
    size_t offset = 0;
    size_t size = 0;

    UnencryptedRange(size_t offset_, size_t size_) :
    offset(offset_), size(size_) {
    }
};

}

std::vector<uint8_t> calculateH264FramePlaintextHeaderSize(rtc::ArrayView<const uint8_t> frame, uint32_t& headerSize) {
    if (frame.empty()) {
        headerSize = 0;
        return std::vector<uint8_t>();
    }

    std::vector<webrtc::H264::NaluIndex> naluIndices =
        webrtc::H264::FindNaluIndices(frame.data(), frame.size());

    if (naluIndices.empty()) {

        headerSize = 0;

        std::vector<uint8_t> frameData;
        frameData.resize(frame.size());
        std::copy(frame.begin(), frame.end(), frameData.begin());
        return frameData;
    }

    size_t maxOffset = 0;
    std::vector<size_t> naluToUpdate;

    for (const auto& naluIndex : naluIndices) {
        size_t startCodeLength = naluIndex.payload_start_offset - naluIndex.start_offset;

        if (startCodeLength == kNalShortStartCode) {
            naluToUpdate.push_back(naluIndex.start_offset);
        }

        size_t headerEndOffset = naluIndex.payload_start_offset + kNalHeaderSize;

        if (naluIndex.payload_size >= kNalHeaderSize) {

            uint8_t nalType = frame[naluIndex.payload_start_offset] & kTypeMask;

            if (nalType == kFuA) {

                if (naluIndex.payload_size >= kFuAHeaderSize) {
                    headerEndOffset = naluIndex.payload_start_offset + kFuAHeaderSize;

                    bool isStartBit = (frame[naluIndex.payload_start_offset + 1] & 0x80) != 0;
                    if (isStartBit) {

                        uint8_t originalNalType = frame[naluIndex.payload_start_offset + 1] & kTypeMask;

                        if (originalNalType == kIdr || originalNalType == 1) {

                            headerEndOffset += 4;
                        }
                    }
                }
            } else if (nalType == kStapA) {

                if (naluIndex.payload_size >= kStapAHeaderSize) {
                    headerEndOffset = naluIndex.payload_start_offset + kStapAHeaderSize;

                    if (naluIndex.payload_size > kStapAHeaderSize) {
                        uint8_t firstNalType = frame[naluIndex.payload_start_offset + kStapAHeaderSize] & kTypeMask;

                        if (firstNalType == kIdr || firstNalType == 1) {

                            headerEndOffset += 4;
                        }
                    }
                }
            }

            else if (nalType == kIdr || nalType == 1) {

                size_t ppsIdBytes = calculateSliceHeaderBytesForPpsId(
                    frame.data() + naluIndex.payload_start_offset,
                    naluIndex.payload_size);

                headerEndOffset = naluIndex.payload_start_offset + ppsIdBytes;
                maxOffset = std::max(maxOffset, headerEndOffset);
                break;
            }

            else if (nalType == kSps || nalType == kPps || nalType == kSei) {

                headerEndOffset = naluIndex.payload_start_offset + naluIndex.payload_size;
            }
        }

        maxOffset = std::max(maxOffset, headerEndOffset);
    }

    std::vector<uint8_t> frameData;
    frameData.resize(frame.size() + naluToUpdate.size());

    size_t offset = 0;

    for (size_t i = 0; i < naluToUpdate.size(); ++i) {
        const auto& naluIndex = naluToUpdate[i];
        if (naluIndex - offset > 0) {
            std::copy(frame.begin() + offset, frame.begin() + naluIndex, frameData.begin() + offset + i);
        }

        frameData[naluIndex + i] = 0;
        offset = naluIndex;
    }

    if (offset < frame.size()) {
        std::copy(frame.begin() + offset, frame.end(), frameData.begin() + offset + naluToUpdate.size());
    }

    headerSize = static_cast<uint32_t>(maxOffset + naluToUpdate.size());
    return frameData;
}

std::vector<uint8_t> calculateVp8FramePlaintextHeaderSize(rtc::ArrayView<const uint8_t> frame, uint32_t& headerSize) {

    if (frame.empty()) {
        headerSize = 0;
        return std::vector<uint8_t>();
    }

    uint8_t first_byte = frame[0];

    bool is_key_frame = (first_byte & P_BIT) == 0;

    if (is_key_frame) {

        headerSize = frame.size() >= 10 ? 10 : ((uint32_t)frame.size());
    } else {

        headerSize = 1;
    }

    std::vector<uint8_t> frameData;
    frameData.resize(frame.size());
    std::copy(frame.begin(), frame.end(), frameData.begin());
    return frameData;
}

bool ValidateEncryptedFrame(FrameTransformerPayloadType payloadType, rtc::ArrayView<uint8_t> frame, int plaintextPrefix) {
    if (payloadType != FrameTransformerPayloadType::H264) {
        return true;
    }

    static_assert(kH26XNaluShortStartSequenceSize - 1 >= 0, "Padding will overflow!");
    constexpr size_t Padding = kH26XNaluShortStartSequenceSize - 1;

    std::vector<UnencryptedRange> unencryptedRanges;
    if (plaintextPrefix != 0) {
        unencryptedRanges.emplace_back(0, plaintextPrefix);
    }

    size_t encryptedSectionStart = 0;
    for (auto& range : unencryptedRanges) {
        if (encryptedSectionStart == range.offset) {
            encryptedSectionStart += range.size;
            continue;
        }

        auto start = encryptedSectionStart - std::min(encryptedSectionStart, size_t{Padding});
        auto end = std::min(range.offset + Padding, frame.size());
        if (FindNextH26XNaluIndex(frame.data() + start, end - start)) {
            return false;
        }

        encryptedSectionStart = range.offset + range.size;
    }

    if (encryptedSectionStart == frame.size()) {
        return true;
    }

    auto start = encryptedSectionStart - std::min(encryptedSectionStart, size_t{Padding});
    auto end = frame.size();
    if (FindNextH26XNaluIndex(frame.data() + start, end - start)) {
        return false;
    }

    return true;
}

std::vector<uint8_t> encryptGroupAudioFrame(
        GroupEncryptDecryptFunction const &transform,
        int64_t userId,
        rtc::ArrayView<const uint8_t> frame,
        uint8_t audioLevel,
        bool hasSpeech) {
    std::vector<uint8_t> buffer;
    buffer.resize(frame.size() + 1 + 1);
    std::copy(frame.begin(), frame.end(), buffer.begin());

    buffer[buffer.size() - 1 - 1] = 0x01;
    uint8_t encodedAudioLevelAndSpeech = 0;
    if (hasSpeech) {
        encodedAudioLevelAndSpeech = encodedAudioLevelAndSpeech | 0x80;
    }
    encodedAudioLevelAndSpeech |= audioLevel & 0x7f;
    buffer[buffer.size() - 1] = encodedAudioLevelAndSpeech;

    return transform(buffer, userId, true, 0);
}

std::vector<uint8_t> encryptGroupVideoFrame(
        GroupEncryptDecryptFunction const &transform,
        int64_t userId,
        FrameTransformerPayloadType payloadType,
        rtc::ArrayView<const uint8_t> frame) {
    uint32_t plaintextHeaderSize = 0;
    std::vector<uint8_t> frameData;
    if (payloadType == FrameTransformerPayloadType::H264) {
        frameData = calculateH264FramePlaintextHeaderSize(frame, plaintextHeaderSize);
    } else if (payloadType == FrameTransformerPayloadType::VP8) {
        frameData = calculateVp8FramePlaintextHeaderSize(frame, plaintextHeaderSize);
    } else {
        return std::vector<uint8_t>();
    }

    if (plaintextHeaderSize > (uint32_t)frameData.size()) {
        plaintextHeaderSize = (uint32_t)frameData.size();
    }

    for (int attempt = 0; attempt < 4; attempt++) {
        auto result = transform(frameData, userId, true, plaintextHeaderSize);
        if (result.empty()) {
            break;
        }
        if (ValidateEncryptedFrame(payloadType, result, plaintextHeaderSize)) {
            return result;
        }
    }
    return std::vector<uint8_t>();
}

std::vector<uint8_t> decryptGroupAudioFrame(
        GroupEncryptDecryptFunction const &transform,
        int64_t userId,
        rtc::ArrayView<const uint8_t> frame,
        uint8_t *audioLevel,
        bool *hasSpeech,
        bool *hadExtension) {
    if (hadExtension) {
        *hadExtension = false;
    }

    std::vector<uint8_t> buffer;
    buffer.resize(frame.size());
    std::copy(frame.begin(), frame.end(), buffer.begin());

    auto result = transform(buffer, userId, false, 0);
    if (result.empty()) {
        return result;
    }
    if (result.size() >= 2) {
        uint8_t extensionFlags = result[result.size() - 2];
        if (extensionFlags & 0x01) {
            uint8_t audioLevelAndSpeech = result[result.size() - 1];
            if (hasSpeech) {
                *hasSpeech = (audioLevelAndSpeech & 0x80) != 0;
            }
            if (audioLevel) {
                *audioLevel = audioLevelAndSpeech & 0x7f;
            }
            if (hadExtension) {
                *hadExtension = true;
            }
            result.resize(result.size() - 2);
        } else {
            result.resize(result.size() - 1);
        }
    }
    return result;
}

std::vector<uint8_t> decryptGroupVideoFrame(
        GroupEncryptDecryptFunction const &transform,
        int64_t userId,
        rtc::ArrayView<const uint8_t> frame) {
    std::vector<uint8_t> encryptedFrame;
    encryptedFrame.resize(frame.size());
    std::copy(frame.begin(), frame.end(), encryptedFrame.begin());
    return transform(encryptedFrame, userId, false, 0);
}

FrameTransformer::FrameTransformer(bool isEncryptor, std::function<std::vector<uint8_t>(std::vector<uint8_t> const &, int64_t, bool, int32_t)> transform, int64_t userId, std::map<int32_t, FrameTransformerPayloadType> const &payloadTypeMapping, std::function<std::pair<uint8_t, bool>()> getAudioLevelAndSpeech, std::function<void(uint8_t, bool)> setAudioLevelAndSpeech) :
_isEncryptor(isEncryptor),
_transform(transform),
_userId(userId),
_payloadTypeMapping(payloadTypeMapping),
_getAudioLevelAndSpeech(getAudioLevelAndSpeech),
_setAudioLevelAndSpeech(setAudioLevelAndSpeech) {
}

FrameTransformer::FrameTransformer(bool isEncryptor, GroupEncryptDecryptFunction transform, std::function<int64_t(uint32_t ssrc)> userIdForSsrc, std::map<int32_t, FrameTransformerPayloadType> const &payloadTypeMapping, std::function<std::pair<uint8_t, bool>()> getAudioLevelAndSpeech, std::function<void(uint8_t, bool)> setAudioLevelAndSpeech) :
_isEncryptor(isEncryptor),
_transform(transform),
_userId(0),
_userIdForSsrc(userIdForSsrc),
_payloadTypeMapping(payloadTypeMapping),
_getAudioLevelAndSpeech(getAudioLevelAndSpeech),
_setAudioLevelAndSpeech(setAudioLevelAndSpeech) {
}

void FrameTransformer::RegisterTransformedFrameCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
    webrtc::MutexLock lock(&_mutex);

    if (_sinkCallback) {
        RTC_LOG(LS_WARNING) << "FrameTransformer: replacing sink callback";
    }
    _sinkCallback = callback;
}

void FrameTransformer::RegisterTransformedFrameSinkCallback(rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback, uint32_t ssrc) {
    webrtc::MutexLock lock(&_mutex);
    _sinkCallbackBySsrc[ssrc] = callback;
}

void FrameTransformer::UnregisterTransformedFrameSinkCallback(uint32_t ssrc) {
    webrtc::MutexLock lock(&_mutex);
    _sinkCallbackBySsrc.erase(ssrc);
}

void FrameTransformer::Transform(std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
    webrtc::MutexLock lock(&_mutex);

    const auto ssrc = frame->GetSsrc();
    const auto i = _sinkCallbackBySsrc.find(ssrc);
    const auto sink = (i != _sinkCallbackBySsrc.end() && i->second)
        ? i->second.get()
        : _sinkCallback.get();
    if (!sink) {
        return;
    }

    FrameTransformerPayloadType payloadType = FrameTransformerPayloadType::Unknown;
    const auto foundPayloadType = _payloadTypeMapping.find(frame->GetPayloadType());
    if (foundPayloadType != _payloadTypeMapping.end()) {
        payloadType = foundPayloadType->second;
    }

    const int64_t userId = _userIdForSsrc ? _userIdForSsrc(ssrc) : _userId;

    if (_isEncryptor) {
        if (payloadType == FrameTransformerPayloadType::H264 || payloadType == FrameTransformerPayloadType::VP8) {
            auto result = encryptGroupVideoFrame(_transform, userId, payloadType, frame->GetData());
            if (!result.empty()) {
                frame->SetData(result);
                sink->OnTransformedFrame(std::move(frame));
            }
        } else {
            std::pair<uint8_t, bool> audioLevelAndSpeech = std::make_pair(0, false);
            if (_getAudioLevelAndSpeech) {
                audioLevelAndSpeech = _getAudioLevelAndSpeech();
            }
            auto result = encryptGroupAudioFrame(_transform, userId, frame->GetData(),
                                                 audioLevelAndSpeech.first,
                                                 audioLevelAndSpeech.second);
            if (!result.empty()) {
                frame->SetData(result);
                sink->OnTransformedFrame(std::move(frame));
            }
        }
    } else {
        if (payloadType != FrameTransformerPayloadType::Opus) {
            auto result = decryptGroupVideoFrame(_transform, userId, frame->GetData());
            if (!result.empty()) {
                frame->SetData(result);
                sink->OnTransformedFrame(std::move(frame));
            }
        } else {
            uint8_t audioLevel = 0;
            bool hasSpeech = false;
            bool hadExtension = false;
            auto result = decryptGroupAudioFrame(_transform, userId, frame->GetData(),
                                                 &audioLevel, &hasSpeech, &hadExtension);
            if (!result.empty()) {

                if (hadExtension && _setAudioLevelAndSpeech) {
                    _setAudioLevelAndSpeech(audioLevel, hasSpeech);
                }
                frame->SetData(result);
                sink->OnTransformedFrame(std::move(frame));
            }
        }
    }
}

}
