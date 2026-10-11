#ifndef TGCALLS_V2WASM_CALL_CORE_BACKEND_H
#define TGCALLS_V2WASM_CALL_CORE_BACKEND_H

#include <cstdint>
#include <functional>
#include <string>

namespace tgcalls {

class CallCoreBackend {
public:
    using EmitFn = std::function<void(const uint8_t *, size_t)>;

    virtual ~CallCoreBackend() = default;

    virtual bool create(std::string const &configJson, EmitFn emit) = 0;
    virtual bool onEvent(const uint8_t *data, size_t len) = 0;
};

}

#endif
