# Building SoulFu

## Linux

Clone the repository:

```sh
git clone --recurse-submodules <repolink> soulfu
cd soulfu
```

Install the required libraries. On Debian/Ubuntu:

```sh
sudo apt-get install \
    build-essential \
    libsdl2-dev \
    libsdl2-net-dev \
    libogg-dev \
    libvorbis-dev \
    libjpeg-dev \
    libgl-dev
```

Build and run:

```sh
make
./soulfu
```

`make` compiles the game binary and builds `datafile.sdf` (the game data archive) in one step. The data tools (`sdp`, `slc`, `ssc`) are compiled automatically as needed.

To build with in-game development tools enabled:

```sh
make debug
```

---

## Cross-compiling for Windows (MinGW-w64)

Cross-compilation is done on Linux using MinGW-w64. You will need:

- `mingw-w64`
- `cmake`
- `wget`

Set up the sysroot by running `packaging/setup_win64_sysroot.sh` from the repository root. This downloads SDL2 and SDL2\_net from their official releases and cross-compiles libogg, libvorbis, and libjpeg-turbo from source into `/tmp/mingw64-sysroot`.

Once the sysroot is in place, build with:

```sh
make -f Makefile.mingw64
```
