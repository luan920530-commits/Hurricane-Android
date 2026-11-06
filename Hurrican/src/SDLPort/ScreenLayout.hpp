#ifndef HURRICAN_SCREEN_LAYOUT_HPP
#define HURRICAN_SCREEN_LAYOUT_HPP

#include <cstdint>

namespace ScreenLayout {
struct Rect { int x, y, w, h; };
// El juego es 4:3. Encajarlo en ambas dimensiones, incluso en ventanas casi cuadradas.
inline Rect FitGame(int width, int height) {
    if (width <= 0 || height <= 0) return {0, 0, 0, 0};
    int w, h;
    if (static_cast<std::int64_t>(width) * 3 < static_cast<std::int64_t>(height) * 4) {
        w = width;
        h = static_cast<int>(static_cast<std::int64_t>(width) * 3 / 4);
    } else {
        h = height;
        w = static_cast<int>(static_cast<std::int64_t>(height) * 4 / 3);
    }
    return {(width - w) / 2, (height - h) / 2, w, h};
}
}
#endif
