#include "../../Hurrican/src/SDLPort/Store.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string path = argv[1];
    std::remove(path.c_str());
    OfflineStore::Inventory v;
    assert(OfflineStore::Load(path, v) && v.coins == 0);
    assert(!OfflineStore::Buy(path, 0));
    for (int i = 0; i < 35; ++i) assert(OfflineStore::Award(path));
    assert(OfflineStore::Buy(path, 0));
    assert(OfflineStore::Buy(path, 1));
    assert(OfflineStore::Buy(path, 2));
    assert(!OfflineStore::Buy(path, 2));
    assert(OfflineStore::Load(path, v));
    assert(v.coins == 0 && v.lives == 1 && v.weapons == 1 && v.bombs == 1);
    assert(OfflineStore::TakeForNewGame(path, v));
    assert(v.lives == 1 && v.bombs == 1 && v.weapons == 1);
    assert(OfflineStore::TakeForNewGame(path, v));
    assert(v.lives == 0 && v.bombs == 0 && v.weapons == 1);
    v.coins = 100;
    v.weapons = 3;
    assert(OfflineStore::Save(path, v));
    assert(!OfflineStore::Buy(path, 1));
    { std::ofstream out(path); out << "HURRICAN_STORE 1\n-1 0 0 0\n"; }
    assert(!OfflineStore::Load(path, v));
    assert(!OfflineStore::Award(path));
    assert(!OfflineStore::Buy(path, 0));
    assert(!OfflineStore::TakeForNewGame(path, v));
    std::ifstream in(path);
    std::string magic;
    in >> magic;
    assert(magic == "HURRICAN_STORE");
    std::remove(path.c_str());
}
