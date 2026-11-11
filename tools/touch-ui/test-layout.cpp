#include "../../Hurrican/src/SDLPort/TouchLayout.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string path = argv[1];
    TouchLayout::Layout a{};
    a.stick = {0.2f, 0.7f};
    for (std::size_t i = 0; i < a.buttons.size(); ++i) a.buttons[i] = {0.1f + i * 0.08f, 0.8f};
    assert(TouchLayout::Save(path, a));
    TouchLayout::Layout b{};
    assert(TouchLayout::Load(path, b));
    assert(b.stick.x == a.stick.x && b.buttons[8].x == a.buttons[8].x);
    auto previous = b;
    { std::ofstream out(path); out << "HURRICAN_TOUCH_LAYOUT 1\n0.2 0.7\n0.5 0.5\n"; }
    assert(!TouchLayout::Load(path, b));
    assert(b.stick.x == previous.stick.x && b.buttons[8].x == previous.buttons[8].x);
    a.buttons[0].x = std::numeric_limits<float>::quiet_NaN();
    assert(!TouchLayout::Save(path, a));
    { std::ofstream out(path); out << "HURRICAN_TOUCH_LAYOUT 999\n"; }
    assert(!TouchLayout::Load(path, b));
    { std::ofstream out(path); out << "HURRICAN_TOUCH_LAYOUT 1\n2 0.7\n"; }
    assert(!TouchLayout::Load(path, b));
    a.buttons[0].x = 0.1f;
    assert(TouchLayout::Save(path, a));
    { std::ofstream out(path, std::ios::app); out << "unexpected\n"; }
    assert(!TouchLayout::Load(path, b));
    assert(!TouchLayout::Save(path + "/not-a-directory/file", a));
    std::remove(path.c_str());
}
