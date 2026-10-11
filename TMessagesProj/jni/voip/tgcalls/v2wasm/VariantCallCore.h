#ifndef TGCALLS_V2WASM_VARIANT_CALL_CORE_H
#define TGCALLS_V2WASM_VARIANT_CALL_CORE_H

#include "v2wasm/ReferenceCallCore.h"

namespace tgcalls {
namespace v2wasm {

class VariantCallCore : public ReferenceCallCore {
public:
    VariantCallCore(json11::Json const &config, std::function<void(json11::Json::object &&)> emit);

protected:
    std::string mungeLocalDescription(std::string const &type, std::string const &sdp) override;
    void onStats(json11::Json const &event) override;
    void mungeOutgoingSignalingMessage(json11::Json::object &message) override;
    void onDataChannelEvent(json11::Json const &event) override;

private:
    int _statsTicks = 0;
    int _iceRestarts = 0;
    int _lastCapKbps = 0;
    int _padCount = 0;
    bool _apmApplied = false;
    bool _configApplied = false;
};

}
}

#endif
