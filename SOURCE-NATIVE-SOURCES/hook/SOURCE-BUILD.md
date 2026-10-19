# Rebuilding Finegram hook native libraries

The three source ZIPs in this directory are history-free working-tree exports.
Extract all three here: lsplant/, shadowhook-src/, shadowhook-build/, xz-embedded/.
Source bytes and license comments are preserved. Shadowhook is already patched;
do not apply patches/shadowhook-optional-linker.patch again. provenance.json pins
all five repositories, including LSPlant's populated DexBuilder/parallel_hashmap.
Documentation/test-only gitlinks listed there are not needed by the native build.

Install JDK 17, Android SDK 35/build-tools 35.0.0, NDK 30.0.16248370, CMake 3.31.6
and Ninja. Set ANDROID_HOME and ANDROID_NDK_HOME to your own toolchain paths.
These recipes reflect preserved source/build configuration; no fresh native
rebuild or byte-identical rebuild is claimed by this distribution.

LSPlant's version catalog pins org.lsposed.libcxx:libcxx:30.0.16248370 (standalone)
and org.lsposed.libcxx:libcxx-full:30.0.16248370 (non-standalone) from Maven.
Its standalone variant uses Android 21, ANDROID_STL=none, LSPLANT_STANDALONE=ON,
ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON. Its original Gradle recipe is included:

    cd lsplant
    chmod +x gradlew
    ./gradlew :lsplant:assembleStandalone --no-daemon
    cd ..

The output shared libraries are under
lsplant/lsplant/build/intermediates/cmake/standalone/obj/<abi>/liblsplant.so
(AGP also stages the same files in intermediates/cxx and prefab_package).
The original build scripts derive publication version from Git tags; a history-free
checkout uses their documented fallback label. Do not run publication tasks.

Build patched shadowhook from the included sibling CMake wrapper, for each of
arm64-v8a and armeabi-v7a. Use CMake/Ninja from the installed SDK, for example:

    for abi in arm64-v8a armeabi-v7a; do
      cmake -S shadowhook-build -B shadowhook-build/build/$abi -G Ninja         -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"         -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-24         -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
      cmake --build shadowhook-build/build/$abi
    done

The wrapper supplies arm64 max-page-size=16384 and the original linker options.

Copy each rebuilt liblsplant.so and libshadowhook.so to
<application-source>/native/fghook/prebuilt/<abi>/. Wrapper CMakeLists.txt,
build.sh, headers and embedded XZ sources are in <application-source>/native/fghook.
The retained source comment mentioning Maven shadowhook is historical: this
release's library matches the patched source build documented here.
On Windows/Git Bash set ANDROID_SDK and JAVA_HOME, then run its build.sh.
For Linux invoke CMake directly using the same Android-24, c++_static, Release,
flexible-page-size options from build.sh, then package the three shared libraries
per ABI in jni/<abi>/ of an AAR with the AndroidManifest.xml/classes.jar/R.txt
layout shown by build.sh. Replace TMessagesProj/libs/fghook-core.aar with that AAR.
For LGPL library replacement/relinking, replace the respective .so files inside
the AAR, rebuild the application using SOURCE-BUILD.md, and sign with your key.
The original signing key and developer service credentials are not distributed.

All six packaged libraries matched existing native build outputs; binary-matches.json
records their hashes and ELF alignment. Private machine paths, CMake caches and
historical command logs are not required for these recipes and are not published.
