#!/bin/bash

# macOS ARM64 DMG build script for SoulFu
# Usage: VERSION=1.8 ./packaging/build_macos-arm64.sh
#
# Supports two modes:
#   - Sysroot mode: if /tmp/macos-sysroot exists, uses Makefile.macos
#     (for machines without working Homebrew — run setup_macos_sysroot.sh first)
#   - Brew/pkg-config mode: otherwise, uses the default Makefile
#     (for CI runners or machines with Homebrew deps installed)

set -e

# Check if version is set via environment variable
if [ -z "$VERSION" ]; then
    echo "Error: VERSION environment variable not set"
    echo "Usage: VERSION=<version> $0"
    exit 1
fi

PACKAGE_NAME="soulfu"
ARCHITECTURE="arm64"
SYSROOT="/tmp/macos-sysroot"
APP="SoulFu.app"
DMG_NAME="packaging/bin/${PACKAGE_NAME}_${VERSION}_macos-${ARCHITECTURE}.dmg"

# Compile (Makefile.macos auto-detects sysroot vs pkg-config)
rm -f soulfu
if [ -d "$SYSROOT/Frameworks/SDL2.framework" ]; then
    echo "Using sysroot at $SYSROOT"
    USE_SYSROOT=1
else
    echo "Using system libraries (pkg-config)"
    USE_SYSROOT=0
fi
make -f Makefile.macos release

# Check if required files exist
if [ ! -f "soulfu" ] || [ ! -f "datafile.sdf" ]; then
    echo "Error: Compilation failed. One or more required files are missing."
    exit 1
fi

# --- Create app bundle ---
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"

# Copy binary
cp soulfu "$APP/Contents/MacOS/soulfu-bin"

# Add rpath for frameworks
install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/soulfu-bin"

# Resolve dylib paths and bundle them
# For each dylib linked by the binary, copy it into Frameworks and fix the load path
fix_dylib() {
    local old_path="$1"
    local lib_name="$2"
    install_name_tool -change "$old_path" "@executable_path/../Frameworks/$lib_name" "$APP/Contents/MacOS/soulfu-bin" 2>/dev/null || true
}

if [ "$USE_SYSROOT" -eq 1 ]; then
    LIB_DIR="$SYSROOT/lib"
    FW_DIR="$SYSROOT/Frameworks"
else
    LIB_DIR="$(brew --prefix)/lib"
    FW_DIR="$(brew --prefix)/opt/sdl2/lib:$(brew --prefix)/opt/sdl2_net/lib"
fi

# Copy and fix SDL2 framework
if [ "$USE_SYSROOT" -eq 1 ] && [ -d "$SYSROOT/Frameworks/SDL2.framework" ]; then
    cp -R "$SYSROOT/Frameworks/SDL2.framework" "$APP/Contents/Frameworks/"
    cp -R "$SYSROOT/Frameworks/SDL2_net.framework" "$APP/Contents/Frameworks/"
else
    # Brew installs frameworks here
    SDL2_FW="$(brew --prefix sdl2)/lib/SDL2.framework"
    SDL2NET_FW="$(brew --prefix sdl2_net)/lib/SDL2_net.framework"
    if [ -d "$SDL2_FW" ]; then
        cp -R "$SDL2_FW" "$APP/Contents/Frameworks/"
        cp -R "$SDL2NET_FW" "$APP/Contents/Frameworks/"
    else
        # Brew may install as dylibs instead of frameworks
        cp "$(brew --prefix sdl2)/lib/libSDL2.dylib" "$APP/Contents/Frameworks/"
        cp "$(brew --prefix sdl2_net)/lib/libSDL2_net.dylib" "$APP/Contents/Frameworks/"
    fi
fi

# Copy and fix libogg, libvorbis, libjpeg dylibs
for lib in libogg.0.dylib libvorbis.0.dylib libjpeg.62.dylib; do
    # Find the actual dylib
    if [ "$USE_SYSROOT" -eq 1 ]; then
        src="$SYSROOT/lib/$lib"
    else
        src="$(find "$(brew --prefix)/lib" "$(brew --prefix)/opt" -name "$lib" 2>/dev/null | head -1)"
    fi
    if [ -f "$src" ]; then
        cp "$src" "$APP/Contents/Frameworks/"
        # Get the install name recorded in the binary and fix it
        old_id="$(otool -D "$src" | tail -1)"
        fix_dylib "$old_id" "$lib"
        # Also try common path patterns
        fix_dylib "$SYSROOT/lib/$lib" "$lib"
        fix_dylib "@rpath/$lib" "$lib"
        # Set the dylib's own install name
        install_name_tool -id "@executable_path/../Frameworks/$lib" "$APP/Contents/Frameworks/$lib" 2>/dev/null || true
    fi
done

# Fix libvorbis -> libogg internal dependency
VORBIS_LIB="$APP/Contents/Frameworks/libvorbis.0.dylib"
if [ -f "$VORBIS_LIB" ]; then
    # Find whatever path libogg is referenced as and rewrite it
    ogg_ref="$(otool -L "$VORBIS_LIB" | grep libogg | awk '{print $1}')"
    if [ -n "$ogg_ref" ]; then
        install_name_tool -change "$ogg_ref" "@executable_path/../Frameworks/libogg.0.dylib" "$VORBIS_LIB"
    fi
fi

# Create launcher script (sets CWD to Resources for datafile.sdf)
cat > "$APP/Contents/MacOS/soulfu" << 'SCRIPT'
#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR/../Resources"
exec "$DIR/soulfu-bin" "$@"
SCRIPT
chmod +x "$APP/Contents/MacOS/soulfu"

# Copy game data
cp datafile.sdf "$APP/Contents/Resources/"
[ -f "Manual.htm" ] && cp Manual.htm "$APP/Contents/Resources/"
[ -f "soulfu.jpg" ] && cp soulfu.jpg "$APP/Contents/Resources/"
[ -f "packaging/license.txt" ] && cp packaging/license.txt "$APP/Contents/Resources/"

# Create .icns icon from PNGs
ICONSET="$(mktemp -d)/soulfu.iconset"
mkdir -p "$ICONSET"
ICON_SRC="packaging/icons/hicolor"
if [ -d "$ICON_SRC" ]; then
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
    echo "Warning: Icon PNGs not found, building without app icon"
    ICON_KEY=""
fi

# Create Info.plist
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

# --- Create DMG ---
mkdir -p packaging/bin
DMG_STAGING="$(mktemp -d)"
cp -R "$APP" "$DMG_STAGING/"
ln -s /Applications "$DMG_STAGING/Applications"

hdiutil create -volname "SoulFu" -srcfolder "$DMG_STAGING" -ov -format UDZO "$DMG_NAME"

if [ $? -eq 0 ]; then
    echo "Package ${DMG_NAME} created successfully"
else
    echo "Error: Failed to create DMG"
    rm -rf "$APP" "$DMG_STAGING"
    exit 1
fi

# Clean up
rm -rf "$APP" "$DMG_STAGING"
rm -f soulfu

echo ""
echo "Build completed successfully!"
echo "DMG: $DMG_NAME"
