#ifndef TGCALLS_STREAMING_AUDIO_RENDERER_H
#define TGCALLS_STREAMING_AUDIO_RENDERER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace webrtc {
template <typename T>
class PushResampler;
}

namespace tgcalls {

class StreamingAudioRenderer {
public:

    using Source = std::function<void(int16_t *samples, size_t numSamples, size_t numChannels, uint32_t sampleRate)>;

    StreamingAudioRenderer();
    ~StreamingAudioRenderer();

    bool render(Source const &source, int16_t *audioSamples, size_t numSamples, size_t numChannels, uint32_t samplesPerSec);

private:
    std::unique_ptr<webrtc::PushResampler<int16_t>> _resampler;
    std::vector<int16_t> _sourceSamples;
};

}

#endif
