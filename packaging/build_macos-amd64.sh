#!/bin/bash

# macOS AMD64 DMG build script for SoulFu
# Usage: VERSION=1.8 ./packaging/build_macos-amd64.sh
#
# Requires: setup_macos_sysroot.sh to have been run first.

set -e

# Check if version is set via environment variable
if [ -z "$VERSION" ]; then
    echo "Error: VERSION environment variable not set"
    echo "Usage: VERSION=<version> $0"
    exit 1
fi

PACKAGE_NAME="soulfu"
ARCHITECTURE="amd64"
SYSROOT="/tmp/macos-sysroot"
APP="SoulFu.app"
DMG_NAME="packaging/bin/${PACKAGE_NAME}_${VERSION}_macos-${ARCHITECTURE}.dmg"

# Verify sysroot exists
if [ ! -d "$SYSROOT/Frameworks/SDL2.framework" ]; then
    echo "Error: Sysroot not found. Run packaging/setup_macos_sysroot.sh first."
    exit 1
fi

# Compile
rm -f soulfu
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

# Fix dylib load paths
install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/soulfu-bin"
install_name_tool -change "$SYSROOT/lib/libogg.0.dylib" @executable_path/../Frameworks/libogg.0.dylib "$APP/Contents/MacOS/soulfu-bin"
install_name_tool -change "$SYSROOT/lib/libvorbis.0.dylib" @executable_path/../Frameworks/libvorbis.0.dylib "$APP/Contents/MacOS/soulfu-bin"
# libjpeg may use @rpath — normalize it too
install_name_tool -change @rpath/libjpeg.62.dylib @executable_path/../Frameworks/libjpeg.62.dylib "$APP/Contents/MacOS/soulfu-bin" 2>/dev/null || true

# Create launcher script (sets CWD to Resources for datafile.sdf)
cat > "$APP/Contents/MacOS/soulfu" << 'SCRIPT'
#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR/../Resources"
exec "$DIR/soulfu-bin" "$@"
SCRIPT
chmod +x "$APP/Contents/MacOS/soulfu"

# Copy frameworks
cp -R "$SYSROOT/Frameworks/SDL2.framework" "$APP/Contents/Frameworks/"
cp -R "$SYSROOT/Frameworks/SDL2_net.framework" "$APP/Contents/Frameworks/"

# Copy dylibs
cp "$SYSROOT/lib/libogg.0.dylib" "$APP/Contents/Frameworks/"
cp "$SYSROOT/lib/libvorbis.0.dylib" "$APP/Contents/Frameworks/"
cp "$SYSROOT/lib/libjpeg.62.dylib" "$APP/Contents/Frameworks/"

# Fix libvorbis -> libogg dependency
install_name_tool -change "$SYSROOT/lib/libogg.0.dylib" @executable_path/../Frameworks/libogg.0.dylib "$APP/Contents/Frameworks/libvorbis.0.dylib"

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
