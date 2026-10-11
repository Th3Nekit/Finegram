#!/usr/bin/env bash

set -Eeuo pipefail

NDK_VERSION="27.2.12479018"
API=21
ABIS="arm64-v8a armeabi-v7a x86_64 x86"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPUS_SRC="${OPUS_SOURCE_DIR:-$SCRIPT_DIR/../third_party/xiph/opus}"
OUT_DIR="${OUT_DIR:-$SCRIPT_DIR/build/opus}"
GEN_DIR="$OUT_DIR/cmake"

: "${ANDROID_NDK_HOME:=${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}/ndk/$NDK_VERSION}"
TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"

mkvar() {
    awk -v v="$2" '
        $0 ~ "^"v"[ \t]*=" { f=1; sub("^"v"[ \t]*=[ \t]*","") }
        f {
            cont = sub(/\\[ \t]*$/,"")
            gsub(/^[ \t]+|[ \t]+$/,"")
            if ($0 != "") print $0
            if (!cont) exit
        }
    ' "$1"
}

to_cmake_list() {
    local out="" f
    while IFS= read -r f; do
        [ -n "$f" ] && out="$out;$OPUS_SRC/$f"
    done
    echo "${out#;}"
}

OPUS_MK="$OPUS_SRC/opus_sources.mk"
CELT_MK="$OPUS_SRC/celt_sources.mk"
SILK_MK="$OPUS_SRC/silk_sources.mk"
for m in "$OPUS_MK" "$CELT_MK" "$SILK_MK"; do
    [ -f "$m" ] || { echo "ERROR: missing $m (is OPUS_SOURCE_DIR correct?)" >&2; exit 1; }
done

BASE_LIST="$( { mkvar "$OPUS_MK" OPUS_SOURCES
                mkvar "$OPUS_MK" OPUS_SOURCES_FLOAT
                mkvar "$CELT_MK" CELT_SOURCES
                mkvar "$SILK_MK" SILK_SOURCES
                mkvar "$SILK_MK" SILK_SOURCES_FIXED; } | to_cmake_list )"
NEON_LIST="$( { mkvar "$CELT_MK" CELT_SOURCES_ARM_NEON_INTR
                mkvar "$SILK_MK" SILK_SOURCES_ARM_NEON_INTR
                mkvar "$SILK_MK" SILK_SOURCES_FIXED_ARM_NEON_INTR; } | to_cmake_list )"

INCLUDES="$OPUS_SRC;$OPUS_SRC/include;$OPUS_SRC/celt;$OPUS_SRC/silk;$OPUS_SRC/silk/fixed"
BASE_DEFS="OPUS_BUILD;FIXED_POINT;USE_ALLOCA;restrict=;LOCALE_NOT_USED;HAVE_LRINT;HAVE_LRINTF"

NEON_DEFS="OPUS_ARM_MAY_HAVE_NEON_INTR;OPUS_ARM_PRESUME_NEON_INTR"

mkdir -p "$GEN_DIR"
cat > "$GEN_DIR/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.18)
project(opus_static C)
add_library(opus STATIC ${OPUS_SOURCES})
set_target_properties(opus PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(opus PRIVATE ${OPUS_DEFS})
target_include_directories(opus PRIVATE ${OPUS_INCLUDES})
# -Oz + fast-math set mirror the in-tree opus target; -g0 drops debug info.
target_compile_options(opus PRIVATE
    -std=c11 -Oz -g0 -ffast-math -funroll-loops -fno-strict-aliasing -fno-math-errno
    -ffunction-sections -fdata-sections -fvisibility=hidden -w)
CMAKE

for abi in $ABIS; do
    b="$OUT_DIR/$abi/build"
    rm -rf "$b"

    srcs="$BASE_LIST"
    defs="$BASE_DEFS"
    extra=()
    case "$abi" in
        arm64-v8a)
            srcs="$BASE_LIST;$NEON_LIST"; defs="$BASE_DEFS;$NEON_DEFS" ;;
        armeabi-v7a)
            srcs="$BASE_LIST;$NEON_LIST"; defs="$BASE_DEFS;$NEON_DEFS"
            extra+=(-DANDROID_ARM_NEON=ON) ;;
    esac

    cmake -S "$GEN_DIR" -B "$b" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="android-$API" \
        "${extra[@]}" \
        -DOPUS_SOURCES="$srcs" \
        -DOPUS_DEFS="$defs" \
        -DOPUS_INCLUDES="$INCLUDES"
    cmake --build "$b"
    install -D "$b/libopus.a" "$OUT_DIR/$abi/libopus.a"
    echo "==> $OUT_DIR/$abi/libopus.a"
done