# ---------------------------------------------------------------------------------------
# Hurrican for Android - native build entry point
#
# This CMake project builds everything the APK needs into a single shared library named
# "main", which the SDL Java activity (SDLActivity) loads.
#
#   libSDL2.so       - SDL2  (shared, required on Android)
#   libSDL2_image.so - SDL2_image (shared, PNG/SDL_image loaders)
#   libmain.so       - libSDL2_mixer + libxmp + the whole Hurrican engine (static)
#
# Gradle passes this file through externalNativeBuild, so no manual NDK invocation is
# needed: the Android Gradle Plugin drives CMake and the NDK for every ABI.
# ---------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.18)

project(hurrican_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Silences some warnings-only issues in the vendored third party code and the 2002 era
# engine code, while keeping the build readable.
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------------------
# SDL2 (shared library, this is the standard layout on Android)
# ---------------------------------------------------------------------------------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------
# SDL2_image: only the PNG loader matters for Hurrican. stb_image is used as backend so
# no external libpng/zlib submodules are required.
# ---------------------------------------------------------------------------------------
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG_SAVE OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SVG OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
add_subdirectory(SDL_image EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------
# libxmp: Impulse Tracker replay. Hurrican ships all of its music as .it modules, so this
# is mandatory (SDL_mixer's own mikmod is not available in SDL2_mixer 2.x).
# ---------------------------------------------------------------------------------------
#
# Quien lo anade al proyecto es SDL_mixer (add_subdirectory), no este fichero: en modo
# "vendored" busca el proyecto en SDL_mixer/external/libxmp, o sea que el arbol de libxmp
# vive ahi. Estas variables tienen que estar puestas antes de add_subdirectory(SDL_mixer),
# que es donde se procesa.
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LIBXMP_PIC ON CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_DEPACKERS OFF CACHE BOOL "" FORCE)
set(LIBXMP_DISABLE_PROWIZARD ON CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)


# ---------------------------------------------------------------------------------------
# SDL2_mixer (static, linked into libmain.so together with libxmp)
# ---------------------------------------------------------------------------------------
# VENDORED ON = usa el libxmp que llevamos en external/, sin buscar paquetes del sistema.
set(SDL2MIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_XMP ON CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_XMP_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_MODPLUG OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC ON CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_FLAC_DRFLAC ON CACHE BOOL "" FORCE)
set(SDL2MIXER_VORBIS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
# SDL2MIXER_MOD_XMP usa el libxmp vendored; no marcar como "shared" para que quede estatico
# dentro de libmain.so.
set(SDL2MIXER_MOD_XMP_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL_mixer EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------
# The game itself
# ---------------------------------------------------------------------------------------
set(HURRICAN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Hurrican")
get_filename_component(HURRICAN_DIR "${HURRICAN_DIR}" ABSOLUTE)

if(NOT EXISTS "${HURRICAN_DIR}/src/Main.cpp")
    message(FATAL_ERROR
        "Hurrican sources not found at ${HURRICAN_DIR}.\n"
        "The Android project expects the upstream Hurrican tree (src/, data/, lang/) "
        "at the repository root.")
endif()

# Every .cpp of the engine, collected automatically so upstream files added later are
# picked up without touching this file.
file(GLOB_RECURSE HURRICAN_SOURCES CONFIGURE_DEPENDS
    "${HURRICAN_DIR}/src/*.cpp")
# Ported from the desktop SDLPort, not needed (or buildable) on Android.
list(FILTER HURRICAN_SOURCES EXCLUDE REGEX "/SDLPort/eglport\\.cpp$")

add_library(main SHARED ${HURRICAN_SOURCES})

target_include_directories(main PRIVATE
    "${HURRICAN_DIR}/src"
    "${HURRICAN_DIR}/src/SDLPort"
    "${HURRICAN_DIR}/3rdparty/glm"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL/include")

# ---------------------------------------------------------------------------------------
# Engine configuration for Android
#
#   ANDROID      -> enables the Android code paths (logging, paths, touch controls)
#   USE_GL2      -> programmable pipeline renderer
#   USE_GLES2    -> OpenGL ES 2.0 flavour of that renderer (the only GL ES level Android
#                   guarantees, works everywhere from Android 4.3 to current)
#   USE_FBO      -> render to a 640x480 texture and scale it to the screen with correct
#                   aspect ratio; this is what makes phones with odd resolutions look right
#   USE_FAST_RNG / USE_FAST_TRIG -> cheaper math, helps on low end ARM devices
#
# USE_ETC1 (texturas comprimidas) esta desactivado a proposito: los .pkm ocupan mas que los
# PNG originales en este juego (58 MB frente a 27 MB) porque casi todas sus texturas son
# pequenas y ETC1 no comprime por debajo de 8 bytes cada bloque de 4x4. Se puede activar con
# -DHURRICAN_USE_ETC1=ON tras generar los .pkm con tools/generate_etc1.py.
# ---------------------------------------------------------------------------------------
option(HURRICAN_USE_ETC1 "Usar texturas ETC1 comprimidas (generar antes con tools/generate_etc1.py)" OFF)
option(HURRICAN_DEBUG_TOUCH "Dibujar las zonas tactiles pulsadas (depuracion)" OFF)

target_compile_definitions(main PRIVATE
    ANDROID
    GL_GLEXT_PROTOTYPES
    USE_GL2
    USE_GLES2
    USE_FBO
    USE_FAST_RNG
    USE_FAST_TRIG
    DJBOBO_NO_EXCEPTION_HOOK
    $<$<CONFIG:Release>:NDEBUG>)

target_compile_options(main PRIVATE
    -fno-strict-aliasing
    -Wno-multichar
    -Wno-write-strings
    $<$<CONFIG:Release>:-O2>)

if(HURRICAN_USE_ETC1)
    target_compile_definitions(main PRIVATE USE_ETC1)
endif()

target_link_libraries(main PRIVATE
    SDL2
    SDL2_image
    SDL2_mixer
    GLESv2
    OpenSLES
    log
    android
    dl
    m)

# SDLActivity carga "main" (libmain.so) desde Java y busca SDL_main() dentro con dlsym,
# por eso el nombre de salida debe ser exactamente "main".
set_target_properties(main PROPERTIES OUTPUT_NAME "main")

# ---------------------------------------------------------------------------------------
# Assets (los prepara Gradle, no CMake)
#
# AGP empaqueta los assets ANTES de invocar a CMake, asi que el paso que copia
# Hurrican/{data,lang} a app/build/generated/assets/hurrican vive en app/build.gradle
# (tarea copyHurricanAssets). Si se hiciera aqui, la primera compilacion generaria un APK
# sin los datos del juego.
# ---------------------------------------------------------------------------------------
