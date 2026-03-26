#!/bin/bash

# macOS AMD64 DMG build script for SoulFu
# Usage: VERSION=1.8 ./packaging/build_macos-amd64.sh
#
# Two modes:
#   - Native (on macOS x86_64 or CI runner): uses Makefile.macos with brew/pkg-config
#   - Cross-compile (on Linux with osxcross): uses Makefile.macos-amd64 with sysroot

set -e

if [ -z "$VERSION" ]; then
    echo "Error: VERSION environment variable not set"
    echo "Usage: VERSION=<version> $0"
    exit 1
fi

PACKAGE_NAME="soulfu"
ARCHITECTURE="amd64"
APP="SoulFu.app"
DMG_NAME="packaging/bin/${PACKAGE_NAME}_${VERSION}_macos-${ARCHITECTURE}.dmg"
CROSS_SYSROOT="/tmp/macos-amd64-sysroot"

# --- Detect build mode ---
if [ "$(uname -s)" = "Darwin" ]; then
    NATIVE=1
    echo "Native macOS build"
else
    NATIVE=0
    echo "Cross-compile build (osxcross)"
    if [ -z "$OSXCROSS" ]; then
        for p in /opt/osxcross /usr/local/osxcross "$HOME/osxcross"; do
            [ -d "$p/bin" ] && OSXCROSS="$p" && break
        done
    fi
    if [ -z "$OSXCROSS" ] || [ ! -d "$OSXCROSS/bin" ]; then
        echo "Error: osxcross not found. Set OSXCROSS env var."
        exit 1
    fi
    export PATH="$OSXCROSS/bin:$PATH"
    CROSS_HOST="$(basename "$(ls "$OSXCROSS/bin"/x86_64-apple-darwin*-clang | head -1)" | sed 's/-clang$//')"
    INSTALL_NAME_TOOL="$(ls "$OSXCROSS/bin"/${CROSS_HOST}-install_name_tool 2>/dev/null | head -1)"
    OTOOL="$(ls "$OSXCROSS/bin"/${CROSS_HOST}-otool 2>/dev/null | head -1)"
fi

# --- Compile ---
rm -f soulfu
if [ "$NATIVE" -eq 1 ]; then
    make -f Makefile.macos release
else
    OSXCROSS="$OSXCROSS" make -f Makefile.macos-amd64 release
fi

if [ ! -f "soulfu" ] || [ ! -f "datafile.sdf" ]; then
    echo "Error: Compilation failed. Required files missing."
    exit 1
fi

# --- Helper: install_name_tool wrapper ---
_install_name_tool() {
    if [ "$NATIVE" -eq 1 ]; then
        install_name_tool "$@"
    else
        "$INSTALL_NAME_TOOL" "$@"
    fi
}

_otool() {
    if [ "$NATIVE" -eq 1 ]; then
        otool "$@"
    else
        "$OTOOL" "$@"
    fi
}

# --- Create app bundle ---
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"

cp soulfu "$APP/Contents/MacOS/soulfu-bin"
_install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/soulfu-bin"

fix_dylib() {
    local old_path="$1"
    local lib_name="$2"
    _install_name_tool -change "$old_path" "@executable_path/../Frameworks/$lib_name" "$APP/Contents/MacOS/soulfu-bin" 2>/dev/null || true
}

# --- Bundle libraries ---
if [ "$NATIVE" -eq 1 ]; then
    # Native: use brew or sysroot frameworks
    MACOS_SYSROOT="/tmp/macos-sysroot"
    if [ -d "$MACOS_SYSROOT/Frameworks/SDL2.framework" ]; then
        USE_SYSROOT=1
        LIB_DIR="$MACOS_SYSROOT/lib"
    else
        USE_SYSROOT=0
        LIB_DIR="$(brew --prefix)/lib"
    fi

    if [ "$USE_SYSROOT" -eq 1 ] && [ -d "$MACOS_SYSROOT/Frameworks/SDL2.framework" ]; then
        cp -R "$MACOS_SYSROOT/Frameworks/SDL2.framework" "$APP/Contents/Frameworks/"
        cp -R "$MACOS_SYSROOT/Frameworks/SDL2_net.framework" "$APP/Contents/Frameworks/"
    else
        SDL2_FW="$(brew --prefix sdl2)/lib/SDL2.framework"
        SDL2NET_FW="$(brew --prefix sdl2_net)/lib/SDL2_net.framework"
        if [ -d "$SDL2_FW" ]; then
            cp -R "$SDL2_FW" "$APP/Contents/Frameworks/"
            cp -R "$SDL2NET_FW" "$APP/Contents/Frameworks/"
        else
            cp "$(brew --prefix sdl2)/lib/libSDL2.dylib" "$APP/Contents/Frameworks/"
            cp "$(brew --prefix sdl2_net)/lib/libSDL2_net.dylib" "$APP/Contents/Frameworks/"
        fi
    fi

    for lib in libogg.0.dylib libvorbis.0.dylib libjpeg.62.dylib; do
        if [ "$USE_SYSROOT" -eq 1 ]; then
            src="$MACOS_SYSROOT/lib/$lib"
        else
            src="$(find "$(brew --prefix)/lib" "$(brew --prefix)/opt" -name "$lib" 2>/dev/null | head -1)"
        fi
        if [ -f "$src" ]; then
            cp "$src" "$APP/Contents/Frameworks/"
            old_id="$(_otool -D "$src" | tail -1)"
            fix_dylib "$old_id" "$lib"
            fix_dylib "$MACOS_SYSROOT/lib/$lib" "$lib"
            fix_dylib "@rpath/$lib" "$lib"
            _install_name_tool -id "@executable_path/../Frameworks/$lib" "$APP/Contents/Frameworks/$lib" 2>/dev/null || true
        fi
    done
else
    # Cross-compile: everything is statically linked, no dylibs to bundle
    echo "All libraries statically linked — no Frameworks to bundle"
fi

# Fix libvorbis -> libogg internal dependency
VORBIS_LIB="$APP/Contents/Frameworks/libvorbis.0.dylib"
if [ -f "$VORBIS_LIB" ]; then
    ogg_ref="$(_otool -L "$VORBIS_LIB" | grep libogg | awk '{print $1}')"
    if [ -n "$ogg_ref" ]; then
        _install_name_tool -change "$ogg_ref" "@executable_path/../Frameworks/libogg.0.dylib" "$VORBIS_LIB"
    fi
fi

# --- Launcher script ---
cat > "$APP/Contents/MacOS/soulfu" << 'SCRIPT'
#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR/../Resources"
exec "$DIR/soulfu-bin" "$@"
SCRIPT
chmod +x "$APP/Contents/MacOS/soulfu"

# --- Game data ---
cp datafile.sdf "$APP/Contents/Resources/"
[ -f "Manual.htm" ] && cp Manual.htm "$APP/Contents/Resources/"
[ -f "soulfu.jpg" ] && cp soulfu.jpg "$APP/Contents/Resources/"
[ -f "packaging/license.txt" ] && cp packaging/license.txt "$APP/Contents/Resources/"

# --- Icon ---
ICON_SRC="packaging/icons/hicolor"
ICON_KEY=""
if [ -d "$ICON_SRC" ]; then
    if [ "$NATIVE" -eq 1 ]; then
        # Native macOS: use iconutil + sips
        ICONSET="$(mktemp -d)/soulfu.iconset"
        mkdir -p "$ICONSET"
        cp "$ICON_SRC/16x16/apps/soulfu.png" "$ICONSET/icon_16x16.png"
        cp "$ICON_SRC/32x32/apps/soulfu.png" "$ICONSET/icon_16x16@2x.png"
        cp "$ICON_SRC/32x32/apps/soulfu.png" "$ICONSET/icon_32x32.png"
        cp "$ICON_SRC/64x64/apps/soulfu.png" "$ICONSET/icon_32x32@2x.png"
        cp "$ICON_SRC/128x128/apps/soulfu.png" "$ICONSET/icon_128x128.png"
        sips -z 256 256 "$ICON_SRC/128x128/apps/soulfu.png" --out "$ICONSET/icon_128x128@2x.png" > /dev/null
        sips -z 256 256 "$ICON_SRC/128x128/apps/soulfu.png" --out "$ICONSET/icon_256x256.png" > /dev/null
        sips -z 512 512 "$ICON_SRC/128x128/apps/soulfu.png" --out "$ICONSET/icon_256x256@2x.png" > /dev/null
        sips -z 512 512 "$ICON_SRC/128x128/apps/soulfu.png" --out "$ICONSET/icon_512x512.png" > /dev/null
        sips -z 1024 1024 "$ICON_SRC/128x128/apps/soulfu.png" --out "$ICONSET/icon_512x512@2x.png" > /dev/null
        iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/soulfu.icns"
        ICON_KEY="<key>CFBundleIconFile</key>
    <string>soulfu</string>"
    else
        # Linux cross-compile: use png2icns (from icnsutils package)
        if command -v png2icns > /dev/null 2>&1; then
            png2icns "$APP/Contents/Resources/soulfu.icns" \
                "$ICON_SRC/128x128/apps/soulfu.png" \
                "$ICON_SRC/32x32/apps/soulfu.png" \
                "$ICON_SRC/16x16/apps/soulfu.png" 2>/dev/null || true
            if [ -f "$APP/Contents/Resources/soulfu.icns" ]; then
                ICON_KEY="<key>CFBundleIconFile</key>
    <string>soulfu</string>"
            fi
        else
            echo "Warning: png2icns not found, building without app icon (apt-get install icnsutils)"
        fi
    fi
fi

# --- Info.plist ---
cat > "$APP/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>soulfu</string>
    <key>CFBundleIdentifier</key>
    <string>com.soulfu.game</string>
    <key>CFBundleName</key>
    <string>SoulFu</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    ${ICON_KEY}
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLIST

# --- Create DMG (or zip on Linux) ---
mkdir -p packaging/bin

if [ "$NATIVE" -eq 1 ]; then
    DMG_STAGING="$(mktemp -d)"
    cp -R "$APP" "$DMG_STAGING/"
    ln -s /Applications "$DMG_STAGING/Applications"
    hdiutil create -volname "SoulFu" -srcfolder "$DMG_STAGING" -ov -format UDZO "$DMG_NAME"
    rm -rf "$DMG_STAGING"
else
    # Linux: use genisoimage for a DMG-like image, or fall back to zip
    if command -v genisoimage > /dev/null 2>&1; then
        genisoimage -V "SoulFu" -D -R -apple -no-pad \
            -o "$DMG_NAME" "$APP" 2>/dev/null
    else
        echo "Warning: genisoimage not found, creating zip instead"
        DMG_NAME="packaging/bin/${PACKAGE_NAME}_${VERSION}_macos-${ARCHITECTURE}.zip"
        (cd "$(dirname "$APP")" && zip -r -q "$OLDPWD/$DMG_NAME" "$(basename "$APP")")
    fi
fi

echo ""
echo "Package $DMG_NAME created successfully"

# Clean up
rm -rf "$APP"
rm -f soulfu
