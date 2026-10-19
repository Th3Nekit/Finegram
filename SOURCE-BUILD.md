# Finegram 1.5.2 corresponding source

This archive contains the build snapshot without changing source bytes or removing
comments. No Git history, release signing keys or private configuration is included.
SOURCE-MANIFEST.json records SHA-256, Unix modes, the build inventory digest, native
origins and exact dependency commits. SOURCE-LICENSES retains licenses for pinned
gitlinks; their complete upstream source and license trees are fetched as follows:

    python3 fetch-source-dependencies.py

Run that command before Gradle. It fetches exact commits into temporary repositories,
copies source files without .git history, and preserves bundled files and overlays.
The four expanded Telegram native trees are already bundled. Existing native static
archives and local AARs are retained byte for byte as build inputs; see native
provenance and the upstream source/build scripts included in the tree.

Build on Linux (or Git Bash/WSL with the matching Android toolchain). Install JDK 17,
Android SDK/platform/build-tools and NDK/CMake versions declared in README.md,
TMessagesProj/build.gradle and native/fghook/build.sh. The Gradle wrapper and its JAR
are included. Native helper scripts and Dockerfile describe rebuilding FFmpeg,
BoringSSL and other native components. The fghook native wrapper has its source,
headers, patch and build script under native/fghook. Consult SOURCE-NATIVE.json
for additional native origins. For the ordinary application rebuild the bundled
native libraries are already present and do not require rebuilding first.

SOURCE-NATIVE-SOURCES includes four complete history-free source bundles for
libbox/sing-box, LSPlant (with native submodules), patched shadowhook and XZ.
Their manifests, license texts, local patches and rebuild instructions are retained.
See SOURCE-NATIVE-SOURCES/hook/SOURCE-BUILD.md and the libbox source bundle's
SOURCE-BUILD.md for replacing/rebuilding the native libraries.

Follow README.md to create your own private.properties (Telegram API ID/hash), own
google-services.json for the modules requiring it, and local.properties containing
your SDK path. Use your own API/service keys. Release signing uses your own keystore;
the original release key is deliberately absent. The result cannot have the original
release signature. Do not reuse developer credentials.

    chmod +x gradlew
    ./gradlew --no-daemon assembleAfatStandalone

The release build also used an init script disabling incremental Java compilation:

    ./gradlew assembleAfatStandalone --console=plain --no-daemon --max-workers=4 \
      --init-script source-build.init.gradle -Dorg.gradle.parallel=true \
      '-Dorg.gradle.jvmargs=-Xmx8g -XX:MaxMetaspaceSize=1g -XX:ActiveProcessorCount=8 -Dfile.encoding=UTF-8' \
      -Pkotlin.compiler.execution.strategy=in-process

The included init script is the exact source-only script used for this build.
Machine-specific SDK, JDK, Gradle-cache paths and signing credentials are omitted.
The source inventory and Gradle files describe this release
(Telegram 13.0.2, Finegram 1.5.2).

The signing and optional local wheel settings are documented in README.md and Gradle
files. No APK build was performed by the source exporter. Archive verification checks
all included source bytes against the supplied build inventory; it does not assert
bit-identical APK output on a different machine or with different credentials.
