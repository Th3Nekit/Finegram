#ifndef TGCALLS_GROUP_AUDIO_CAPTURE_POST_PROCESSOR_H
#define TGCALLS_GROUP_AUDIO_CAPTURE_POST_PROCESSOR_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "group/GroupInstanceImpl.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/synchronization/mutex.h"

#ifndef USE_RNNOISE
#define USE_RNNOISE 1
#endif

struct DenoiseState;

namespace webrtc {
class AudioBuffer;
}

namespace tgcalls {

class VadHistory {
public:
    VadHistory();
    bool update(float vadProbability);

private:
    static const int kLength = 8;
    float _vadResultHistory[kLength];
};

struct NoiseSuppressionConfiguration {
    NoiseSuppressionConfiguration(bool isEnabled_) :
    isEnabled(isEnabled_) {

    }

    bool isEnabled = false;
};

#if USE_RNNOISE

class AudioCapturePostProcessor : public webrtc::CustomProcessing {
public:
    AudioCapturePostProcessor(std::function<void(GroupLevelValue const &)> updated,
                              std::shared_ptr<NoiseSuppressionConfiguration> noiseSuppressionConfiguration,
                              std::vector<float> *externalAudioSamples,
                              webrtc::Mutex *externalAudioSamplesMutex);
    ~AudioCapturePostProcessor() override;

private:
    void Initialize(int sample_rate_hz, int num_channels) override;
    void Process(webrtc::AudioBuffer *originalBuffer) override;
    std::string ToString() const override;
    void SetRuntimeSetting(webrtc::AudioProcessing::RuntimeSetting setting) override;

    std::function<void(GroupLevelValue const &)> _updated;
    std::shared_ptr<NoiseSuppressionConfiguration> _noiseSuppressionConfiguration;

    int _currentSampleRate = 0;

    DenoiseState *_denoiseState = nullptr;
    std::vector<float> _frameSamples;
    int32_t _peakCount = 0;
    float _peak = 0;
    VadHistory _history;

    std::vector<float> *_externalAudioSamples = nullptr;
    webrtc::Mutex *_externalAudioSamplesMutex = nullptr;
};
#endif

}

#endif
