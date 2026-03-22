#!/bin/bash
# Sets up the macOS build sysroot for SoulFu.
# Downloads SDL2/SDL2_net frameworks from GitHub,
# and compiles libogg, libvorbis, libjpeg-turbo from source.
#
# Run from project root: ./packaging/setup_macos_sysroot.sh

set -e

SYSROOT="/tmp/macos-sysroot"
BUILDDIR="$(mktemp -d)"

SDL2_VERSION="2.30.12"
SDL2_NET_VERSION="2.2.0"
LIBOGG_VERSION="1.3.6"
LIBVORBIS_VERSION="1.3.7"
LIBJPEG_VERSION="3.1.0"

cleanup() { rm -rf "$BUILDDIR"; }
trap cleanup EXIT

echo "=== SoulFu macOS sysroot setup ==="
echo "Sysroot: $SYSROOT"
echo "Build dir: $BUILDDIR"
mkdir -p "$SYSROOT/include" "$SYSROOT/lib" "$SYSROOT/Frameworks"

# ---------------------------------------------------------------------------
echo ""
echo "[1/5] SDL2 $SDL2_VERSION (macOS framework from GitHub)..."
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.dmg"
curl -L -o "$BUILDDIR/SDL2.dmg" "$SDL2_URL"
hdiutil attach "$BUILDDIR/SDL2.dmg" -quiet -mountpoint "$BUILDDIR/sdl2-vol"
cp -R "$BUILDDIR/sdl2-vol/SDL2.framework" "$SYSROOT/Frameworks/"
hdiutil detach "$BUILDDIR/sdl2-vol" -quiet
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[2/5] SDL2_net $SDL2_NET_VERSION (macOS framework from GitHub)..."
SDL2NET_URL="https://github.com/libsdl-org/SDL_net/releases/download/release-$SDL2_NET_VERSION/SDL2_net-$SDL2_NET_VERSION.dmg"
curl -L -o "$BUILDDIR/SDL2_net.dmg" "$SDL2NET_URL"
hdiutil attach "$BUILDDIR/SDL2_net.dmg" -quiet -mountpoint "$BUILDDIR/sdl2net-vol"
cp -R "$BUILDDIR/sdl2net-vol/SDL2_net.framework" "$SYSROOT/Frameworks/"
hdiutil detach "$BUILDDIR/sdl2net-vol" -quiet
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[3/5] libogg $LIBOGG_VERSION (compile from source)..."
OGG_URL="https://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VERSION.tar.xz"
curl -L -o "$BUILDDIR/libogg.tar.xz" "$OGG_URL"
tar -xf "$BUILDDIR/libogg.tar.xz" -C "$BUILDDIR"
(
    cd "$BUILDDIR/libogg-$LIBOGG_VERSION"
    ./configure --prefix="$SYSROOT" --quiet
    make -j"$(sysctl -n hw.ncpu)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[4/5] libvorbis $LIBVORBIS_VERSION (compile from source)..."
VORBIS_URL="https://downloads.xiph.org/releases/vorbis/libvorbis-$LIBVORBIS_VERSION.tar.xz"
curl -L -o "$BUILDDIR/libvorbis.tar.xz" "$VORBIS_URL"
tar -xf "$BUILDDIR/libvorbis.tar.xz" -C "$BUILDDIR"
(
    cd "$BUILDDIR/libvorbis-$LIBVORBIS_VERSION"
    CFLAGS="-I$SYSROOT/include" LDFLAGS="-L$SYSROOT/lib" \
    ./configure --prefix="$SYSROOT" --with-ogg="$SYSROOT" --quiet
    make -j"$(sysctl -n hw.ncpu)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[5/5] libjpeg-turbo $LIBJPEG_VERSION (compile from source)..."
JPEG_URL="https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEG_VERSION/libjpeg-turbo-$LIBJPEG_VERSION.tar.gz"
curl -L -o "$BUILDDIR/libjpeg-turbo.tar.gz" "$JPEG_URL"
tar -xzf "$BUILDDIR/libjpeg-turbo.tar.gz" -C "$BUILDDIR"
(
    cd "$BUILDDIR"
    mkdir jpeg-build && cd jpeg-build
    cmake "../libjpeg-turbo-$LIBJPEG_VERSION" \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_SHARED=ON \
        -DENABLE_STATIC=ON \
        -DCMAKE_INSTALL_LIBDIR=lib \
        > /dev/null
    make -j"$(sysctl -n hw.ncpu)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "=== Sysroot setup complete! ==="
echo ""
echo "Frameworks:"
ls "$SYSROOT/Frameworks/"
echo ""
echo "Sysroot libs:"
ls "$SYSROOT/lib/"*.dylib 2>/dev/null || ls "$SYSROOT/lib/"*.a 2>/dev/null
echo ""
echo "Build with:  make -f Makefile.macos"
