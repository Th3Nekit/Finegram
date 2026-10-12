#ifndef TGCALLS_V2WASM_WAMR_CORE_BACKEND_H
#define TGCALLS_V2WASM_WAMR_CORE_BACKEND_H

#include "v2wasm/CallCoreBackend.h"

#include <vector>

#include "wasm_export.h"

namespace tgcalls {

class WamrCoreBackend final : public CallCoreBackend {
public:
#if TGCALLS_ALLOW_EXTERNAL_WASM_CORE

    explicit WamrCoreBackend(std::string modulePath);
#endif

    WamrCoreBackend(const uint8_t *moduleBytes, size_t moduleSize);
    ~WamrCoreBackend() override;

    bool create(std::string const &configJson, EmitFn emit) override;
    bool onEvent(const uint8_t *data, size_t len) override;

    static void hostEmitNative(wasm_exec_env_t execEnv, uint8_t *data, uint32_t len);
    static int32_t hostDeflateNative(wasm_exec_env_t execEnv, uint8_t *in, uint32_t inLen, uint8_t *out, uint32_t outCap);
    static int32_t hostInflateNative(wasm_exec_env_t execEnv, uint8_t *in, uint32_t inLen, uint8_t *out, uint32_t outCap);

private:
    bool callCoreFunction(wasm_function_inst_t function, const uint8_t *data, size_t len);
#if TGCALLS_ALLOW_EXTERNAL_WASM_CORE
    bool readModuleFromFile();

    std::string _modulePath;
#endif
    EmitFn _emit;

    std::vector<uint8_t> _moduleBytes;
    int _devNullFd = -1;
    wasm_module_t _module = nullptr;
    wasm_module_inst_t _instance = nullptr;
    wasm_exec_env_t _execEnv = nullptr;
    wasm_function_inst_t _coreInit = nullptr;
    wasm_function_inst_t _coreOnEvent = nullptr;
    wasm_function_inst_t _rtAlloc = nullptr;
    wasm_function_inst_t _rtFree = nullptr;
};

}

#endif
