#ifndef TGCALLS_V2WASM_CALL_CORE_ABI_H
#define TGCALLS_V2WASM_CALL_CORE_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TgcallsCallCore TgcallsCallCore;

#if defined(__wasm__)
__attribute__((import_module("env"), import_name("host_deflate")))
#endif
int32_t tgcalls_host_deflate(const uint8_t *in, size_t inLen, uint8_t *out, size_t outCap);
#if defined(__wasm__)
__attribute__((import_module("env"), import_name("host_inflate")))
#endif
int32_t tgcalls_host_inflate(const uint8_t *in, size_t inLen, uint8_t *out, size_t outCap);

typedef void (*TgcallsCoreEmitFn)(void *userData, const uint8_t *data, size_t len);

TgcallsCallCore *tgcalls_core_create(const char *configJson, TgcallsCoreEmitFn emit, void *userData);
void tgcalls_core_on_event(TgcallsCallCore *core, const uint8_t *data, size_t len);
void tgcalls_core_destroy(TgcallsCallCore *core);

#ifdef __cplusplus
}
#endif

#endif
