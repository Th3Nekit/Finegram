#include "group/GroupInstanceReferenceImpl.h"

#include "LogSinkImpl.h"
#include "FakeAudioDeviceModule.h"
#include "StaticThreads.h"
#include "ThreadLocalObject.h"
#include "AudioDeviceHelper.h"

#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/enable_media.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/jsep.h"
#include "api/jsep_session_description.h"
#include "api/jsep_ice_candidate.h"
#include "api/candidate.h"
#include "api/units/time_delta.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/stats/rtcstats_objects.h"
#include "pc/session_description.h"

#include "pc/peer_connection.h"
#include "pc/media_session.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "absl/strings/match.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "rtc_base/network.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/helpers.h"
#include "rtc_base/time_utils.h"

#include "modules/audio_processing/audio_buffer.h"

#include "platform/PlatformInterface.h"
#ifdef WEBRTC_IOS
#include "platform/darwin/iOS/tgcalls_audio_device_module_ios.h"
#endif

#include "group/GroupJoinPayloadInternal.h"
#include "group/GroupFrameTransformer.h"
#include "group/GroupAudioCapturePostProcessor.h"

#include "third-party/json11.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace tgcalls {

namespace {

absl::string_view mapIceCandidateTypeToInternal(const std::string &type) {
    if (type == "host") return cricket::LOCAL_PORT_TYPE;
    if (type == "srflx") return cricket::STUN_PORT_TYPE;
    if (type == "prflx") return cricket::PRFLX_PORT_TYPE;
    if (type == "relay") return cricket::RELAY_PORT_TYPE;
    return type;
}

class GRPeerConnectionObserver : public webrtc::PeerConnectionObserver {
public:
    std::function<void()> onRenegotiationNeeded;
    std::function<void(const webrtc::IceCandidateInterface *)> onIceCandidate;
    std::function<void(webrtc::PeerConnectionInterface::IceConnectionState)> onConnectionChange;
    std::function<void(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>)> onTrack;
    std::function<void(webrtc::scoped_refptr<webrtc::DataChannelInterface>)> onDataChannel;

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState) override {}
    void OnAddStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface>) override {}
    void OnRemoveStream(webrtc::scoped_refptr<webrtc::MediaStreamInterface>) override {}

    void OnTrack(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override {
        if (onTrack) onTrack(transceiver);
    }

    void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> dc) override {
        if (onDataChannel) onDataChannel(dc);
    }

    void OnRenegotiationNeeded() override {
        if (onRenegotiationNeeded) onRenegotiationNeeded();
    }

    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) override {
        if (onConnectionChange) onConnectionChange(state);
    }

    void OnStandardizedIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState) override {}
    void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState) override {}
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) override {}

    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override {
        if (onIceCandidate) onIceCandidate(candidate);
    }

    void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>&) override {}
    void OnIceSelectedCandidatePairChanged(const cricket::CandidatePairChangeEvent&) override {}
    void OnAddTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>, const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>&) override {}
    void OnRemoveTrack(webrtc::scoped_refptr<webrtc::RtpReceiverInterface>) override {}
};

class GRDataChannelObserver : public webrtc::DataChannelObserver {
public:
    std::function<void()> onStateChange;
    std::function<void(webrtc::DataBuffer const &)> onMessage;

    void OnStateChange() override { if (onStateChange) onStateChange(); }
    void OnMessage(webrtc::DataBuffer const &buffer) override { if (onMessage) onMessage(buffer); }
};

class GRSetSDPObserver : public webrtc::SetSessionDescriptionObserver {
public:
    GRSetSDPObserver(std::function<void(webrtc::RTCError)> callback) : _callback(std::move(callback)) {}
    void OnSuccess() override { _callback(webrtc::RTCError::OK()); }
    void OnFailure(webrtc::RTCError error) override { _callback(std::move(error)); }
private:
    std::function<void(webrtc::RTCError)> _callback;
};

class GRCreateSDPObserver : public webrtc::CreateSessionDescriptionObserver {
public:
    GRCreateSDPObserver(std::function<void(webrtc::SessionDescriptionInterface*)> onSuccess,
                        std::function<void(webrtc::RTCError)> onFailure)
        : _onSuccess(std::move(onSuccess)), _onFailure(std::move(onFailure)) {}

    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override { _onSuccess(desc); }
    void OnFailure(webrtc::RTCError error) override { _onFailure(std::move(error)); }

private:
    std::function<void(webrtc::SessionDescriptionInterface*)> _onSuccess;
    std::function<void(webrtc::RTCError)> _onFailure;
};

class GRStatsObserver : public webrtc::RTCStatsCollectorCallback {
public:
    explicit GRStatsObserver(std::function<void(rtc::scoped_refptr<const webrtc::RTCStatsReport>)> cb)
        : _cb(std::move(cb)) {}
    void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override {
        if (_cb) _cb(report);
    }
private:
    std::function<void(rtc::scoped_refptr<const webrtc::RTCStatsReport>)> _cb;
};

class GRAudioLevelSink : public webrtc::AudioTrackSinkInterface {
public:
    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override {
        if (bits_per_sample != 16 || !audio_data) return;
        const int16_t* samples = static_cast<const int16_t*>(audio_data);
        const size_t total = number_of_channels * number_of_frames;
        if (total == 0) return;

        if (!_loggedFirstSamples) {
            _loggedFirstSamples = true;
            RTC_LOG(LS_WARNING) << "GroupRef levelSink: first OnData total=" << total
                                << " sampleRate=" << sample_rate;
        }

        int32_t peak = 0;
        for (size_t i = 0; i < total; ++i) {
            int32_t s = samples[i];
            int32_t a = (s < 0) ? -s : s;
            if (a > peak) peak = a;
        }

        std::lock_guard<std::mutex> lock(_mu);
        if (peak > _runningPeak) _runningPeak = peak;
        _samplesAccumulated += total;
    }

    float consumeLevel() {
        int32_t peak;
        size_t samples;
        {
            std::lock_guard<std::mutex> lock(_mu);
            peak = _runningPeak;
            samples = _samplesAccumulated;
            _runningPeak = 0;
            _samplesAccumulated = 0;
        }
        if (samples == 0) return 0.0f;

        constexpr float kZeroDbovSinePeak = 23170.475f;
        float level = static_cast<float>(peak) / kZeroDbovSinePeak;
        if (level > 1.0f) level = 1.0f;
        return level;
    }

    void attachTo(webrtc::AudioTrackInterface* track) {
        if (_attachedTrack.get() == track) return;
        detach();
        if (track) {
            track->AddSink(this);
            _attachedTrack = webrtc::scoped_refptr<webrtc::AudioTrackInterface>(track);
        }
    }

    void detach() {
        if (_attachedTrack) {
            _attachedTrack->RemoveSink(this);
            _attachedTrack = nullptr;
        }
    }

    ~GRAudioLevelSink() override {
        detach();
    }

private:
    std::mutex _mu;
    int32_t _runningPeak{0};
    size_t _samplesAccumulated{0};
    webrtc::scoped_refptr<webrtc::AudioTrackInterface> _attachedTrack;
    bool _loggedFirstSamples = false;
};

class GRUserIdRegistry {
public:
    void setUserId(uint32_t ssrc, int64_t userId) {
        webrtc::MutexLock lock(&_mutex);
        _userIdBySsrc[ssrc] = userId;
    }

    int64_t userIdForSsrc(uint32_t ssrc) const {
        webrtc::MutexLock lock(&_mutex);
        auto it = _userIdBySsrc.find(ssrc);
        return it == _userIdBySsrc.end() ? 0 : it->second;
    }

    bool isKnown(uint32_t ssrc) const {
        webrtc::MutexLock lock(&_mutex);
        return _userIdBySsrc.find(ssrc) != _userIdBySsrc.end();
    }

private:
    mutable webrtc::Mutex _mutex;
    std::map<uint32_t, int64_t> _userIdBySsrc RTC_GUARDED_BY(_mutex);
};

class GRMyAudioLevelHolder {
public:
    void set(GroupLevelValue const &value) {
        webrtc::MutexLock lock(&_mutex);
        _value = value;
    }
    GroupLevelValue get() {
        webrtc::MutexLock lock(&_mutex);
        return _value;
    }
private:
    webrtc::Mutex _mutex;
    GroupLevelValue _value;
};

class GRAudioFrameTransformer : public webrtc::FrameTransformerInterface {
public:
    using SsrcCallback = std::function<void(uint32_t ssrc)>;

    GRAudioFrameTransformer(SsrcCallback onNewSsrc,
                            GroupEncryptDecryptFunction e2eEncryptDecrypt,
                            std::shared_ptr<GRUserIdRegistry> userIds,
                            std::map<int32_t, FrameTransformerPayloadType> payloadTypeMapping)
        : _onNewSsrc(std::move(onNewSsrc))
        , _e2eEncryptDecrypt(std::move(e2eEncryptDecrypt))
        , _userIds(std::move(userIds))
        , _payloadTypeMapping(std::move(payloadTypeMapping)) {}

    void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
        if (!frame) return;
        const uint32_t ssrc = frame->GetSsrc();

        bool notifyDiscovery = false;
        rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink;
        {
            webrtc::MutexLock lock(&_mu);
            if (_seen.size() < kMaxSeen && _seen.insert(ssrc).second) {
                notifyDiscovery = true;
            }
            sink = _sink;
        }

        if (!notifyDiscovery && _e2eEncryptDecrypt && _userIds && !_userIds->isKnown(ssrc)) {
            notifyDiscovery = true;
        }

        if (notifyDiscovery && _onNewSsrc) {
            _onNewSsrc(ssrc);
        }
        if (!sink) {
            return;
        }

        if (!_e2eEncryptDecrypt) {
            sink->OnTransformedFrame(std::move(frame));
            return;
        }

        if (!_userIds || !_userIds->isKnown(ssrc)) {
            return;
        }
        const int64_t userId = _userIds->userIdForSsrc(ssrc);

        FrameTransformerPayloadType payloadType = FrameTransformerPayloadType::Unknown;
        const auto found = _payloadTypeMapping.find(frame->GetPayloadType());
        if (found != _payloadTypeMapping.end()) {
            payloadType = found->second;
        }
        if (payloadType != FrameTransformerPayloadType::Opus) {
            return;
        }

        auto result = decryptGroupAudioFrame(_e2eEncryptDecrypt, userId, frame->GetData(),
                                             nullptr, nullptr, nullptr);
        if (result.empty()) {
            return;
        }
        frame->SetData(result);
        sink->OnTransformedFrame(std::move(frame));
    }

    void RegisterTransformedFrameCallback(
            rtc::scoped_refptr<webrtc::TransformedFrameCallback> cb) override {
        webrtc::MutexLock lock(&_mu);
        _sink = std::move(cb);
    }
    void RegisterTransformedFrameSinkCallback(
            rtc::scoped_refptr<webrtc::TransformedFrameCallback>,
            uint32_t) override {}
    void UnregisterTransformedFrameCallback() override {
        webrtc::MutexLock lock(&_mu);
        _sink = nullptr;
    }
    void UnregisterTransformedFrameSinkCallback(uint32_t) override {}

private:
    static constexpr size_t kMaxSeen = 256;

    SsrcCallback _onNewSsrc;
    GroupEncryptDecryptFunction _e2eEncryptDecrypt;
    std::shared_ptr<GRUserIdRegistry> _userIds;
    std::map<int32_t, FrameTransformerPayloadType> _payloadTypeMapping;

    webrtc::Mutex _mu;
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> _sink RTC_GUARDED_BY(_mu);
    std::set<uint32_t> _seen RTC_GUARDED_BY(_mu);
};

class GRPerReceiverAudioTransformer : public webrtc::FrameTransformerInterface {
public:
    void Transform(std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
        if (!frame) return;
        const uint32_t ssrc = frame->GetSsrc();
        rtc::scoped_refptr<webrtc::TransformedFrameCallback> sink;
        bool logFirst = false;
        {
            webrtc::MutexLock lock(&_mu);
            sink = _sink;
            if (!_loggedFirstTransform) {
                _loggedFirstTransform = true;
                logFirst = true;
            }
        }
        if (logFirst) {
            RTC_LOG(LS_WARNING) << "GroupRef perRecv[" << this << "]: first Transform ssrc="
                                << ssrc << " sink=" << (sink ? "ok" : "null");
        }
        if (sink) sink->OnTransformedFrame(std::move(frame));
    }

    void RegisterTransformedFrameCallback(
            rtc::scoped_refptr<webrtc::TransformedFrameCallback> cb) override {
        RTC_LOG(LS_WARNING) << "GroupRef perRecv[" << this << "]: Register callback="
                            << cb.get();
        webrtc::MutexLock lock(&_mu);
        _sink = std::move(cb);
    }
    void RegisterTransformedFrameSinkCallback(
            rtc::scoped_refptr<webrtc::TransformedFrameCallback> cb,
            uint32_t ssrc) override {
        RTC_LOG(LS_WARNING) << "GroupRef perRecv[" << this << "]: RegisterSink ssrc="
                            << ssrc << " callback=" << cb.get();
    }
    void UnregisterTransformedFrameCallback() override {
        RTC_LOG(LS_WARNING) << "GroupRef perRecv[" << this << "]: Unregister";
        webrtc::MutexLock lock(&_mu);
        _sink = nullptr;
    }
    void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override {
        RTC_LOG(LS_WARNING) << "GroupRef perRecv[" << this << "]: UnregisterSink ssrc=" << ssrc;
    }

private:
    webrtc::Mutex _mu;
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> _sink RTC_GUARDED_BY(_mu);
    bool _loggedFirstTransform RTC_GUARDED_BY(_mu) = false;
};

class GRVideoSinkProxy : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    void OnFrame(const webrtc::VideoFrame& frame) override {
        std::lock_guard<std::mutex> lock(_mu);
        for (int i = static_cast<int>(_sinks.size()) - 1; i >= 0; --i) {
            if (auto strong = _sinks[i].lock()) {
                strong->OnFrame(frame);
            } else {
                _sinks.erase(_sinks.begin() + i);
            }
        }
    }

    void OnDiscardedFrame() override {
        std::lock_guard<std::mutex> lock(_mu);
        for (int i = static_cast<int>(_sinks.size()) - 1; i >= 0; --i) {
            if (auto strong = _sinks[i].lock()) {
                strong->OnDiscardedFrame();
            } else {
                _sinks.erase(_sinks.begin() + i);
            }
        }
    }

    void addSink(std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> sink) {
        auto incoming = sink.lock();
        if (!incoming) return;
        std::lock_guard<std::mutex> lock(_mu);
        for (int i = static_cast<int>(_sinks.size()) - 1; i >= 0; --i) {
            auto strong = _sinks[i].lock();
            if (!strong) {
                _sinks.erase(_sinks.begin() + i);
            } else if (strong.get() == incoming.get()) {
                return;
            }
        }
        _sinks.push_back(std::move(sink));
    }

    bool hasLiveSinks() {
        std::lock_guard<std::mutex> lock(_mu);
        for (const auto& weak : _sinks) {
            if (!weak.expired()) return true;
        }
        return false;
    }

private:
    std::mutex _mu;
    std::vector<std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>>> _sinks;
};

}

class GroupInstanceReferenceInternal : public std::enable_shared_from_this<GroupInstanceReferenceInternal> {
private:
    struct VideoSinkProxyEntry;

public:
    GroupInstanceReferenceInternal(GroupInstanceDescriptor &&descriptor, std::shared_ptr<Threads> threads)
        : _threads(std::move(threads))
        , _networkStateUpdated(std::move(descriptor.networkStateUpdated))
        , _audioLevelsUpdated(std::move(descriptor.audioLevelsUpdated))
        , _createAudioDeviceModule(std::move(descriptor.createAudioDeviceModule))
        , _createWrappedAudioDeviceModule(std::move(descriptor.createWrappedAudioDeviceModule))
        , _onMutedSpeechActivityDetected(std::move(descriptor.onMutedSpeechActivityDetected))
        , _requestMediaChannelDescriptions(std::move(descriptor.requestMediaChannelDescriptions))
        , _outgoingAudioBitrateKbit(descriptor.outgoingAudioBitrateKbit)
        , _disableAudioInput(descriptor.disableAudioInput)
        , _enableSystemMute(descriptor.ios_enableSystemMute)
        , _videoContentType(descriptor.videoContentType)
        , _videoCodecPreferences(std::move(descriptor.videoCodecPreferences))
        , _getVideoSource(std::move(descriptor.getVideoSource))
        , _dataChannelMessageReceived(std::move(descriptor.dataChannelMessageReceived))
        , _minOutgoingVideoBitrateKbit(descriptor.minOutgoingVideoBitrateKbit)
        , _e2eEncryptDecrypt(std::move(descriptor.e2eEncryptDecrypt))
        , _userIds(std::make_shared<GRUserIdRegistry>())
    {
        if (_e2eEncryptDecrypt) {
            _myAudioLevelAndSpeech = std::make_shared<AudioLevelAndSpeechHolder>();
        }

        _myAudioLevel = std::make_shared<GRMyAudioLevelHolder>();
        _noiseSuppressionConfiguration =
            std::make_shared<NoiseSuppressionConfiguration>(descriptor.initialEnableNoiseSuppression);

        _payloadTypeMapping.insert(std::make_pair(111, FrameTransformerPayloadType::Opus));
        _payloadTypeMapping.insert(std::make_pair(100, FrameTransformerPayloadType::VP8));
        _payloadTypeMapping.insert(std::make_pair(104, FrameTransformerPayloadType::H264));
    }

    ~GroupInstanceReferenceInternal() {
        detachAllVideoSinkProxies();
        if (_peerConnection) {
            _peerConnection->Close();
        }
        _threads->getWorkerThread()->BlockingCall([this]() {
            _audioDeviceModule = nullptr;
        });
    }

    webrtc::scoped_refptr<WrappedAudioDeviceModule> createAudioDeviceModule(webrtc::TaskQueueFactory *taskQueueFactory) {
        auto onMutedSpeechActivityDetected = _onMutedSpeechActivityDetected;
#ifdef WEBRTC_IOS
        bool disableRecording = _disableAudioInput;
        bool enableSystemMute = _enableSystemMute;
#endif
        const auto create = [&](webrtc::AudioDeviceModule::AudioLayer layer) {
#ifdef WEBRTC_IOS
            auto result = rtc::make_ref_counted<webrtc::tgcalls_ios_adm::AudioDeviceModuleIOS>(false, disableRecording, enableSystemMute, disableRecording ? 2 : 1);
            if (result) {
                result->mutedSpeechDetectionChanged = ^(bool value) {
                    if (onMutedSpeechActivityDetected) {
                        onMutedSpeechActivityDetected(value);
                    }
                };
            }
            return result;
#else
            return webrtc::AudioDeviceModule::Create(layer, taskQueueFactory);
#endif
        };
        const auto check = [&](const webrtc::scoped_refptr<webrtc::AudioDeviceModule> &result) -> webrtc::scoped_refptr<WrappedAudioDeviceModule> {
            if (!result) {
                return nullptr;
            }
            if (result->Init() == 0) {
                return PlatformInterface::SharedInstance()->wrapAudioDeviceModule(result);
            } else {
                return nullptr;
            }
        };
        if (_createWrappedAudioDeviceModule) {
            auto result = _createWrappedAudioDeviceModule(taskQueueFactory);
            if (result) {
                return result;
            }
        }
        if (_createAudioDeviceModule) {
            if (const auto result = check(_createAudioDeviceModule(taskQueueFactory))) {
                return result;
            }
        }
        return check(create(webrtc::AudioDeviceModule::kPlatformDefaultAudio));
    }

    void start() {
        const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());

        auto taskQueueFactory = webrtc::CreateDefaultTaskQueueFactory();
        _threads->getWorkerThread()->BlockingCall([this, taskQueueFactoryPtr = taskQueueFactory.get()]() {
            _audioDeviceModule = createAudioDeviceModule(taskQueueFactoryPtr);
        });

        webrtc::PeerConnectionFactoryDependencies deps;
        deps.network_thread = _threads->getNetworkThread();
        deps.signaling_thread = _threads->getMediaThread();
        deps.worker_thread = _threads->getWorkerThread();
        deps.task_queue_factory = std::move(taskQueueFactory);
        deps.adm = _audioDeviceModule;

        webrtc::AudioProcessingBuilder builder;
#if USE_RNNOISE

        if (_audioLevelsUpdated) {
            builder.SetCapturePostProcessing(std::make_unique<AudioCapturePostProcessor>(
                [myAudioLevel = _myAudioLevel](GroupLevelValue const &level) {
                    if (myAudioLevel) {
                        myAudioLevel->set(level);
                    }
                },
                _noiseSuppressionConfiguration, nullptr, nullptr));
        }
#endif
        deps.audio_processing = builder.Create();

        deps.audio_encoder_factory = webrtc::CreateAudioEncoderFactory<webrtc::AudioEncoderOpus>();
        deps.audio_decoder_factory = webrtc::CreateAudioDecoderFactory<webrtc::AudioDecoderOpus>();

        deps.video_encoder_factory = PlatformInterface::SharedInstance()->makeVideoEncoderFactory(false, false);
        deps.video_decoder_factory = PlatformInterface::SharedInstance()->makeVideoDecoderFactory();

        webrtc::EnableMedia(deps);

        deps.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(deps.task_queue_factory.get());

        _peerConnectionFactory = webrtc::CreateModularPeerConnectionFactory(std::move(deps));
        if (!_peerConnectionFactory) {
            RTC_LOG(LS_ERROR) << "GroupRef: Failed to create PeerConnectionFactory";
            return;
        }

        {
            webrtc::PeerConnectionFactoryInterface::Options factoryOptions;
            factoryOptions.network_ignore_mask = 0;
            _peerConnectionFactory->SetOptions(factoryOptions);
        }

        _peerConnectionObserver = std::make_unique<GRPeerConnectionObserver>();

        _peerConnectionObserver->onConnectionChange = [weak, threads = _threads](
            webrtc::PeerConnectionInterface::IceConnectionState state) {
            threads->getMediaThread()->PostTask([weak, state]() {
                if (auto strong = weak.lock()) {
                    strong->onIceConnectionChange(state);
                }
            });
        };

        _peerConnectionObserver->onTrack = [weak, threads = _threads](
            webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
            threads->getMediaThread()->PostTask([weak, transceiver]() {
                if (auto strong = weak.lock()) {
                    strong->onTrackAdded(transceiver);
                }
            });
        };

        webrtc::PeerConnectionInterface::RTCConfiguration config;
        config.type = webrtc::PeerConnectionInterface::IceTransportsType::kAll;
        config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
        config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
        config.rtcp_mux_policy = webrtc::PeerConnectionInterface::RtcpMuxPolicy::kRtcpMuxPolicyRequire;
        config.continual_gathering_policy = webrtc::PeerConnectionInterface::ContinualGatheringPolicy::GATHER_CONTINUALLY;
        config.audio_jitter_buffer_fast_accelerate = true;

        webrtc::PeerConnectionDependencies pcDeps(nullptr);
        pcDeps.observer = _peerConnectionObserver.get();

        _networkMonitorFactory = PlatformInterface::SharedInstance()->createNetworkMonitorFactory();
        _socketFactory = std::make_unique<rtc::BasicPacketSocketFactory>(_threads->getNetworkThread()->socketserver());
        _networkManager = std::make_unique<rtc::BasicNetworkManager>(_networkMonitorFactory.get(), _threads->getNetworkThread()->socketserver());
        pcDeps.allocator = std::make_unique<cricket::BasicPortAllocator>(_networkManager.get(), _socketFactory.get());

        auto pcOrError = _peerConnectionFactory->CreatePeerConnectionOrError(config, std::move(pcDeps));
        if (!pcOrError.ok()) {
            RTC_LOG(LS_ERROR) << "GroupRef: Failed to create PeerConnection: " << pcOrError.error().message();
            return;
        }
        _peerConnection = pcOrError.value();

        if (_videoContentType != VideoContentType::None) {
            for (int i = 0; i < 3; i++) {
                SimulcastLayer layer;
                layer.ssrc = rtc::CreateRandomId();
                layer.fidSsrc = rtc::CreateRandomId();
                _outgoingVideoSsrcs.push_back(layer);
            }
        }

        webrtc::DataChannelInit dcInit;
        auto dcOrError = _peerConnection->CreateDataChannelOrError("data", &dcInit);
        if (dcOrError.ok()) {
            _dataChannel = dcOrError.value();
            setupDataChannel();
        }

        cricket::AudioOptions audioOpts;
        auto audioSource = _peerConnectionFactory->CreateAudioSource(audioOpts);
        auto audioTrack = _peerConnectionFactory->CreateAudioTrack("audio0", audioSource.get());

        webrtc::RtpTransceiverInit transceiverInit;
        transceiverInit.stream_ids = {"0"};

        auto result = _peerConnection->AddTransceiver(audioTrack, transceiverInit);
        if (result.ok()) {
            _outgoingAudioTransceiver = result.value();
            _outgoingAudioTrack = audioTrack;

            webrtc::RtpParameters params = _outgoingAudioTransceiver->sender()->GetParameters();
            if (params.encodings.empty()) {
                params.encodings.push_back(webrtc::RtpEncodingParameters());
            }
            params.encodings[0].max_bitrate_bps = _outgoingAudioBitrateKbit * 1024;
            _outgoingAudioTransceiver->sender()->SetParameters(params);

            if (_e2eEncryptDecrypt) {

                auto myAudioLevelAndSpeech = _myAudioLevelAndSpeech;
                _outgoingAudioTransceiver->sender()->SetEncoderToPacketizerFrameTransformer(
                    rtc::make_ref_counted<FrameTransformer>(
                        true, _e2eEncryptDecrypt, int64_t(), _payloadTypeMapping,
                        [myAudioLevelAndSpeech]() -> std::pair<uint8_t, bool> {
                            if (myAudioLevelAndSpeech) {
                                return myAudioLevelAndSpeech->get();
                            }
                            return std::make_pair(0, false);
                        },
                        nullptr));
            }

            _audioFrameTransformer = rtc::make_ref_counted<GRAudioFrameTransformer>(
                [weak, threads = _threads](uint32_t ssrc) {
                    threads->getMediaThread()->PostTask([weak, ssrc]() {
                        if (auto strong = weak.lock()) {
                            strong->handleDiscoveredAudioSsrc(ssrc);
                        }
                    });
                },
                _e2eEncryptDecrypt, _userIds, _payloadTypeMapping);
            _outgoingAudioTransceiver->receiver()
                ->SetDepacketizerToDecoderFrameTransformer(_audioFrameTransformer);

            startStatsLogging();

            _outgoingAudioTrack->set_enabled(false);
        }

        if (_videoContentType != VideoContentType::None && !_outgoingVideoSsrcs.empty()) {
            webrtc::RtpTransceiverInit videoInit;
            videoInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;
            videoInit.stream_ids = {"video"};

            auto videoResult = _peerConnection->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, videoInit);
            if (videoResult.ok()) {
                _outgoingVideoTransceiver = videoResult.value();
                if (_e2eEncryptDecrypt) {

                    _outgoingVideoTransceiver->sender()->SetEncoderToPacketizerFrameTransformer(
                        rtc::make_ref_counted<FrameTransformer>(
                            true, _e2eEncryptDecrypt, int64_t(), _payloadTypeMapping,
                            nullptr, nullptr));
                }
                RTC_LOG(LS_INFO) << "GroupRef: Added outgoing video transceiver (no track yet)";
            }
        }

        RTC_LOG(LS_INFO) << "GroupRef: PeerConnection created successfully";
    }

    void emitJoinPayload(std::function<void(GroupJoinPayload const &)> completion) {
        _joinCompletion = std::move(completion);

        auto observer = rtc::make_ref_counted<GRCreateSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](
                webrtc::SessionDescriptionInterface* desc) {
                auto strong = weak.lock();
                if (!strong) return;
                strong->_threads->getMediaThread()->PostTask([weak, ownedDesc = std::unique_ptr<webrtc::SessionDescriptionInterface>(desc->Clone())]() mutable {
                    if (auto s = weak.lock()) {
                        s->onLocalOfferCreated(std::move(ownedDesc));
                    }
                });
            },
            [](webrtc::RTCError error) {
                RTC_LOG(LS_ERROR) << "GroupRef: CreateOffer failed: " << error.message();
            }
        );

        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offerOptions;
        _peerConnection->CreateOffer(observer.get(), offerOptions);
    }

    void onLocalOfferCreated(std::unique_ptr<webrtc::SessionDescriptionInterface> offer) {

        mungeVideoSsrcsInOffer(offer.get());
        mungeVideoCodecsInOffer(offer.get());

        auto* rawOffer = offer.release();
        auto observer = rtc::make_ref_counted<GRSetSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](webrtc::RTCError error) {
                if (!error.ok()) {
                    RTC_LOG(LS_ERROR) << "GroupRef: SetLocalDescription failed: " << error.message();
                    return;
                }
                auto strong = weak.lock();
                if (!strong) return;
                strong->_threads->getMediaThread()->PostTask([weak]() {
                    if (auto s = weak.lock()) {
                        s->onLocalDescriptionSet();
                    }
                });
            }
        );
        _peerConnection->SetLocalDescription(observer.get(), rawOffer);
    }

    void onLocalDescriptionSet() {
        auto localDesc = _peerConnection->local_description();
        if (!localDesc) {
            RTC_LOG(LS_ERROR) << "GroupRef: local_description is null after SetLocalDescription";
            return;
        }

        auto* cricketDesc = localDesc->description();
        if (!cricketDesc || cricketDesc->contents().empty()) {
            RTC_LOG(LS_ERROR) << "GroupRef: empty local description";
            return;
        }

        const auto& firstContent = cricketDesc->contents()[0];
        const auto* transportInfo = cricketDesc->GetTransportInfoByName(firstContent.name);
        if (!transportInfo) {
            RTC_LOG(LS_ERROR) << "GroupRef: no transport info in local description";
            return;
        }

        std::string ufrag = transportInfo->description.ice_ufrag;
        std::string pwd = transportInfo->description.ice_pwd;

        std::string fingerprintHash;
        std::string fingerprintValue;
        if (transportInfo->description.identity_fingerprint) {
            fingerprintHash = transportInfo->description.identity_fingerprint->algorithm;
            fingerprintValue = transportInfo->description.identity_fingerprint->GetRfc4572Fingerprint();
        }

        uint32_t audioSsrc = 0;
        auto* audioDesc = firstContent.media_description();
        if (audioDesc && !audioDesc->streams().empty()) {
            audioSsrc = audioDesc->streams()[0].first_ssrc();
        }

        _localUfrag = ufrag;
        _localPwd = pwd;
        _outgoingSsrc = audioSsrc;

        GroupJoinInternalPayload internalPayload;
        internalPayload.audioSsrc = audioSsrc;
        internalPayload.transport.ufrag = ufrag;
        internalPayload.transport.pwd = pwd;

        GroupJoinTransportDescription::Fingerprint fp;
        fp.hash = fingerprintHash;
        fp.fingerprint = fingerprintValue;
        fp.setup = "passive";
        internalPayload.transport.fingerprints.push_back(fp);

        if (_videoContentType != VideoContentType::None && !_outgoingVideoSsrcs.empty()) {
            GroupParticipantVideoInformation videoInfo;

            GroupJoinPayloadVideoSourceGroup simGroup;
            simGroup.semantics = "SIM";
            for (const auto& layer : _outgoingVideoSsrcs) {
                simGroup.ssrcs.push_back(layer.ssrc);
            }
            videoInfo.ssrcGroups.push_back(std::move(simGroup));

            for (const auto& layer : _outgoingVideoSsrcs) {
                GroupJoinPayloadVideoSourceGroup fidGroup;
                fidGroup.semantics = "FID";
                fidGroup.ssrcs = {layer.ssrc, layer.fidSsrc};
                videoInfo.ssrcGroups.push_back(std::move(fidGroup));
            }

            internalPayload.videoInformation = std::move(videoInfo);
        }

        GroupJoinPayload payload;
        payload.audioSsrc = audioSsrc;
        payload.json = internalPayload.serialize();

        if (_joinCompletion) {
            _joinCompletion(payload);
            _joinCompletion = nullptr;
        }
    }

    void setJoinResponsePayload(std::string const &payload) {

        auto parsed = GroupJoinResponsePayload::parse(payload);
        if (!parsed) {
            RTC_LOG(LS_ERROR) << "GroupRef: Failed to parse join response";
            return;
        }

        _remoteTransport = parsed->transport;

        auto remoteAnswer = buildRemoteAnswer();
        if (!remoteAnswer) {
            RTC_LOG(LS_ERROR) << "GroupRef: Failed to build remote answer";
            return;
        }

        auto observer = rtc::make_ref_counted<GRSetSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](webrtc::RTCError error) {
                if (!error.ok()) {
                    RTC_LOG(LS_ERROR) << "GroupRef: SetRemoteDescription failed: " << error.message();
                    return;
                }
                auto strong = weak.lock();
                if (!strong) return;
                strong->_threads->getMediaThread()->PostTask([weak]() {
                    if (auto s = weak.lock()) {
                        s->addRemoteIceCandidates();
                    }
                });
            }
        );
        _peerConnection->SetRemoteDescription(observer.get(), remoteAnswer.release());
    }

    void addRemoteIceCandidates() {
        if (!_peerConnection) return;

        std::string bundleMid = "0";
        auto localDesc = _peerConnection->local_description();
        if (localDesc && !localDesc->description()->contents().empty()) {
            bundleMid = localDesc->description()->contents()[0].name;
        }

        for (const auto& candidate : _remoteTransport.candidates) {
            int port = 0;
            try { port = std::stoi(candidate.port); } catch (...) { continue; }
            int priority = 0;
            try { priority = std::stoi(candidate.priority); } catch (...) {}

            cricket::Candidate c;
            c.set_foundation(candidate.foundation);
            c.set_component(std::stoi(candidate.component));
            c.set_protocol(candidate.protocol);
            c.set_priority(priority);
            c.set_address(rtc::SocketAddress(candidate.ip, port));
            c.set_type(mapIceCandidateTypeToInternal(candidate.type));

            auto iceCandidate = webrtc::CreateIceCandidate(bundleMid, 0, c);
            if (iceCandidate) {
                if (!_peerConnection->AddIceCandidate(iceCandidate.get())) {
                    RTC_LOG(LS_WARNING) << "GroupRef: Failed to add ICE candidate " << candidate.ip << ":" << candidate.port;
                } else {
                    RTC_LOG(LS_INFO) << "GroupRef: Added ICE candidate " << candidate.ip << ":" << candidate.port;
                }
            }
        }

        if (_getVideoSource && _videoContentType != VideoContentType::None && !_outgoingVideoTrack) {
            setVideoSource(_getVideoSource);
        }

        onJoined();
    }

    void onJoined() {
        if (_isJoined) return;
        _isJoined = true;

        if (_hasPendingRequestedVideoChannels) {
            applyRequestedVideoChannels();
        }
        if (_pendingRenegotiation && !_isRenegotiating) {
            _pendingRenegotiation = false;
            renegotiate();
        }
    }

    std::unique_ptr<webrtc::SessionDescriptionInterface> buildRemoteAnswer() {
        auto localDesc = _peerConnection->local_description();
        if (!localDesc || !localDesc->description()) {
            RTC_LOG(LS_ERROR) << "GroupRef: No local description available for building answer";
            return nullptr;
        }

        auto* localCricketDesc = localDesc->description();
        auto cricketDesc = std::make_unique<cricket::SessionDescription>();
        std::vector<std::string> bundleMids;

        cricket::TransportDescription transportDesc;
        transportDesc.ice_ufrag = _remoteTransport.ufrag;
        transportDesc.ice_pwd = _remoteTransport.pwd;
        transportDesc.ice_mode = cricket::ICEMODE_LITE;

        if (!_remoteTransport.fingerprints.empty()) {
            auto& fp = _remoteTransport.fingerprints[0];
            auto fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(fp.hash, fp.fingerprint);
            if (fingerprint) {
                transportDesc.identity_fingerprint = std::move(fingerprint);
            }

            transportDesc.connection_role = cricket::CONNECTIONROLE_ACTIVE;
        }

        std::map<std::string, uint32_t> midToSsrc;
        for (const auto& [ssrc, info] : _remoteSsrcs) {
            if (info.transceiver) {
                auto mid = info.transceiver->mid();
                if (mid.has_value()) {
                    midToSsrc[mid.value()] = ssrc;
                }
            }
        }

        bool isFirstAudio = true;
        for (const auto& localContent : localCricketDesc->contents()) {
            const std::string& mid = localContent.name;
            auto* localMedia = localContent.media_description();
            if (!localMedia) continue;

            if (localMedia->type() == cricket::MEDIA_TYPE_DATA) {

                auto dataContent = localMedia->Clone();
                dataContent->set_direction(webrtc::RtpTransceiverDirection::kSendRecv);

                cricket::ContentInfo ci(localContent.type);
                ci.name = mid;
                ci.rejected = false;
                ci.bundle_only = false;
                ci.set_media_description(std::move(dataContent));

                cricketDesc->AddContent(std::move(ci));
                cricketDesc->AddTransportInfo(cricket::TransportInfo(mid, transportDesc));
                bundleMids.push_back(mid);

            } else if (localMedia->type() == cricket::MEDIA_TYPE_AUDIO) {
                auto audioContent = std::make_unique<cricket::AudioContentDescription>();

                cricket::AudioCodec opus = cricket::CreateAudioCodec(111, "opus", 48000, 2);
                opus.params["minptime"] = "10";
                opus.params["useinbandfec"] = "1";
                audioContent->AddCodec(opus);
                audioContent->set_rtcp_mux(true);

                if (isFirstAudio) {

                    isFirstAudio = false;

                    for (const auto& ext : localMedia->rtp_header_extensions()) {
                        if (ext.uri == webrtc::RtpExtension::kMidUri) {
                            continue;
                        }
                        audioContent->AddRtpHeaderExtension(ext);
                    }

                    audioContent->set_direction(webrtc::RtpTransceiverDirection::kSendRecv);
                } else {

                    audioContent->AddRtpHeaderExtension(webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri, 1));
                    audioContent->AddRtpHeaderExtension(webrtc::RtpExtension(webrtc::RtpExtension::kAbsSendTimeUri, 2));
                    audioContent->AddRtpHeaderExtension(webrtc::RtpExtension(webrtc::RtpExtension::kTransportSequenceNumberUri, 3));

                    audioContent->set_direction(webrtc::RtpTransceiverDirection::kSendOnly);

                    auto ssrcIt = midToSsrc.find(mid);
                    if (ssrcIt != midToSsrc.end()) {
                        cricket::StreamParams stream;
                        stream.cname = "sfu-audio";
                        stream.add_ssrc(ssrcIt->second);
                        audioContent->AddStream(stream);
                    }
                }

                cricket::ContentInfo ci(cricket::MediaProtocolType::kRtp);
                ci.name = mid;
                ci.rejected = false;
                ci.bundle_only = false;
                ci.set_media_description(std::move(audioContent));

                cricketDesc->AddContent(std::move(ci));
                cricketDesc->AddTransportInfo(cricket::TransportInfo(mid, transportDesc));
                bundleMids.push_back(mid);

            } else if (localMedia->type() == cricket::MEDIA_TYPE_VIDEO) {
                auto videoContent = std::make_unique<cricket::VideoContentDescription>();

                cricket::VideoCodec h264 = cricket::CreateVideoCodec(104, "H264");
                h264.SetParam("level-asymmetry-allowed", "1");
                h264.SetParam("packetization-mode", "1");
                h264.SetParam("profile-level-id", "42e01f");
                h264.AddFeedbackParam(cricket::FeedbackParam("nack"));
                h264.AddFeedbackParam(cricket::FeedbackParam("nack", "pli"));
                h264.AddFeedbackParam(cricket::FeedbackParam("ccm", "fir"));
                h264.AddFeedbackParam(cricket::FeedbackParam("goog-remb"));
                h264.AddFeedbackParam(cricket::FeedbackParam("transport-cc"));

                cricket::VideoCodec rtx = cricket::CreateVideoCodec(105, "rtx");
                rtx.SetParam("apt", "104");

                videoContent->AddCodec(h264);
                videoContent->AddCodec(rtx);
                videoContent->set_rtcp_mux(true);

                bool isOutgoing = (_outgoingVideoTransceiver &&
                                   _outgoingVideoTransceiver->mid().has_value() &&
                                   _outgoingVideoTransceiver->mid().value() == mid);

                for (const auto& ext : localMedia->rtp_header_extensions()) {
                    if (ext.uri == webrtc::RtpExtension::kMidUri) {
                        continue;
                    }
                    videoContent->AddRtpHeaderExtension(ext);
                }

                if (isOutgoing) {

                    videoContent->set_direction(webrtc::RtpTransceiverDirection::kRecvOnly);
                } else {
                    videoContent->set_direction(webrtc::RtpTransceiverDirection::kSendOnly);

                    for (const auto& [epId, ep] : _remoteVideoEndpoints) {
                        if (ep.transceiver && ep.transceiver->mid().has_value() &&
                            ep.transceiver->mid().value() == mid) {

                            cricket::StreamParams stream;
                            stream.cname = "sfu-video";
                            std::vector<uint32_t> allSsrcs;

                            for (const auto& group : ep.ssrcGroups) {
                                cricket::SsrcGroup cricketGroup(group.semantics, group.ssrcs);
                                stream.ssrc_groups.push_back(cricketGroup);
                                for (uint32_t s : group.ssrcs) {
                                    if (std::find(allSsrcs.begin(), allSsrcs.end(), s) == allSsrcs.end()) {
                                        allSsrcs.push_back(s);
                                    }
                                }
                            }
                            for (uint32_t s : allSsrcs) {
                                stream.add_ssrc(s);
                            }

                            videoContent->AddStream(stream);
                            break;
                        }
                    }
                }

                cricket::ContentInfo ci(cricket::MediaProtocolType::kRtp);
                ci.name = mid;
                ci.rejected = false;
                ci.bundle_only = false;
                ci.set_media_description(std::move(videoContent));

                cricketDesc->AddContent(std::move(ci));
                cricketDesc->AddTransportInfo(cricket::TransportInfo(mid, transportDesc));
                bundleMids.push_back(mid);
            }
        }

        if (!bundleMids.empty()) {
            cricket::ContentGroup bundleGroup(cricket::GROUP_TYPE_BUNDLE);
            for (const auto& name : bundleMids) {
                bundleGroup.AddContentName(name);
            }
            cricketDesc->AddGroup(bundleGroup);
        }

        auto jsepAnswer = std::make_unique<webrtc::JsepSessionDescription>(
            webrtc::SdpType::kAnswer,
            std::move(cricketDesc),
            "0", "0");

        if (!bundleMids.empty()) {
            for (const auto& candidate : _remoteTransport.candidates) {
                int port = std::stoi(candidate.port);
                int priority = 0;
                try { priority = std::stoi(candidate.priority); } catch (...) {}

                cricket::Candidate c;
                c.set_foundation(candidate.foundation);
                c.set_component(std::stoi(candidate.component));
                c.set_protocol(candidate.protocol);
                c.set_priority(priority);
                c.set_address(rtc::SocketAddress(candidate.ip, port));
                c.set_type(mapIceCandidateTypeToInternal(candidate.type));

                auto iceCandidate = webrtc::CreateIceCandidate(bundleMids[0], 0, c);
                if (iceCandidate) {
                    jsepAnswer->AddCandidate(iceCandidate.get());
                }
            }
        }

        return jsepAnswer;
    }

    void setConnectionMode(GroupConnectionMode mode, bool, bool) {

    }

    void setIsMuted(bool isMuted) {
        _isMuted = isMuted;
        if (_outgoingAudioTrack) {
            _outgoingAudioTrack->set_enabled(!isMuted);
        }
    }

    void setVolume(uint32_t ssrc, double volume) {

    }

    void stop(std::function<void()> completion) {
        _isPollingAudioLevels = false;
        _isLoggingStats = false;
        detachAllVideoSinkProxies();
        if (_peerConnection) {
            _peerConnection->Close();
        }
        if (completion) {
            completion();
        }
    }

    void removeSsrcs(std::vector<uint32_t>) {}
    void removeIncomingVideoSource(uint32_t) {}
    void setIsNoiseSuppressionEnabled(bool isNoiseSuppressionEnabled) {
        if (_noiseSuppressionConfiguration) {
            _noiseSuppressionConfiguration->isEnabled = isNoiseSuppressionEnabled;
        }
    }
    void setVideoCapture(std::shared_ptr<VideoCaptureInterface>) {}
    void setVideoSource(std::function<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>()> getVideoSource) {
        if (!_peerConnection || !_peerConnectionFactory) return;
        if (!_outgoingVideoTransceiver) return;

        if (!getVideoSource) {
            if (_outgoingVideoTransceiver) {
                _outgoingVideoTransceiver->sender()->SetTrack(nullptr);
            }
            _outgoingVideoTrack = nullptr;
            return;
        }

        auto source = getVideoSource();
        if (!source) return;

        auto videoTrack = _peerConnectionFactory->CreateVideoTrack(source, "video0");
        if (!videoTrack) return;

        _outgoingVideoTrack = videoTrack;

        _outgoingVideoTransceiver->sender()->SetTrack(videoTrack.get());
    }
    void setAudioOutputDevice(std::string) {}
    void setAudioInputDevice(std::string) {}
    void addExternalAudioSamples(std::vector<uint8_t>&&) {}
    void addOutgoingVideoOutput(std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>>) {}
    void addIncomingVideoOutput(std::string const &endpointId, std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> sink) {
        auto& entry = _videoSinkProxies[endpointId];
        if (!entry.proxy) {
            entry.proxy = std::make_shared<GRVideoSinkProxy>();
        }
        entry.proxy->addSink(std::move(sink));

        attachVideoSinkProxy(endpointId, entry);
    }
    void setRequestedVideoChannels(std::vector<VideoChannelDescription>&& channels) {
        if (!_peerConnection) return;

        _requestedVideoChannels = std::move(channels);

        if (!_isJoined) {

            _hasPendingRequestedVideoChannels = true;
            RTC_LOG(LS_INFO) << "GroupRef: deferring " << _requestedVideoChannels.size()
                             << " requested video channel(s) until joined";
            return;
        }

        applyRequestedVideoChannels();
    }

    void applyRequestedVideoChannels() {
        _hasPendingRequestedVideoChannels = false;
        const std::vector<VideoChannelDescription>& channels = _requestedVideoChannels;

        bool changed = false;

        std::set<std::string> requestedEndpoints;
        for (const auto& ch : channels) {
            requestedEndpoints.insert(ch.endpointId);
        }

        for (const auto& ch : channels) {
            for (const auto& group : ch.ssrcGroups) {
                for (uint32_t s : group.ssrcs) {
                    _userIds->setUserId(s, ch.userId);
                }
            }
        }

        for (const auto& ch : channels) {
            auto existing = _remoteVideoEndpoints.find(ch.endpointId);
            if (existing != _remoteVideoEndpoints.end()) {

                if (existing->second.transceiver && !existing->second.transceiver->mid().has_value()) {
                    changed = true;
                }
                continue;
            }

            webrtc::RtpTransceiverInit init;
            init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
            init.stream_ids = {"video-" + ch.endpointId};

            auto result = _peerConnection->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
            if (!result.ok()) {
                RTC_LOG(LS_ERROR) << "GroupRef: Failed to add video transceiver for endpoint "
                                  << ch.endpointId << ": " << result.error().message();
                continue;
            }

            RemoteVideoEndpoint ep;
            ep.transceiver = result.value();

            if (_e2eEncryptDecrypt) {

                auto userIds = _userIds;
                ep.transceiver->receiver()->SetDepacketizerToDecoderFrameTransformer(
                    rtc::make_ref_counted<FrameTransformer>(
                        false, _e2eEncryptDecrypt,
                        [userIds](uint32_t frameSsrc) -> int64_t {
                            return userIds->userIdForSsrc(frameSsrc);
                        },
                        _payloadTypeMapping, nullptr, nullptr));
            }
            ep.ssrcGroups = ch.ssrcGroups;
            _remoteVideoEndpoints[ch.endpointId] = std::move(ep);
            changed = true;

            RTC_LOG(LS_INFO) << "GroupRef: Added recvonly video transceiver for endpoint " << ch.endpointId;
        }

        for (auto it = _remoteVideoEndpoints.begin(); it != _remoteVideoEndpoints.end(); ) {
            if (requestedEndpoints.find(it->first) == requestedEndpoints.end()) {
                RTC_LOG(LS_INFO) << "GroupRef: Removing video endpoint " << it->first;
                detachVideoSinkProxy(it->first);
                it = _remoteVideoEndpoints.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }

        if (changed) {
            renegotiate();
        }

        sendReceiverVideoConstraints(channels);
    }
    void getStats(std::function<void(GroupInstanceStats)> completion) {
        if (!_peerConnection) {
            if (completion) completion(GroupInstanceStats{});
            return;
        }
        const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());
        auto observer = rtc::make_ref_counted<GRStatsObserver>(
            [weak, completion = std::move(completion)](rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
                if (auto strong = weak.lock()) {
                    strong->logAudioStatsFromReport(std::move(report));
                }
                if (completion) completion(GroupInstanceStats{});
            });
        _peerConnection->GetStats(observer.get());
    }
    void internal_addCustomNetworkEvent(bool) {}

private:
    void setupDataChannel() {
        _dataChannelObserver = std::make_unique<GRDataChannelObserver>();

        _dataChannelObserver->onStateChange = [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())]() {
            auto strong = weak.lock();
            if (!strong) return;
            strong->_threads->getMediaThread()->PostTask([weak]() {
                if (auto s = weak.lock()) {
                    s->onDataChannelStateChanged();
                }
            });
        };

        _dataChannelObserver->onMessage = [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](
            webrtc::DataBuffer const &buffer) {
            if (buffer.binary) return;
            std::string msg(buffer.data.data(), buffer.data.data() + buffer.data.size());
            auto strong = weak.lock();
            if (!strong) return;
            strong->_threads->getMediaThread()->PostTask([weak, msg = std::move(msg)]() {
                if (auto s = weak.lock()) {
                    s->onDataChannelMessage(msg);
                }
            });
        };

        _dataChannel->RegisterObserver(_dataChannelObserver.get());
    }

    void onDataChannelStateChanged() {
        if (_dataChannel && _dataChannel->state() == webrtc::DataChannelInterface::DataState::kOpen) {
            const bool wasOpen = _isDataChannelOpen;
            _isDataChannelOpen = true;
            RTC_LOG(LS_INFO) << "GroupRef: Data channel open";
            if (!wasOpen && !_requestedVideoChannels.empty()) {

                sendReceiverVideoConstraints(_requestedVideoChannels);
            }
        } else {
            _isDataChannelOpen = false;
        }
    }

    void onDataChannelMessage(std::string const &msg) {

        if (_dataChannelMessageReceived) {
            _dataChannelMessageReceived(msg);
        }
    }

    static constexpr int kDiscoveryRenegotiationDelayMs = 250;

    void scheduleDiscoveryRenegotiation() {
        if (_discoveryRenegotiationScheduled) return;
        _discoveryRenegotiationScheduled = true;

        const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());
        _threads->getMediaThread()->PostDelayedTask(
            [weak]() {
                auto strong = weak.lock();
                if (!strong) return;
                strong->_discoveryRenegotiationScheduled = false;
                strong->renegotiate();
            },
            webrtc::TimeDelta::Millis(kDiscoveryRenegotiationDelayMs));
    }

    void renegotiate() {

        if (!_isJoined) {
            _pendingRenegotiation = true;
            return;
        }

        if (_isRenegotiating) {
            _pendingRenegotiation = true;
            return;
        }
        _isRenegotiating = true;

        for (auto& [ssrc, info] : _remoteSsrcs) {
            if (!info.transceiver) {
                webrtc::RtpTransceiverInit init;
                init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
                init.stream_ids = {std::to_string(ssrc)};

                auto result = _peerConnection->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
                if (result.ok()) {
                    info.transceiver = result.value();

                    if (_e2eEncryptDecrypt) {
                        auto userIds = _userIds;
                        info.perReceiverTransformer = rtc::make_ref_counted<FrameTransformer>(
                            false, _e2eEncryptDecrypt,
                            [userIds](uint32_t frameSsrc) -> int64_t {
                                return userIds->userIdForSsrc(frameSsrc);
                            },
                            _payloadTypeMapping, nullptr, nullptr);
                    } else {
                        info.perReceiverTransformer =
                            rtc::make_ref_counted<GRPerReceiverAudioTransformer>();
                    }
                    info.transceiver->receiver()
                        ->SetDepacketizerToDecoderFrameTransformer(info.perReceiverTransformer);
                    RTC_LOG(LS_WARNING) << "GroupRef: Added recvonly transceiver for SSRC " << ssrc
                                        << " perRecvTransformer=" << info.perReceiverTransformer.get()
                                        << " receiver=" << info.transceiver->receiver().get();
                }
            }
        }

        auto observer = rtc::make_ref_counted<GRCreateSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](
                webrtc::SessionDescriptionInterface* desc) {
                auto strong = weak.lock();
                if (!strong) return;
                strong->_threads->getMediaThread()->PostTask([weak, ownedDesc = std::unique_ptr<webrtc::SessionDescriptionInterface>(desc->Clone())]() mutable {
                    if (auto s = weak.lock()) {
                        s->onRenegotiationOfferCreated(std::move(ownedDesc));
                    }
                });
            },
            [](webrtc::RTCError error) {
                RTC_LOG(LS_ERROR) << "GroupRef: Renegotiation CreateOffer failed: " << error.message();
            }
        );

        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions opts;
        _peerConnection->CreateOffer(observer.get(), opts);
    }

    void mungeVideoSsrcsInOffer(webrtc::SessionDescriptionInterface* offer) {
        if (!_outgoingVideoTransceiver || _outgoingVideoSsrcs.empty()) return;

        auto* cricketDesc = offer->description();
        if (!cricketDesc) return;

        for (auto& content : cricketDesc->contents()) {
            if (!content.media_description() ||
                content.media_description()->type() != cricket::MEDIA_TYPE_VIDEO ||
                content.media_description()->direction() != webrtc::RtpTransceiverDirection::kSendOnly) {
                continue;
            }

            auto* videoDesc = content.media_description()->as_video();
            if (!videoDesc) break;

            cricket::StreamParams stream;
            stream.id = _outgoingVideoTransceiver->sender()->id();

            auto* localDesc = _peerConnection->local_description();
            if (localDesc) {
                for (const auto& c : localDesc->description()->contents()) {
                    auto* media = c.media_description();
                    if (media && media->type() == cricket::MEDIA_TYPE_AUDIO && !media->streams().empty()) {
                        stream.cname = media->streams()[0].cname;
                        break;
                    }
                }
            }

            if (stream.cname.empty()) {
                for (const auto& c : cricketDesc->contents()) {
                    auto* media = c.media_description();
                    if (media && media->type() == cricket::MEDIA_TYPE_AUDIO && !media->streams().empty()) {
                        stream.cname = media->streams()[0].cname;
                        break;
                    }
                }
            }
            if (stream.cname.empty()) {
                stream.cname = "ref-video";
            }

            std::vector<uint32_t> simSsrcs;
            for (const auto& layer : _outgoingVideoSsrcs) {
                stream.add_ssrc(layer.ssrc);
                stream.add_ssrc(layer.fidSsrc);
                simSsrcs.push_back(layer.ssrc);
                stream.ssrc_groups.push_back(
                    cricket::SsrcGroup(cricket::kFidSsrcGroupSemantics, {layer.ssrc, layer.fidSsrc}));
            }
            stream.ssrc_groups.push_back(
                cricket::SsrcGroup(cricket::kSimSsrcGroupSemantics, simSsrcs));
            stream.set_stream_ids({"video"});

            videoDesc->mutable_streams().clear();
            videoDesc->mutable_streams().push_back(stream);

            break;
        }
    }

    void mungeVideoCodecsInOffer(webrtc::SessionDescriptionInterface* offer) {
        auto* cricketDesc = offer->description();
        if (!cricketDesc) return;

        struct Slot {
            const char* name;
            int payloadType;
        };
        static constexpr Slot kSlots[] = {
            {cricket::kVp8CodecName, 100},
            {cricket::kVp9CodecName, 102},
            {cricket::kH264CodecName, 104},
        };

        for (auto& content : cricketDesc->contents()) {
            auto* media = content.media_description();
            if (!media || media->type() != cricket::MEDIA_TYPE_VIDEO) continue;
            auto* videoDesc = media->as_video();
            if (!videoDesc) continue;

            const std::vector<cricket::Codec> original = videoDesc->codecs();
            std::vector<cricket::Codec> pinned;

            for (const auto& slot : kSlots) {
                const cricket::Codec* chosen = nullptr;
                for (const auto& codec : original) {
                    if (!absl::EqualsIgnoreCase(codec.name, slot.name)) continue;

                    bool preferred = true;
                    if (absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName)) {

                        std::string profile;
                        std::string mode;
                        codec.GetParam(cricket::kH264FmtpProfileLevelId, &profile);
                        codec.GetParam(cricket::kH264FmtpPacketizationMode, &mode);
                        preferred = absl::StartsWithIgnoreCase(profile, "42e0") && mode == "1";
                    } else if (absl::EqualsIgnoreCase(codec.name, cricket::kVp9CodecName)) {
                        std::string profile;
                        preferred = !codec.GetParam("profile-id", &profile) || profile == "0";
                    }

                    if (!chosen || preferred) {
                        chosen = &codec;
                    }
                    if (preferred) break;
                }
                if (!chosen) continue;

                cricket::Codec codec = *chosen;
                codec.id = slot.payloadType;
                pinned.push_back(codec);
                pinned.push_back(cricket::CreateVideoRtxCodec(slot.payloadType + 1, slot.payloadType));
            }

            if (pinned.empty()) continue;
            videoDesc->set_codecs(pinned);
        }
    }

    void onRenegotiationOfferCreated(std::unique_ptr<webrtc::SessionDescriptionInterface> offer) {
        mungeVideoSsrcsInOffer(offer.get());
        mungeVideoCodecsInOffer(offer.get());

        auto* rawOffer = offer.release();
        auto observer = rtc::make_ref_counted<GRSetSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](webrtc::RTCError error) {
                if (!error.ok()) {
                    RTC_LOG(LS_ERROR) << "GroupRef: Renegotiation SetLocalDescription failed: " << error.message();
                    if (auto strong2 = weak.lock()) {
                        strong2->_threads->getMediaThread()->PostTask([weak]() {
                            if (auto s = weak.lock()) { s->onRenegotiationComplete(); }
                        });
                    }
                    return;
                }
                if (auto strong = weak.lock()) {
                    strong->_threads->getMediaThread()->PostTask([weak]() {
                        if (auto s = weak.lock()) {
                            s->onRenegotiationLocalDescSet();
                        }
                    });
                }
            }
        );
        _peerConnection->SetLocalDescription(observer.get(), rawOffer);
    }

    void onRenegotiationLocalDescSet() {

        auto localDesc = _peerConnection->local_description();
        if (!localDesc) return;

        for (auto& [ssrc, info] : _remoteSsrcs) {
            if (info.transceiver) {
                info.mid = info.transceiver->mid().value_or(info.mid);
            }
        }

        for (auto& [endpointId, ep] : _remoteVideoEndpoints) {
            if (ep.transceiver) {
                ep.mid = ep.transceiver->mid().value_or(ep.mid);
            }
        }

        auto remoteAnswer = buildRemoteAnswer();
        if (!remoteAnswer) {
            RTC_LOG(LS_ERROR) << "GroupRef: Failed to build renegotiation answer";
            return;
        }

        auto observer = rtc::make_ref_counted<GRSetSDPObserver>(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())](webrtc::RTCError error) {
                if (!error.ok()) {
                    RTC_LOG(LS_ERROR) << "GroupRef: Renegotiation SetRemoteDescription failed: " << error.message();
                    if (auto strong2 = weak.lock()) {
                        strong2->_threads->getMediaThread()->PostTask([weak]() {
                            if (auto s = weak.lock()) { s->onRenegotiationComplete(); }
                        });
                    }
                    return;
                }
                auto strong = weak.lock();
                if (!strong) return;
                strong->_threads->getMediaThread()->PostTask([weak]() {
                    if (auto s = weak.lock()) {
                        s->onRenegotiationComplete();
                    }
                });
            }
        );
        _peerConnection->SetRemoteDescription(observer.get(), remoteAnswer.release());
    }

    void onRenegotiationComplete() {
        wirePendingVideoSinks();
        wireRemoteAudioLevelSinks();

        _isRenegotiating = false;
        if (_pendingRenegotiation) {
            _pendingRenegotiation = false;

            bool hasUnnegotiated = false;
            for (auto& [ssrc, info] : _remoteSsrcs) {
                if (info.transceiver && !info.transceiver->mid().has_value()) {
                    hasUnnegotiated = true;
                    break;
                }
            }
            if (!hasUnnegotiated) {
                for (auto& [epId, ep] : _remoteVideoEndpoints) {
                    if (ep.transceiver && !ep.transceiver->mid().has_value()) {
                        hasUnnegotiated = true;
                        break;
                    }
                }
            }
            if (hasUnnegotiated) {
                renegotiate();
            }
        }
    }

    void wirePendingVideoSinks() {

        for (auto& [endpointId, entry] : _videoSinkProxies) {
            attachVideoSinkProxy(endpointId, entry);
        }
    }

    void attachVideoSinkProxy(const std::string& endpointId, VideoSinkProxyEntry& entry) {
        if (!entry.proxy || entry.attachedTrack) return;

        auto epIt = _remoteVideoEndpoints.find(endpointId);
        if (epIt == _remoteVideoEndpoints.end() || !epIt->second.transceiver) return;

        auto receiver = epIt->second.transceiver->receiver();
        if (!receiver || !receiver->track()) return;
        if (receiver->track()->kind() != webrtc::MediaStreamTrackInterface::kVideoKind) return;

        webrtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack(
            static_cast<webrtc::VideoTrackInterface*>(receiver->track().get()));
        videoTrack->AddOrUpdateSink(entry.proxy.get(), rtc::VideoSinkWants());
        entry.attachedTrack = videoTrack;
        RTC_LOG(LS_INFO) << "GroupRef: Attached video sink proxy for endpoint " << endpointId;
    }

    void detachVideoSinkProxy(const std::string& endpointId) {
        auto it = _videoSinkProxies.find(endpointId);
        if (it == _videoSinkProxies.end()) return;
        auto& entry = it->second;
        if (entry.attachedTrack && entry.proxy) {
            entry.attachedTrack->RemoveSink(entry.proxy.get());
        }
        entry.attachedTrack = nullptr;

        if (!entry.proxy || !entry.proxy->hasLiveSinks()) {
            _videoSinkProxies.erase(it);
        }
    }

    void detachAllVideoSinkProxies() {
        for (auto& [endpointId, entry] : _videoSinkProxies) {
            if (entry.attachedTrack && entry.proxy) {
                entry.attachedTrack->RemoveSink(entry.proxy.get());
            }
            entry.attachedTrack = nullptr;
        }
    }

    void sendReceiverVideoConstraints(const std::vector<VideoChannelDescription>& channels) {
        if (!_dataChannel || !_isDataChannelOpen) return;

        json11::Json::object constraints;
        for (const auto& ch : channels) {
            int height = 0;
            switch (ch.maxQuality) {
                case VideoChannelDescription::Quality::Thumbnail: height = 90; break;
                case VideoChannelDescription::Quality::Medium: height = 180; break;
                case VideoChannelDescription::Quality::Full: height = 360; break;
            }
            constraints[ch.endpointId] = json11::Json::object{
                {"minHeight", height},
                {"maxHeight", height}
            };
        }

        json11::Json msg = json11::Json::object{
            {"colibriClass", "ReceiverVideoConstraints"},
            {"defaultConstraints", json11::Json::object{{"maxHeight", 0}}},
            {"constraints", constraints}
        };

        std::string msgStr = msg.dump();
        webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(msgStr.data(), msgStr.size()), false);
        _dataChannel->Send(buffer);
        RTC_LOG(LS_INFO) << "GroupRef: Sent ReceiverVideoConstraints for " << channels.size() << " endpoints";
    }

    void onIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
        bool connected = (state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected ||
                         state == webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted);

        if (connected != _isConnected) {
            _isConnected = connected;
            if (connected) {
                startAudioLevelPolling();
            }
            if (_networkStateUpdated) {
                GroupNetworkState netState;
                netState.isConnected = connected;
                netState.connectionMode = GroupConnectionMode::GroupConnectionModeRtc;
                _networkStateUpdated(netState);
            }
        }
    }

    void onTrackAdded(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
        auto mid = transceiver->mid().value_or("?");
        auto kind = transceiver->receiver()->track() ? transceiver->receiver()->track()->kind() : "unknown";
        RTC_LOG(LS_INFO) << "GroupRef: Remote track added (mid=" << mid << ", kind=" << kind << ")";

        if (kind != webrtc::MediaStreamTrackInterface::kVideoKind) return;

        for (const auto& [endpointId, ep] : _remoteVideoEndpoints) {
            if (ep.transceiver && ep.transceiver->mid().has_value() &&
                ep.transceiver->mid().value() == mid) {
                auto proxyIt = _videoSinkProxies.find(endpointId);
                if (proxyIt != _videoSinkProxies.end()) {
                    attachVideoSinkProxy(endpointId, proxyIt->second);
                }
                break;
            }
        }
    }

    void logAudioStatsFromReport(rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
        for (const auto& stats : *report) {
            if (stats.type() != std::string("inbound-rtp")) continue;
            const auto& inbound = stats.cast_to<webrtc::RTCInboundRtpStreamStats>();
            if (!inbound.kind.has_value() || inbound.kind.value() != "audio") continue;
            RTC_LOG(LS_WARNING)
                << "GroupRef stats:"
                << " ssrc=" << inbound.ssrc.value_or(0)
                << " packets=" << inbound.packets_received.value_or(0)
                << " bytes=" << inbound.bytes_received.value_or(0)
                << " audioLevel=" << inbound.audio_level.value_or(0.0)
                << " totalSamples=" << inbound.total_samples_received.value_or(0)
                << " concealedSamples=" << inbound.concealed_samples.value_or(0)
                << " jitterBufferEmittedCount=" << inbound.jitter_buffer_emitted_count.value_or(0);
        }
    }

    void startStatsLogging() {
        if (_isLoggingStats) return;
        _isLoggingStats = true;
        scheduleStatsLog();
    }

    void scheduleStatsLog() {
        const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());
        _threads->getMediaThread()->PostDelayedTask(
            [weak]() {
                auto strong = weak.lock();
                if (!strong) return;
                strong->pollStatsForLogging();
                if (strong->_isLoggingStats) {
                    strong->scheduleStatsLog();
                }
            },
            webrtc::TimeDelta::Millis(kStatsLogIntervalMs));
    }

    void pollStatsForLogging() {
        if (!_peerConnection) return;
        const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());
        auto observer = rtc::make_ref_counted<GRStatsObserver>(
            [weak](rtc::scoped_refptr<const webrtc::RTCStatsReport> report) {
                if (auto strong = weak.lock()) {
                    strong->logAudioStatsFromReport(std::move(report));
                }
            });
        _peerConnection->GetStats(observer.get());
    }

    void startAudioLevelPolling() {
        if (_isPollingAudioLevels) return;
        _isPollingAudioLevels = true;
        scheduleAudioLevelPoll();
    }

    void scheduleAudioLevelPoll() {
        _threads->getMediaThread()->PostDelayedTask(
            [weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this())]() {
                if (auto strong = weak.lock()) {
                    strong->pollAudioLevels();
                    if (strong->_isPollingAudioLevels) {
                        strong->scheduleAudioLevelPoll();
                    }
                }
            },
            webrtc::TimeDelta::Millis(100));
    }

    void pollAudioLevels() {
        if (!_audioLevelsUpdated || !_peerConnection) return;

        const GroupLevelValue myLevel = _myAudioLevel ? _myAudioLevel->get() : GroupLevelValue();
        const bool isSpeaking = myLevel.voice && !_isMuted;

        GroupLevelsUpdate update;

        GroupLevelUpdate selfEntry;
        selfEntry.ssrc = 0;
        selfEntry.value.level = _isMuted ? 0.0f : myLevel.level;
        selfEntry.value.voice = isSpeaking;
        update.updates.push_back(selfEntry);

        if (_myAudioLevelAndSpeech) {

            uint8_t compressedAudioLevel = 127;
            if (myLevel.level > 0.0f) {
                float dBov = 20.0f * log10(myLevel.level);
                compressedAudioLevel = static_cast<uint8_t>(std::clamp(static_cast<int>(-dBov), 0, 127));
            }
            _myAudioLevelAndSpeech->set(compressedAudioLevel, isSpeaking);
        }

        constexpr float kVoiceThreshold = 0.02f;
        for (auto& [ssrc, info] : _remoteSsrcs) {
            if (!info.levelSink) continue;
            float level = info.levelSink->consumeLevel();
            GroupLevelUpdate entry;
            entry.ssrc = ssrc;
            entry.value.level = level;
            entry.value.voice = level >= kVoiceThreshold;
            update.updates.push_back(entry);
        }

        if (!update.updates.empty()) {
            _audioLevelsUpdated(update);
        }
    }

    void handleDiscoveredAudioSsrc(uint32_t ssrc) {
        if (ssrc == 0) return;
        if (ssrc == _outgoingSsrc) return;

        const bool isNew = _remoteSsrcs.count(ssrc) == 0;
        if (isNew) {
            std::string mid = std::to_string(_nextMid++);
            RemoteSsrcInfo info;
            info.mid = mid;
            _remoteSsrcs.emplace(ssrc, std::move(info));
            RTC_LOG(LS_INFO) << "GroupRef: queued discovered audio SSRC " << ssrc
                             << " (mid=" << mid << ")";
        }

        const bool needsUserId = _e2eEncryptDecrypt && !_userIds->isKnown(ssrc);
        if ((isNew || needsUserId) &&
            _requestMediaChannelDescriptions &&
            _pendingDescriptionRequests.insert(ssrc).second) {
            const auto weak = std::weak_ptr<GroupInstanceReferenceInternal>(shared_from_this());
            auto threads = _threads;
            _requestMediaChannelDescriptions({ssrc},
                [weak, threads, ssrc](std::vector<MediaChannelDescription> &&descriptions) {
                    threads->getMediaThread()->PostTask([weak, ssrc, descriptions]() mutable {
                        if (auto strong = weak.lock()) {
                            strong->_pendingDescriptionRequests.erase(ssrc);
                            strong->processMediaChannelDescriptions(std::move(descriptions));
                        }
                    });
                });
        }

        if (isNew) {
            scheduleDiscoveryRenegotiation();
        }
    }

    void processMediaChannelDescriptions(std::vector<MediaChannelDescription> descriptions) {
        for (const auto &description : descriptions) {
            if (description.audioSsrc == 0) {
                continue;
            }
            _userIds->setUserId(description.audioSsrc, description.userId);
            RTC_LOG(LS_INFO) << "GroupRef: ssrc " << description.audioSsrc
                             << " belongs to user " << description.userId;
        }
    }

    void wireRemoteAudioLevelSinks() {
        for (auto& [ssrc, info] : _remoteSsrcs) {
            if (info.levelSink) continue;
            if (!info.transceiver) continue;

            auto receiver = info.transceiver->receiver();
            if (!receiver) continue;
            auto track = receiver->track();
            if (!track || track->kind() != webrtc::MediaStreamTrackInterface::kAudioKind) continue;

            auto* audioTrack = static_cast<webrtc::AudioTrackInterface*>(track.get());
            info.levelSink = std::make_unique<GRAudioLevelSink>();
            info.levelSink->attachTo(audioTrack);
            RTC_LOG(LS_INFO) << "GroupRef: wired audio level sink for SSRC " << ssrc;
        }
    }

private:
    struct RemoteSsrcInfo {
        std::string mid;
        webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver;
        std::unique_ptr<GRAudioLevelSink> levelSink;
        rtc::scoped_refptr<webrtc::FrameTransformerInterface> perReceiverTransformer;
    };

    struct RemoteVideoEndpoint {
        std::string mid;
        webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver;
        std::vector<MediaSsrcGroup> ssrcGroups;
    };

    struct VideoSinkProxyEntry {
        std::shared_ptr<GRVideoSinkProxy> proxy;
        webrtc::scoped_refptr<webrtc::VideoTrackInterface> attachedTrack;
    };

    std::shared_ptr<Threads> _threads;

    std::function<void(GroupNetworkState)> _networkStateUpdated;
    std::function<void(GroupLevelsUpdate const &)> _audioLevelsUpdated;
    std::function<webrtc::scoped_refptr<webrtc::AudioDeviceModule>(webrtc::TaskQueueFactory*)> _createAudioDeviceModule;
    std::function<webrtc::scoped_refptr<WrappedAudioDeviceModule>(webrtc::TaskQueueFactory*)> _createWrappedAudioDeviceModule;
    std::function<void(bool)> _onMutedSpeechActivityDetected;
    std::function<std::shared_ptr<RequestMediaChannelDescriptionTask>(std::vector<uint32_t> const &, std::function<void(std::vector<MediaChannelDescription> &&)>)> _requestMediaChannelDescriptions;
    int _outgoingAudioBitrateKbit = 32;
    bool _disableAudioInput = false;
    bool _enableSystemMute = false;

    VideoContentType _videoContentType = VideoContentType::None;
    std::vector<VideoCodecName> _videoCodecPreferences;
    std::function<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>()> _getVideoSource;
    std::function<void(std::string const &)> _dataChannelMessageReceived;
    int _minOutgoingVideoBitrateKbit = 100;

    struct SimulcastLayer {
        uint32_t ssrc;
        uint32_t fidSsrc;
    };
    std::vector<SimulcastLayer> _outgoingVideoSsrcs;

    webrtc::scoped_refptr<webrtc::VideoTrackInterface> _outgoingVideoTrack;
    webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> _outgoingVideoTransceiver;

    std::function<void(GroupJoinPayload const &)> _joinCompletion;
    GroupJoinTransportDescription _remoteTransport;
    std::string _localUfrag;
    std::string _localPwd;

    webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _peerConnectionFactory;
    std::unique_ptr<GRPeerConnectionObserver> _peerConnectionObserver;
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
    webrtc::scoped_refptr<WrappedAudioDeviceModule> _audioDeviceModule;

    std::unique_ptr<rtc::NetworkMonitorFactory> _networkMonitorFactory;
    std::unique_ptr<rtc::BasicPacketSocketFactory> _socketFactory;
    std::unique_ptr<rtc::BasicNetworkManager> _networkManager;

    webrtc::scoped_refptr<webrtc::AudioTrackInterface> _outgoingAudioTrack;
    webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> _outgoingAudioTransceiver;

    rtc::scoped_refptr<GRAudioFrameTransformer> _audioFrameTransformer;

    webrtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
    std::unique_ptr<GRDataChannelObserver> _dataChannelObserver;
    bool _isDataChannelOpen = false;

    std::map<uint32_t, RemoteSsrcInfo> _remoteSsrcs;
    std::shared_ptr<GRUserIdRegistry> _userIds;
    std::set<uint32_t> _pendingDescriptionRequests;
    std::shared_ptr<GRMyAudioLevelHolder> _myAudioLevel;
    std::shared_ptr<NoiseSuppressionConfiguration> _noiseSuppressionConfiguration;
    bool _isMuted = true;
    GroupEncryptDecryptFunction _e2eEncryptDecrypt;
    std::map<int32_t, FrameTransformerPayloadType> _payloadTypeMapping;
    std::shared_ptr<AudioLevelAndSpeechHolder> _myAudioLevelAndSpeech;
    int _nextMid = 10;
    uint32_t _outgoingSsrc = 0;

    std::map<std::string, RemoteVideoEndpoint> _remoteVideoEndpoints;

    std::vector<VideoChannelDescription> _requestedVideoChannels;
    bool _hasPendingRequestedVideoChannels = false;

    bool _isJoined = false;

    std::map<std::string, VideoSinkProxyEntry> _videoSinkProxies;

    bool _isPollingAudioLevels = false;

    static constexpr int kStatsLogIntervalMs = 5000;
    bool _isLoggingStats = false;

    bool _isRenegotiating = false;
    bool _pendingRenegotiation = false;

    bool _discoveryRenegotiationScheduled = false;

    bool _isConnected = false;
};

GroupInstanceReferenceImpl::GroupInstanceReferenceImpl(GroupInstanceDescriptor &&descriptor) {
    if (descriptor.config.need_log) {
        _logSink = std::make_unique<LogSinkImpl>(descriptor.config.logPath);
    }

    _threads = descriptor.threads;

    _internal.reset(new ThreadLocalObject<GroupInstanceReferenceInternal>(_threads->getMediaThread(), [descriptor = std::move(descriptor), threads = _threads]() mutable {
        return std::make_shared<GroupInstanceReferenceInternal>(std::move(descriptor), threads);
    }));
    _internal->perform([](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->start();
    });
}

GroupInstanceReferenceImpl::~GroupInstanceReferenceImpl() {
    if (_logSink) {
        rtc::LogMessage::RemoveLogToStream(_logSink.get());
    }
    _internal.reset();
    _threads->getMediaThread()->BlockingCall([] {});
}

void GroupInstanceReferenceImpl::stop(std::function<void()> completion) {
    _internal->perform([completion = std::move(completion)](GroupInstanceReferenceInternal *unwrapped) mutable {
        unwrapped->stop(std::move(completion));
    });
}

void GroupInstanceReferenceImpl::setConnectionMode(GroupConnectionMode mode, bool keep, bool unified) {
    _internal->perform([mode, keep, unified](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->setConnectionMode(mode, keep, unified);
    });
}

void GroupInstanceReferenceImpl::emitJoinPayload(std::function<void(GroupJoinPayload const &)> completion) {
    _internal->perform([completion = std::move(completion)](GroupInstanceReferenceInternal *unwrapped) mutable {
        unwrapped->emitJoinPayload(std::move(completion));
    });
}

void GroupInstanceReferenceImpl::setJoinResponsePayload(std::string const &payload) {
    auto payloadCopy = payload;
    _internal->perform([payloadCopy = std::move(payloadCopy)](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->setJoinResponsePayload(payloadCopy);
    });
}

void GroupInstanceReferenceImpl::removeSsrcs(std::vector<uint32_t> ssrcs) {
    _internal->perform([ssrcs = std::move(ssrcs)](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->removeSsrcs(ssrcs);
    });
}

void GroupInstanceReferenceImpl::removeIncomingVideoSource(uint32_t ssrc) {}

void GroupInstanceReferenceImpl::setIsMuted(bool isMuted) {
    _internal->perform([isMuted](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->setIsMuted(isMuted);
    });
}

void GroupInstanceReferenceImpl::setIsNoiseSuppressionEnabled(bool isNoiseSuppressionEnabled) {
    _internal->perform([isNoiseSuppressionEnabled](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->setIsNoiseSuppressionEnabled(isNoiseSuppressionEnabled);
    });
}
void GroupInstanceReferenceImpl::setVideoCapture(std::shared_ptr<VideoCaptureInterface>) {

}
void GroupInstanceReferenceImpl::setVideoSource(std::function<webrtc::scoped_refptr<webrtc::VideoTrackSourceInterface>()> getVideoSource) {
    _internal->perform([getVideoSource = std::move(getVideoSource)](GroupInstanceReferenceInternal *unwrapped) mutable {
        unwrapped->setVideoSource(std::move(getVideoSource));
    });
}
void GroupInstanceReferenceImpl::setAudioOutputDevice(std::string) {}
void GroupInstanceReferenceImpl::setAudioInputDevice(std::string) {}
void GroupInstanceReferenceImpl::addExternalAudioSamples(std::vector<uint8_t>&&) {}
void GroupInstanceReferenceImpl::addOutgoingVideoOutput(std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>>) {

}
void GroupInstanceReferenceImpl::addIncomingVideoOutput(std::string const &endpointId, std::weak_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> sink) {
    _internal->perform([endpointId, sink](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->addIncomingVideoOutput(endpointId, sink);
    });
}

void GroupInstanceReferenceImpl::setVolume(uint32_t ssrc, double volume) {
    _internal->perform([ssrc, volume](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->setVolume(ssrc, volume);
    });
}

void GroupInstanceReferenceImpl::setRequestedVideoChannels(std::vector<VideoChannelDescription>&& channels) {
    _internal->perform([channels = std::move(channels)](GroupInstanceReferenceInternal *unwrapped) mutable {
        unwrapped->setRequestedVideoChannels(std::move(channels));
    });
}

void GroupInstanceReferenceImpl::getStats(std::function<void(GroupInstanceStats)> completion) {
    _internal->perform([completion = std::move(completion)](GroupInstanceReferenceInternal *unwrapped) {
        unwrapped->getStats(std::move(completion));
    });
}

void GroupInstanceReferenceImpl::internal_addCustomNetworkEvent(bool) {}

}
