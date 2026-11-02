# Исходники нативных библиотек

В `TMessagesProj/libs` лежат две собранные библиотеки. Здесь их исходники в том виде, из которого они собирались.

## libbox.aar

Ядро sing-box для прокси VLESS и Hysteria2.

- `sing-box/sing-box-finegram-source.zip` — sing-box на коммите `710d7c715b10cf9fc158b1ab7fd7198754962c59` с уже наложенным `finegram-libbox.patch`.
- Сборка: Go 1.27, gomobile из `github.com/sagernet/gomobile` v0.1.13.

```bash
go run ./cmd/internal/build_libbox -target android -platform android/arm64
```

Теги сборки: `with_quic`, `with_utls`, `badlinkname`, `tfogo_checklinkname0`. Зависимости закреплены в `go.mod` и `go.sum`, их подтягивает `go mod download`.

## fghook-core.aar

Перехваты методов для плагинов. Внутри три библиотеки: `libfghook.so` (код в `native/fghook`), `liblsplant.so` (LSPlant, LGPL-3.0) и `libshadowhook.so` (ShadowHook, MIT).

- `lsplant/` — LSPlant на коммите `1dc6ef5c6c5c` вместе с его нативными сабмодулями.
- `shadowhook/` — ShadowHook на коммите `593f491be687` с нашим патчем, патч уже наложен.
- `xz/` — xz-embedded на коммите `ae63ae3a36ed`.

Нужны JDK 17, Android SDK 35, NDK 30.0.16248370, CMake 3.31.6 и Ninja.

LSPlant:

```bash
cd lsplant
./gradlew :lsplant:assembleStandalone
```

ShadowHook, для `arm64-v8a` и `armeabi-v7a`:

```bash
cmake -S shadowhook-build -B build/$ABI -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
cmake --build build/$ABI
```

Готовые `liblsplant.so` и `libshadowhook.so` кладутся в `native/fghook/prebuilt/<abi>/`, после этого `native/fghook/build.sh` собирает `fghook-core.aar`.

Чтобы заменить LSPlant своей сборкой, достаточно подменить `liblsplant.so` внутри AAR, пересобрать приложение и подписать своим ключом.

## tde2e

`tde2e/LICENSE_1_0.txt` — лицензия TDLib для кода tde2e, который лежит в `TMessagesProj/jni/tde2e`.
