#!/bin/bash
# Sets up the macOS amd64 cross-compilation sysroot for SoulFu on Linux.
# Requires osxcross to be installed. Set OSXCROSS env var to its root.
# Cross-compiles SDL2, SDL2_net, libogg, libvorbis, libjpeg-turbo.
#
# Run from project root: OSXCROSS=/opt/osxcross ./packaging/setup_macos_amd64_sysroot.sh

set -e

# --- Locate osxcross ---
if [ -z "$OSXCROSS" ]; then
    for p in /opt/osxcross /usr/local/osxcross "$HOME/osxcross"; do
        [ -d "$p/bin" ] && OSXCROSS="$p" && break
    done
fi
if [ -z "$OSXCROSS" ] || [ ! -d "$OSXCROSS/bin" ]; then
    echo "Error: osxcross not found. Set OSXCROSS=/path/to/osxcross"
    echo "See: https://github.com/tpoechtrager/osxcross"
    exit 1
fi

export PATH="$OSXCROSS/bin:$PATH"

# Find the compiler triple
CROSS_CC="$(ls "$OSXCROSS/bin"/x86_64-apple-darwin*-clang 2>/dev/null | head -1)"
if [ -z "$CROSS_CC" ]; then
    echo "Error: x86_64-apple-darwin*-clang not found in $OSXCROSS/bin"
    exit 1
fi
CROSS_CXX="${CROSS_CC}++"
HOST="$(basename "$CROSS_CC" | sed 's/-clang$//')"
CROSS_AR="$(ls "$OSXCROSS/bin"/${HOST}-ar 2>/dev/null | head -1)"
CROSS_RANLIB="$(ls "$OSXCROSS/bin"/${HOST}-ranlib 2>/dev/null | head -1)"
SDK_PATH="$(${CROSS_CC} -print-sysroot 2>/dev/null || echo "")"

SYSROOT="/tmp/macos-amd64-sysroot"
BUILDDIR="$(mktemp -d)"

SDL2_VERSION="2.30.12"
SDL2_NET_VERSION="2.2.0"
LIBOGG_VERSION="1.3.6"
LIBVORBIS_VERSION="1.3.7"
LIBJPEG_VERSION="3.1.0"

cleanup() { rm -rf "$BUILDDIR"; }
trap cleanup EXIT

echo "=== SoulFu macOS amd64 cross-compile sysroot ==="
echo "osxcross:    $OSXCROSS"
echo "Host triple: $HOST"
echo "Compiler:    $CROSS_CC"
echo "Sysroot:     $SYSROOT"
echo ""
mkdir -p "$SYSROOT/include" "$SYSROOT/lib"

# ---------------------------------------------------------------------------
echo "[1/5] SDL2 $SDL2_VERSION (cross-compile from source)..."
wget -q --show-progress -O "$BUILDDIR/sdl2.tar.gz" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz"
tar -xzf "$BUILDDIR/sdl2.tar.gz" -C "$BUILDDIR"
(
    cd "$BUILDDIR"
    mkdir sdl2-build && cd sdl2-build
    cmake "../SDL2-$SDL2_VERSION" \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DCMAKE_C_COMPILER="$CROSS_CC" \
        -DCMAKE_CXX_COMPILER="$CROSS_CXX" \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=ON -DSDL_STATIC=ON \
        -DSDL_FRAMEWORK=OFF \
        > /dev/null
    make -j"$(nproc)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[2/5] SDL2_net $SDL2_NET_VERSION (cross-compile from source)..."
wget -q --show-progress -O "$BUILDDIR/sdl2_net.tar.gz" \
    "https://github.com/libsdl-org/SDL_net/releases/download/release-$SDL2_NET_VERSION/SDL2_net-$SDL2_NET_VERSION.tar.gz"
tar -xzf "$BUILDDIR/sdl2_net.tar.gz" -C "$BUILDDIR"
(
    cd "$BUILDDIR/SDL2_net-$SDL2_NET_VERSION"
    ./configure --host="$HOST" --prefix="$SYSROOT" \
        --with-sdl-prefix="$SYSROOT" \
        CC="$CROSS_CC" AR="$CROSS_AR" RANLIB="$CROSS_RANLIB" \
        CFLAGS="-I$SYSROOT/include/SDL2 -I$SYSROOT/include" \
        LDFLAGS="-L$SYSROOT/lib" \
        --quiet
    make -j"$(nproc)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[3/5] libogg $LIBOGG_VERSION (cross-compile from source)..."
wget -q --show-progress -O "$BUILDDIR/libogg.tar.xz" \
    "https://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VERSION.tar.xz"
tar -xf "$BUILDDIR/libogg.tar.xz" -C "$BUILDDIR"
(
    cd "$BUILDDIR/libogg-$LIBOGG_VERSION"
    ./configure --host="$HOST" --prefix="$SYSROOT" \
        CC="$CROSS_CC" AR="$CROSS_AR" RANLIB="$CROSS_RANLIB" \
        --quiet
    make -j"$(nproc)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[4/5] libvorbis $LIBVORBIS_VERSION (cross-compile from source)..."
wget -q --show-progress -O "$BUILDDIR/libvorbis.tar.xz" \
    "https://downloads.xiph.org/releases/vorbis/libvorbis-$LIBVORBIS_VERSION.tar.xz"
tar -xf "$BUILDDIR/libvorbis.tar.xz" -C "$BUILDDIR"
(
    cd "$BUILDDIR/libvorbis-$LIBVORBIS_VERSION"
    CFLAGS="-I$SYSROOT/include" LDFLAGS="-L$SYSROOT/lib" \
    ./configure --host="$HOST" --prefix="$SYSROOT" \
        CC="$CROSS_CC" AR="$CROSS_AR" RANLIB="$CROSS_RANLIB" \
        --with-ogg="$SYSROOT" --quiet
    make -j"$(nproc)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "[5/5] libjpeg-turbo $LIBJPEG_VERSION (cross-compile from source)..."
wget -q --show-progress -O "$BUILDDIR/libjpeg-turbo.tar.gz" \
    "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEG_VERSION/libjpeg-turbo-$LIBJPEG_VERSION.tar.gz"
tar -xzf "$BUILDDIR/libjpeg-turbo.tar.gz" -C "$BUILDDIR"
(
    cd "$BUILDDIR"
    mkdir jpeg-build && cd jpeg-build
    cmake "../libjpeg-turbo-$LIBJPEG_VERSION" \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DCMAKE_C_COMPILER="$CROSS_CC" \
        -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DENABLE_SHARED=ON -DENABLE_STATIC=ON \
        -DWITH_TURBOJPEG=OFF \
        -DWITH_SIMD=OFF \
        > /dev/null
    make -j"$(nproc)" --quiet
    make install --quiet
)
echo "  done."

# ---------------------------------------------------------------------------
echo ""
echo "=== Cross-compile sysroot setup complete! ==="
echo ""
echo "Sysroot libs:"
ls "$SYSROOT/lib/"*.dylib 2>/dev/null || ls "$SYSROOT/lib/"*.a 2>/dev/null
echo ""
echo "Build with:  make -f Makefile.macos-amd64 release"
