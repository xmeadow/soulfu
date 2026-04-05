#!/bin/bash

# Android APK build script for SoulFu
# Usage: VERSION=1.8 ./packaging/android/build_android.sh
# Run from the project root directory.

# Check if version is set via environment variable
if [ -z "$VERSION" ]; then
    echo "Error: VERSION environment variable not set"
    echo "Usage: VERSION=<version> $0"
    exit 1
fi

PACKAGE_NAME="soulfu"
ANDROID_DIR="packaging/android"

# Rebuild datafile.sdf (compiles scripts, language files, and packs datadir/)
make data
if [ ! -f "datafile.sdf" ]; then
    echo "Error: datafile.sdf not found. 'make data' failed."
    exit 1
fi

# Clone dependencies if not yet present
if [ ! -d "${ANDROID_DIR}/SDL2" ]; then
    echo "Dependencies not found, running setup_deps.sh..."
    (cd "${ANDROID_DIR}" && ./setup_deps.sh)
fi

# Make sure datafile.sdf is linked into assets
ASSETS_DIR="${ANDROID_DIR}/app/src/main/assets"
mkdir -p "${ASSETS_DIR}"
if [ ! -e "${ASSETS_DIR}/datafile.sdf" ]; then
    ln -sf "../../../../../../datafile.sdf" "${ASSETS_DIR}/datafile.sdf"
fi

# Find Android SDK
if [ -z "$ANDROID_HOME" ]; then
    if [ -d "/usr/lib/android-sdk" ]; then
        export ANDROID_HOME="/usr/lib/android-sdk"
    elif [ -d "$HOME/Android/Sdk" ]; then
        export ANDROID_HOME="$HOME/Android/Sdk"
    else
        echo "Error: ANDROID_HOME not set and Android SDK not found."
        echo "Install with: sudo apt install android-sdk google-android-ndk-r26b-installer \\"
        echo "    google-android-platform-34-installer google-android-build-tools-34.0.0-installer"
        exit 1
    fi
fi

echo "sdk.dir=${ANDROID_HOME}" > "${ANDROID_DIR}/local.properties"

# Use system cmake if available (avoids needing to download cmake via sdkmanager)
CMAKE_BIN="$(which cmake 2>/dev/null)"
if [ -n "$CMAKE_BIN" ]; then
    CMAKE_DIR="$(dirname "$(dirname "$CMAKE_BIN")")"
    echo "cmake.dir=${CMAKE_DIR}" >> "${ANDROID_DIR}/local.properties"
fi

# Inject version into build.gradle
sed -i "s/versionName \".*\"/versionName \"${VERSION}\"/" "${ANDROID_DIR}/app/build.gradle"

# Build APK — debug is auto-signed (installable), release requires separate signing
BUILD_TYPE="${1:-debug}"
echo "Building Android APK ${VERSION} (${BUILD_TYPE})..."
if [ "${BUILD_TYPE}" = "release" ]; then
    (cd "${ANDROID_DIR}" && ./gradlew assembleRelease)
else
    (cd "${ANDROID_DIR}" && ./gradlew assembleDebug)
fi

# Check build succeeded
if [ "${BUILD_TYPE}" = "release" ]; then
    APK_SRC="${ANDROID_DIR}/app/build/outputs/apk/release/app-release-unsigned.apk"
else
    APK_SRC="${ANDROID_DIR}/app/build/outputs/apk/debug/app-debug.apk"
fi
if [ ! -f "${APK_SRC}" ]; then
    echo "Build failed: APK not found at ${APK_SRC}"
    exit 1
fi

# Copy to packaging/bin
mkdir -p packaging/bin
APK_OUT="packaging/bin/${PACKAGE_NAME}_${VERSION}_android.apk"
cp "${APK_SRC}" "${APK_OUT}"

echo "Package ${APK_OUT} created successfully"
