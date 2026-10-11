#ifndef TGCALLS_V2WASM_CORE_FACTORY_H
#define TGCALLS_V2WASM_CORE_FACTORY_H

#include <functional>
#include <memory>

#include "third-party/json11.hpp"

#include "v2wasm/ReferenceCallCore.h"

namespace tgcalls {
namespace v2wasm {

std::unique_ptr<ReferenceCallCore> createModuleCore(json11::Json const &config, std::function<void(json11::Json::object &&)> emit);

}
}

#endif
