// Android offline store. Balances and pending items live outside the game assets.
#ifndef HURRICAN_ANDROID_STORE_HPP
#define HURRICAN_ANDROID_STORE_HPP
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

namespace OfflineStore {
struct Inventory {
    int coins = 0;
    int lives = 0;    // consumables waiting for the next new game
    int weapons = 0;  // permanent starting upgrades, maximum three
    int bombs = 0;    // consumables waiting for the next new game
};
inline bool Valid(const Inventory &v) {
    return v.coins >= 0 && v.coins <= 1000000 && v.lives >= 0 && v.lives <= 9 &&
           v.weapons >= 0 && v.weapons <= 3 && v.bombs >= 0 && v.bombs <= 9;
}
// Missing file is a new account. A damaged/unreadable file is never overwritten.
inline bool Load(const std::string &path, Inventory &v) {
    errno = 0;
    std::ifstream in(path);
    if (!in) {
        if (errno == ENOENT) { v = Inventory{}; return true; }
        return false;
    }
    Inventory result;
    std::string magic, extra;
    int version;
    if (!(in >> magic >> version >> result.coins >> result.lives >> result.weapons >> result.bombs) ||
        magic != "HURRICAN_STORE" || version != 1 || !Valid(result) || (in >> extra)) return false;
    v = result;
    return true;
}
inline bool Save(const std::string &path, const Inventory &v) {
    if (!Valid(v)) return false;
    std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) return false;
        out << "HURRICAN_STORE 1\n" << v.coins << ' ' << v.lives << ' ' << v.weapons << ' ' << v.bombs << '\n';
        out.close();
        if (!out) { std::remove(temp.c_str()); return false; }
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) { std::remove(temp.c_str()); return false; }
    return true;
}
inline bool Award(const std::string &path) {
    Inventory v;
    if (!Load(path, v) || v.coins >= 1000000) return false;
    ++v.coins;
    return Save(path, v);
}
// 0: life (10), 1: permanent weapon upgrade (20), 2: bomb (5).
inline bool Buy(const std::string &path, int item) {
    Inventory v;
    if (!Load(path, v)) return false;
    int *count = nullptr, price = 0, limit = 0;
    switch (item) {
        case 0: count = &v.lives; price = 10; limit = 9; break;
        case 1: count = &v.weapons; price = 20; limit = 3; break;
        case 2: count = &v.bombs; price = 5; limit = 9; break;
        default: return false;
    }
    if (v.coins < price || *count >= limit) return false;
    v.coins -= price;
    ++*count;
    return Save(path, v);
}
// Consume pending items transactionally before applying them to a new game.
inline bool TakeForNewGame(const std::string &path, Inventory &out) {
    Inventory v;
    if (!Load(path, v)) return false;
    Inventory remaining = v;
    remaining.lives = remaining.bombs = 0;
    if ((v.lives || v.bombs) && !Save(path, remaining)) return false;
    out = v;
    return true;
}
}
#endif
