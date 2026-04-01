cmake_minimum_required(VERSION 3.22.1)
project(soulfu C)

include(ExternalProject)

# Paths: jni/ -> app/ -> android/ -> packaging/ -> project root
set(SOULFU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(SOULFU_SOURCE "${SOULFU_ROOT}/source")
set(ANDROID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# ============================================================
# SDL2 — build from source as a shared library
# ============================================================
set(SDL2_SOURCE "${ANDROID_DIR}/SDL2")
if(NOT EXISTS "${SDL2_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL2 source not found at ${SDL2_SOURCE}.\n"
        "Run:  cd packaging/android && ./setup_deps.sh")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory("${SDL2_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/SDL2" EXCLUDE_FROM_ALL)

# ============================================================
# gl4es — OpenGL 1.x to OpenGL ES 2.0 translation layer
# ============================================================
set(GL4ES_SOURCE "${ANDROID_DIR}/gl4es")
if(NOT EXISTS "${GL4ES_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR
        "gl4es source not found at ${GL4ES_SOURCE}.\n"
        "Run:  cd packaging/android && ./setup_deps.sh")
endif()

set(NOX11 ON CACHE BOOL "" FORCE)
set(NOEGL OFF CACHE BOOL "" FORCE)
set(GLX_STUBS ON CACHE BOOL "" FORCE)
# STATICLIB=ON builds gl4es as a static library, avoiding the .so.1 versioned
# name issue on Android. GL symbols are baked directly into libsoulfu.so.
set(STATICLIB ON CACHE BOOL "" FORCE)
add_subdirectory("${GL4ES_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/gl4es" EXCLUDE_FROM_ALL)
set(STATICLIB OFF CACHE BOOL "" FORCE)

# ============================================================
# libjpeg-turbo — built as ExternalProject (blocks add_subdirectory)
# ============================================================
set(JPEG_SOURCE "${ANDROID_DIR}/libjpeg-turbo")
if(NOT EXISTS "${JPEG_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR
        "libjpeg-turbo not found at ${JPEG_SOURCE}.\n"
        "Run:  cd packaging/android && ./setup_deps.sh")
endif()

set(JPEG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/libjpeg-turbo")
set(JPEG_LIB "${JPEG_BUILD_DIR}/libjpeg.a")

ExternalProject_Add(libjpeg_turbo_ext
    SOURCE_DIR   "${JPEG_SOURCE}"
    BINARY_DIR   "${JPEG_BUILD_DIR}"
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=${ANDROID_PLATFORM}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DENABLE_SHARED=OFF
        -DENABLE_STATIC=ON
        -DWITH_TURBOJPEG=OFF
        -DCMAKE_INSTALL_PREFIX=${JPEG_BUILD_DIR}/install
    BUILD_BYPRODUCTS "${JPEG_LIB}"
)

add_library(jpeg STATIC IMPORTED GLOBAL)
set_target_properties(jpeg PROPERTIES IMPORTED_LOCATION "${JPEG_LIB}")
add_dependencies(jpeg libjpeg_turbo_ext)

# libjpeg-turbo 3.x moved headers to src/; jconfig.h is generated into build dir
set(JPEG_INCLUDE_DIRS "${JPEG_SOURCE}/src" "${JPEG_SOURCE}" "${JPEG_BUILD_DIR}")

# ============================================================
# libogg + libvorbis — needed for OGG audio
# ============================================================
set(OGG_SOURCE "${ANDROID_DIR}/libogg")
set(VORBIS_SOURCE "${ANDROID_DIR}/libvorbis")

if(NOT EXISTS "${OGG_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR
        "libogg not found at ${OGG_SOURCE}.\n"
        "Run:  cd packaging/android && ./setup_deps.sh")
endif()

if(NOT EXISTS "${VORBIS_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR
        "libvorbis not found at ${VORBIS_SOURCE}.\n"
        "Run:  cd packaging/android && ./setup_deps.sh")
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory("${OGG_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/libogg" EXCLUDE_FROM_ALL)

set(OGG_INCLUDE_DIR "${OGG_SOURCE}/include" CACHE PATH "" FORCE)
set(OGG_LIBRARY ogg CACHE STRING "" FORCE)
add_subdirectory("${VORBIS_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/libvorbis" EXCLUDE_FROM_ALL)

# ============================================================
# SoulFu — the game itself
# ============================================================
add_library(soulfu SHARED "${SOULFU_SOURCE}/soulfu.c")

target_compile_definitions(soulfu PRIVATE
    __ANDROID__
    SDF_PATH="datafile.sdf"
)

target_include_directories(soulfu PRIVATE
    "${SOULFU_SOURCE}"
    "${SDL2_SOURCE}/include"
    "${GL4ES_SOURCE}/include"
    ${JPEG_INCLUDE_DIRS}
    "${OGG_SOURCE}/include"
    "${VORBIS_SOURCE}/include"
)

# Note: gl4es library target name is "GL" in ptitSeb/gl4es CMake
target_link_libraries(soulfu PRIVATE
    SDL2
    GL
    jpeg
    ogg
    vorbis
    android
    log
    EGL
    GLESv2
    m
)

add_dependencies(soulfu libjpeg_turbo_ext)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_link_options(soulfu PRIVATE "-s")
endif()
