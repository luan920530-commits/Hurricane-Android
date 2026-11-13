// Versioned, independent persistence for the Android touch layout.
#ifndef HURRICAN_TOUCH_LAYOUT_HPP
#define HURRICAN_TOUCH_LAYOUT_HPP

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

namespace TouchLayout {
constexpr std::size_t kButtons = 9;
struct Point { float x, y; };
struct Layout { Point stick; std::array<Point, kButtons> buttons; };

inline bool valid(Point p, float lo, float hi) {
    return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= lo && p.x <= hi && p.y >= lo && p.y <= hi;
}
inline bool Load(const std::string &path, Layout &out) {
    std::ifstream in(path);
    std::string magic;
    int version = 0;
    Layout candidate{};
    if (!(in >> magic >> version) || magic != "HURRICAN_TOUCH_LAYOUT" || version != 1 ||
        !(in >> candidate.stick.x >> candidate.stick.y) || !valid(candidate.stick, 0.05f, 0.95f))
        return false;
    for (auto &p : candidate.buttons) {
        if (!(in >> p.x >> p.y) || !valid(p, 0.03f, 0.97f)) return false;
    }
    std::string extra;
    if (in >> extra) return false;
    out = candidate;
    return true;
}
inline bool Save(const std::string &path, const Layout &layout) {
    if (!valid(layout.stick, 0.05f, 0.95f)) return false;
    for (const auto &p : layout.buttons) if (!valid(p, 0.03f, 0.97f)) return false;
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) return false;
        out << "HURRICAN_TOUCH_LAYOUT 1\n" << std::setprecision(9)
            << layout.stick.x << ' ' << layout.stick.y << '\n';
        for (const auto &p : layout.buttons) out << p.x << ' ' << p.y << '\n';
        out.close();
        if (!out) { std::remove(temp.c_str()); return false; }
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}
}
#endif
