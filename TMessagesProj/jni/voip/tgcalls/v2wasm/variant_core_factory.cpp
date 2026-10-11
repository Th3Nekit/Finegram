#include "v2wasm/CoreFactory.h"
#include "v2wasm/VariantCallCore.h"

namespace tgcalls {
namespace v2wasm {

std::unique_ptr<ReferenceCallCore> createModuleCore(json11::Json const &config, std::function<void(json11::Json::object &&)> emit) {
    return std::make_unique<VariantCallCore>(config, std::move(emit));
}

}
}
