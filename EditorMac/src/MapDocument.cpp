#include "MapDocument.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace hurrican::editor {
namespace {

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "Hurrican .map files are little-endian; build this editor for Intel macOS.");
#endif

bool ReadExact(std::ifstream& stream, void* destination, std::size_t bytes) {
    stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(bytes));
    return stream.good() || (stream.eof() && static_cast<std::size_t>(stream.gcount()) == bytes);
}

bool WriteExact(std::ofstream& stream, const void* source, std::size_t bytes) {
    stream.write(static_cast<const char*>(source), static_cast<std::streamsize>(bytes));
    return stream.good();
}

} // namespace

MapTile& MapDocument::tile(std::uint32_t x, std::uint32_t y) {
    if (x >= width() || y >= height())
        throw std::out_of_range("Hurrican map tile coordinate is outside the level");
    return tiles[static_cast<std::size_t>(x) * height() + y];
}

const MapTile& MapDocument::tile(std::uint32_t x, std::uint32_t y) const {
    if (x >= width() || y >= height())
        throw std::out_of_range("Hurrican map tile coordinate is outside the level");
    return tiles[static_cast<std::size_t>(x) * height() + y];
}

std::uint64_t MapDocument::ExpectedFileSize() const noexcept {
    return sizeof(MapHeader) + static_cast<std::uint64_t>(width()) * height() * sizeof(MapTile) +
           static_cast<std::uint64_t>(header.object_count) * sizeof(MapObject) + sizeof(MapAppendix);
}

bool MapDocument::Load(const std::filesystem::path& path, MapDocument& out, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Cannot open map: " + path.string();
        return false;
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) < sizeof(MapHeader)) {
        error = "File is too small to contain a Hurrican map header.";
        return false;
    }
    const auto file_size = static_cast<std::uint64_t>(end);
    input.seekg(0, std::ios::beg);

    MapDocument parsed;
    if (!ReadExact(input, &parsed.header, sizeof(parsed.header))) {
        error = "Could not read the complete Hurrican map header.";
        return false;
    }

    if (parsed.header.width == 0 || parsed.header.width > kMaxWidth ||
        parsed.header.height == 0 || parsed.header.height > kMaxHeight) {
        error = "Map dimensions are invalid or exceed the game limits.";
        return false;
    }
    if (parsed.header.tileset_count > 64 || parsed.header.object_count > kMaxObjects) {
        error = "Map header contains an unsupported tileset or object count.";
        return false;
    }

    const std::uint64_t expected = parsed.ExpectedFileSize();
    if (expected != file_size) {
        error = "Map size does not match its header (expected " + std::to_string(expected) +
                " bytes, found " + std::to_string(file_size) + ").";
        return false;
    }

    const std::uint64_t tile_count = static_cast<std::uint64_t>(parsed.width()) * parsed.height();
    if (tile_count > std::numeric_limits<std::size_t>::max() / sizeof(MapTile)) {
        error = "Map is too large for this computer.";
        return false;
    }

    parsed.tiles.resize(static_cast<std::size_t>(tile_count));
    if (!parsed.tiles.empty() &&
        !ReadExact(input, parsed.tiles.data(), parsed.tiles.size() * sizeof(MapTile))) {
        error = "Map tile data is truncated.";
        return false;
    }

    parsed.objects.resize(parsed.header.object_count);
    if (!parsed.objects.empty() &&
        !ReadExact(input, parsed.objects.data(), parsed.objects.size() * sizeof(MapObject))) {
        error = "Map object data is truncated.";
        return false;
    }

    if (!ReadExact(input, &parsed.appendix, sizeof(parsed.appendix))) {
        error = "Map metadata appendix is missing or truncated.";
        return false;
    }

    out = std::move(parsed);
    error.clear();
    return true;
}

bool MapDocument::Save(const std::filesystem::path& path, std::string& error) const {
    if (width() == 0 || width() > kMaxWidth || height() == 0 || height() > kMaxHeight ||
        header.tileset_count > 64 || header.object_count != objects.size() ||
        tiles.size() != static_cast<std::size_t>(width()) * height()) {
        error = "Map document is inconsistent or exceeds the game limits.";
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Cannot create map: " + path.string();
        return false;
    }

    if (!WriteExact(output, &header, sizeof(header)) ||
        !WriteExact(output, tiles.data(), tiles.size() * sizeof(MapTile)) ||
        !WriteExact(output, objects.data(), objects.size() * sizeof(MapObject)) ||
        !WriteExact(output, &appendix, sizeof(appendix))) {
        error = "A write error occurred while saving the map.";
        return false;
    }

    error.clear();
    return true;
}

} // namespace hurrican::editor
