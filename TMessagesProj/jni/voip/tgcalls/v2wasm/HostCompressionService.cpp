#include <cstring>
#include <vector>

#include "v2wasm/CallCoreABI.h"

#include "utils/gzip.h"

extern "C" int32_t tgcalls_host_deflate(const uint8_t *in, size_t inLen, uint8_t *out, size_t outCap) {
    if (!in || !out || inLen == 0) {
        return -1;
    }
    const auto compressed = tgcalls::gzipData(std::vector<uint8_t>(in, in + inLen));
    if (!compressed || compressed->size() > outCap) {
        return -1;
    }
    memcpy(out, compressed->data(), compressed->size());
    return (int32_t)compressed->size();
}

extern "C" int32_t tgcalls_host_inflate(const uint8_t *in, size_t inLen, uint8_t *out, size_t outCap) {
    if (!in || !out || inLen == 0) {
        return -1;
    }

    const auto decompressed = tgcalls::gunzipData(std::vector<uint8_t>(in, in + inLen), outCap);
    if (!decompressed || decompressed->size() > outCap) {
        return -1;
    }
    memcpy(out, decompressed->data(), decompressed->size());
    return (int32_t)decompressed->size();
}
