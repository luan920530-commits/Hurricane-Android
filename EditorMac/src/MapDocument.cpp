#include "MapDocument.hpp"

#include <algorithm>
#include <cstring>
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
        error = "No se puede abrir el mapa: " + path.string();
        return false;
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) < sizeof(MapHeader)) {
        error = "El archivo es demasiado pequeño para contener la cabecera del mapa.";
        return false;
    }
    const auto file_size = static_cast<std::uint64_t>(end);
    input.seekg(0, std::ios::beg);

    MapDocument parsed;
    if (!ReadExact(input, &parsed.header, sizeof(parsed.header))) {
        error = "No se pudo leer completa la cabecera del mapa de Hurrican.";
        return false;
    }

    if (parsed.header.width == 0 || parsed.header.width > kMaxWidth ||
        parsed.header.height == 0 || parsed.header.height > kMaxHeight) {
        error = "Las dimensiones del mapa no son válidas o superan los límites del juego.";
        return false;
    }
    if (parsed.header.tileset_count > 64 || parsed.header.object_count > kMaxObjects) {
        error = "La cabecera contiene una cantidad de tilesets u objetos no compatible.";
        return false;
    }

    const std::uint64_t expected = parsed.ExpectedFileSize();
    if (expected != file_size) {
        error = "El tamaño del mapa no coincide con la cabecera (se esperaban " + std::to_string(expected) +
                " bytes y se encontraron " + std::to_string(file_size) + ").";
        return false;
    }

    const std::uint64_t tile_count = static_cast<std::uint64_t>(parsed.width()) * parsed.height();
    if (tile_count > std::numeric_limits<std::size_t>::max() / sizeof(MapTile)) {
        error = "El mapa es demasiado grande para este equipo.";
        return false;
    }

    parsed.tiles.resize(static_cast<std::size_t>(tile_count));
    if (!parsed.tiles.empty() &&
        !ReadExact(input, parsed.tiles.data(), parsed.tiles.size() * sizeof(MapTile))) {
        error = "Los datos de tiles del mapa están incompletos.";
        return false;
    }

    parsed.objects.resize(parsed.header.object_count);
    if (!parsed.objects.empty() &&
        !ReadExact(input, parsed.objects.data(), parsed.objects.size() * sizeof(MapObject))) {
        error = "Los datos de objetos del mapa están incompletos.";
        return false;
    }

    if (!ReadExact(input, &parsed.appendix, sizeof(parsed.appendix))) {
        error = "Falta el bloque final de metadatos del mapa o está incompleto.";
        return false;
    }

    out = std::move(parsed);
    error.clear();
    return true;
}

bool MapDocument::CreateEmpty(std::uint32_t width, std::uint32_t height, MapDocument& out, std::string& error) {
    if (width == 0 || width > kMaxWidth || height == 0 || height > kMaxHeight) {
        error = "Las dimensiones del mapa nuevo superan los límites del juego.";
        return false;
    }
    MapDocument map;
    constexpr char signature[] = "Hurrican Level File V1.0 (c) 2002 Poke53280 - ";
    static_assert(sizeof(signature) - 1 == sizeof(map.header.signature), "Map signature size changed");
    std::memcpy(map.header.signature, signature, sizeof(map.header.signature));
    std::memcpy(map.header.description, "Nuevo nivel de Hurrican", sizeof("Nuevo nivel de Hurrican"));
    map.header.time_limit = 500;
    map.header.width = width;
    map.header.height = height;
    map.header.object_count = 1;
    map.header.scroll_background = 1;
    map.tiles.resize(static_cast<std::size_t>(width) * height);
    for (MapTile& tile : map.tiles) {
        tile.red = tile.green = tile.blue = tile.alpha = 255;
    }
    MapObject start{};
    start.id = 0;
    start.x = static_cast<std::int32_t>(std::min<std::uint32_t>(2, width - 1) * 20);
    start.y = static_cast<std::int32_t>(std::min<std::uint32_t>(2, height - 1) * 20);
    start.change_light = 1;
    map.objects.push_back(start);
    out = std::move(map);
    error.clear();
    return true;
}

bool MapDocument::Save(const std::filesystem::path& path, std::string& error) const {
    if (width() == 0 || width() > kMaxWidth || height() == 0 || height() > kMaxHeight ||
        header.tileset_count > 64 || header.object_count != objects.size() ||
        tiles.size() != static_cast<std::size_t>(width()) * height()) {
        error = "El documento del mapa es inconsistente o supera los límites del juego.";
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "No se puede crear el mapa: " + path.string();
        return false;
    }

    if (!WriteExact(output, &header, sizeof(header)) ||
        !WriteExact(output, tiles.data(), tiles.size() * sizeof(MapTile)) ||
        !WriteExact(output, objects.data(), objects.size() * sizeof(MapObject)) ||
        !WriteExact(output, &appendix, sizeof(appendix))) {
        error = "Ocurrió un error de escritura al guardar el mapa.";
        return false;
    }

    error.clear();
    return true;
}

} // namespace hurrican::editor
