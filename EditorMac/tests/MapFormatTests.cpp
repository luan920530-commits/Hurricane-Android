#include "../src/MapDocument.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
using hurrican::editor::MapDocument;

static std::string ReadBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: map_format_tests <Hurrican/data/levels>\n";
        return 2;
    }

    const fs::path levels = argv[1];
    const fs::path scratch = fs::temp_directory_path() / "hurrican-map-roundtrip-test.map";
    std::size_t checked = 0;

    for (const auto& entry : fs::recursive_directory_iterator(levels)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".map")
            continue;

        MapDocument map;
        std::string error;
        if (!MapDocument::Load(entry.path(), map, error)) {
            std::cerr << "FAIL load " << entry.path() << ": " << error << '\n';
            return 1;
        }
        if (!map.Save(scratch, error)) {
            std::cerr << "FAIL save " << entry.path() << ": " << error << '\n';
            return 1;
        }
        if (ReadBytes(entry.path()) != ReadBytes(scratch)) {
            std::cerr << "FAIL round-trip changed bytes in " << entry.path() << '\n';
            return 1;
        }
        ++checked;
    }

    std::error_code ignored;
    fs::remove(scratch, ignored);
    std::cout << "PASS: loaded and byte-for-byte round-tripped " << checked << " .map files.\n";
    return checked == 0 ? 1 : 0;
}
