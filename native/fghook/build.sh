#!/bin/sh

set -e
cd "$(dirname "$0")"
SDK=${ANDROID_SDK:-C:/Android/Sdk}
NDK=$SDK/ndk/30.0.16248370
CMAKE=$SDK/cmake/3.31.6/bin/cmake.exe
NINJA=$SDK/cmake/3.31.6/bin/ninja.exe

rm -rf out
for ABI in arm64-v8a armeabi-v7a; do
    "$CMAKE" -S . -B "build/$ABI" -G Ninja \
        -DCMAKE_MAKE_PROGRAM="$NINJA" \
        -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-24 \
        -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
        -DCMAKE_BUILD_TYPE=Release
    "$CMAKE" --build "build/$ABI"
    mkdir -p "out/aar/jni/$ABI"
    cp "build/$ABI/libfghook.so" "prebuilt/$ABI/liblsplant.so" "prebuilt/$ABI/libshadowhook.so" "out/aar/jni/$ABI/"
done

cat > out/aar/AndroidManifest.xml <<'EOF'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.th3nekit.finegram.fghook">
    <uses-sdk android:minSdkVersion="24" />
</manifest>
EOF
touch out/aar/R.txt
mkdir -p out/empty
(cd out/empty && "$JAVA_HOME/bin/jar" cf ../aar/classes.jar . 2>/dev/null || jar cf ../aar/classes.jar .)
(cd out/aar && "$JAVA_HOME/bin/jar" cfM ../fghook-core.aar . 2>/dev/null || jar cfM ../fghook-core.aar .)
echo "готово: out/fghook-core.aar"
