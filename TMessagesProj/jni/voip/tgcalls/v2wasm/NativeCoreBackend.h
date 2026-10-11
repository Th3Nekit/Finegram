#ifndef TGCALLS_V2WASM_NATIVE_CORE_BACKEND_H
#define TGCALLS_V2WASM_NATIVE_CORE_BACKEND_H

#include "v2wasm/CallCoreBackend.h"
#include "v2wasm/CallCoreABI.h"

namespace tgcalls {

class NativeCoreBackend final : public CallCoreBackend {
public:
    ~NativeCoreBackend() override;

    bool create(std::string const &configJson, EmitFn emit) override;
    bool onEvent(const uint8_t *data, size_t len) override;

private:
    static void emitTrampoline(void *userData, const uint8_t *data, size_t len);

    TgcallsCallCore *_core = nullptr;
    EmitFn _emit;
};

}

#endif
