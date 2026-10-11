#include "v2wasm/CoreFactory.h"

namespace tgcalls {
namespace v2wasm {

std::unique_ptr<ReferenceCallCore> createModuleCore(json11::Json const &config, std::function<void(json11::Json::object &&)> emit) {
    return std::make_unique<ReferenceCallCore>(config, std::move(emit));
}

}
}
