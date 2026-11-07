#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hurrican::editor {

// Binary .map records shared by the original Windows editor and the game runtime.
// These explicit padding bytes reproduce the original 32-bit MSVC layout on Intel macOS.
#pragma pack(push, 1)
struct MapHeader {
    char signature[46];
    char description[100];
    char background_file[24];
    char parallax_a_file[24];
    char parallax_b_file[24];
    char cloud_file[24];
    std::uint8_t padding_before_time[2];
    std::uint32_t time_limit;
    std::uint8_t tileset_count;
    char tileset_names[64][16];
    std::uint8_t padding_before_dimensions[3];
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t object_count;
    std::uint8_t scroll_background;
    std::uint8_t padding_at_end[3];
};

struct MapTile {
    std::uint8_t tileset_back;
    std::uint8_t tileset_front;
    std::uint8_t art_back;
    std::uint8_t art_front;
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha;
    std::uint32_t block_flags;
};

struct MapObject {
    std::uint32_t id;
    std::int32_t x;
    std::int32_t y;
    std::uint8_t change_light;
    std::uint8_t skill;
    std::uint8_t padding[2];
    std::int32_t value1;
    std::int32_t value2;
};

struct MapAppendix {
    char songs[2][30];
    std::int32_t powerblock_id;
    char liquid_color_1[8];
    char liquid_color_2[8];
    std::uint8_t flashlight;
    std::uint8_t padding[3];
};
#pragma pack(pop)

static_assert(sizeof(MapHeader) == 1292, "Unexpected Hurrican .map header layout");
static_assert(sizeof(MapTile) == 12, "Unexpected Hurrican .map tile layout");
static_assert(sizeof(MapObject) == 24, "Unexpected Hurrican .map object layout");
static_assert(sizeof(MapAppendix) == 84, "Unexpected Hurrican .map appendix layout");

class MapDocument {
public:
    static constexpr std::uint32_t kMaxWidth = 1024;
    static constexpr std::uint32_t kMaxHeight = 1600;
    static constexpr std::uint32_t kMaxObjects = 1'000'000;

    MapHeader header{};
    std::vector<MapTile> tiles; // X-major, same order as the game/editor file format.
    std::vector<MapObject> objects;
    MapAppendix appendix{};

    std::uint32_t width() const noexcept { return header.width; }
    std::uint32_t height() const noexcept { return header.height; }
    MapTile& tile(std::uint32_t x, std::uint32_t y);
    const MapTile& tile(std::uint32_t x, std::uint32_t y) const;

    static bool Load(const std::filesystem::path& path, MapDocument& out, std::string& error);
    bool Save(const std::filesystem::path& path, std::string& error) const;
    std::uint64_t ExpectedFileSize() const noexcept;
};

} // namespace hurrican::editor
