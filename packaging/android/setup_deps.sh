#!/bin/bash
# Setup script for SoulFu Android build dependencies
# Called automatically by build_android.sh, or run manually from packaging/android/.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== SoulFu Android dependency setup ==="

# SDL2
if [ ! -d "SDL2" ]; then
    echo "Cloning SDL2..."
    git clone --depth 1 --branch release-2.30.x https://github.com/libsdl-org/SDL.git SDL2
else
    echo "SDL2 already present"
fi

# gl4es
if [ ! -d "gl4es" ]; then
    echo "Cloning gl4es..."
    git clone --depth 1 https://github.com/ptitSeb/gl4es.git gl4es
else
    echo "gl4es already present"
fi

# libjpeg-turbo
if [ ! -d "libjpeg-turbo" ]; then
    echo "Cloning libjpeg-turbo..."
    git clone --depth 1 https://github.com/libjpeg-turbo/libjpeg-turbo.git libjpeg-turbo
else
    echo "libjpeg-turbo already present"
fi

# libogg
if [ ! -d "libogg" ]; then
    echo "Cloning libogg..."
    git clone --depth 1 https://github.com/xiph/ogg.git libogg
else
    echo "libogg already present"
fi

# libvorbis
if [ ! -d "libvorbis" ]; then
    echo "Cloning libvorbis..."
    git clone --depth 1 https://github.com/xiph/vorbis.git libvorbis
else
    echo "libvorbis already present"
fi

# Copy SDL2 Java sources into the Android project
# SDLActivity.java and friends are needed for the JNI bridge
SDL_JAVA_SRC="SDL2/android-project/app/src/main/java/org/libsdl/app"
DEST_JAVA="app/src/main/java/org/libsdl/app"
if [ -d "$SDL_JAVA_SRC" ]; then
    echo "Copying SDL2 Java sources..."
    mkdir -p "$DEST_JAVA"
    cp -r "$SDL_JAVA_SRC"/*.java "$DEST_JAVA/"
else
    echo "WARNING: SDL2 Java sources not found at $SDL_JAVA_SRC"
    echo "         You may need to copy them manually."
fi

# Symlink datafile.sdf into assets
ASSETS_DIR="app/src/main/assets"
mkdir -p "$ASSETS_DIR"
if [ ! -e "$ASSETS_DIR/datafile.sdf" ]; then
    echo "Linking datafile.sdf into assets..."
    ln -sf "../../../../../../datafile.sdf" "$ASSETS_DIR/datafile.sdf"
else
    echo "datafile.sdf asset already present"
fi

echo ""
echo "=== Done! ==="
echo "To build:"
echo "  cd packaging/android"
echo "  ./gradlew assembleDebug"
echo ""
echo "The APK will be at:"
echo "  packaging/android/app/build/outputs/apk/debug/app-debug.apk"
