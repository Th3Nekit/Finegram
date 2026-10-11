#ifndef TGCALLS_V2WASM_CORE_GZIP_H
#define TGCALLS_V2WASM_CORE_GZIP_H

#include <cstdint>
#include <optional>
#include <vector>

namespace tgcalls {
namespace v2wasm {

bool coreIsGzip(std::vector<uint8_t> const &data);
std::optional<std::vector<uint8_t>> coreGzipData(std::vector<uint8_t> const &data);
std::optional<std::vector<uint8_t>> coreGunzipData(std::vector<uint8_t> const &data, size_t sizeLimit);

}
}

#endif
