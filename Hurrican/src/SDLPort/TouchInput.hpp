// Logica de reparto tactil independiente de SDL y del dibujo; tambien se prueba en escritorio.
#ifndef HURRICAN_TOUCH_INPUT_HPP
#define HURRICAN_TOUCH_INPUT_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace TouchInput {
struct Finger { std::int64_t device, id; float x, y, start_x, start_y; };
struct Button { float x, y, radius; };
struct Rect { float x, y, w, h; };

class Stick {
public:
    void Reset() { active = false; x = y = 0; }

    void Update(const std::vector<Finger> &fingers, const Rect &area,
                const std::vector<Button> &buttons, float default_x, float default_y) {
        const float radius = area.h * 0.135f;
        const Finger *owner = nullptr;
        if (active) {
            for (const auto &finger : fingers)
                if (finger.device == device && finger.id == id) { owner = &finger; break; }
            if (!owner) Reset();
        }
        if (!active) {
            for (const auto &finger : fingers) {
                bool on_button = false;
                for (const auto &button : buttons) {
                    const float dx = finger.start_x - button.x, dy = finger.start_y - button.y;
                    if (dx * dx + dy * dy <= button.radius * button.radius) { on_button = true; break; }
                }
                if (on_button) continue;
                const float nx = (finger.start_x - area.x) / area.w;
                const float ny = (finger.start_y - area.y) / area.h;
                const float cx = area.x + default_x * area.w, cy = area.y + default_y * area.h;
                const bool on_stick = std::hypot(finger.start_x - cx, finger.start_y - cy) < radius * 1.2f;
                if ((nx >= 0 && nx < 0.42f && ny >= 0.25f && ny <= 1.0f) || on_stick) {
                    active = true; device = finger.device; id = finger.id; owner = &finger;
                    origin_x = on_stick ? cx : finger.start_x;
                    origin_y = on_stick ? cy : finger.start_y;
                    break;
                }
            }
        }
        x = y = 0;
        if (owner) {
            x = owner->x - origin_x; y = owner->y - origin_y;
            const float length = std::hypot(x, y);
            if (length > radius && length > 0) { x *= radius / length; y *= radius / length; }
        }
    }
    bool Owns(const Finger &finger) const { return active && device == finger.device && id == finger.id; }
    bool Left(float radius) const { return x < -radius * 0.30f; }
    bool Right(float radius) const { return x > radius * 0.30f; }
    bool Up(float radius) const { return y < -radius * 0.30f; }
    bool Down(float radius) const { return y > radius * 0.30f; }
    bool active = false;
    std::int64_t device = 0, id = 0;
    float origin_x = 0, origin_y = 0, x = 0, y = 0;
};

inline int Hit(const std::vector<Button> &buttons, float x, float y) {
    int best = -1;
    float best_distance = 1.0f;
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        const auto &b = buttons[i];
        if (b.radius <= 0) continue;
        const float dx = (x - b.x) / b.radius, dy = (y - b.y) / b.radius;
        const float distance = dx * dx + dy * dy;
        if (distance <= best_distance) { best_distance = distance; best = static_cast<int>(i); }
    }
    return best;
}
} // namespace TouchInput
#endif
