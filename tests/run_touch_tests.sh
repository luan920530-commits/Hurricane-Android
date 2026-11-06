#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
g++ -std=c++17 -Wno-unused-result -ffunction-sections -fdata-sections \
  -DANDROID -DUSE_GL2 -DUSE_GLES2 -DUSE_FBO \
  -I Hurrican/src -I Hurrican/src/SDLPort -I Hurrican/3rdparty/glm \
  -I android/app/src/main/jni/SDL/include -I android/app/src/main/jni/SDL_image/include \
  -I android/app/src/main/jni/SDL_mixer/include -I android/app/src/main/jni/SDL/src/video/khronos \
  tests/touch_controls_test.cpp Hurrican/src/SDLPort/TouchControls.cpp \
  -Wl,--gc-sections -o "$tmp/touch_controls_test"
"$tmp/touch_controls_test"
