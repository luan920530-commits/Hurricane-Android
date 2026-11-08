#include "MapDocument.hpp"
#include "FileDialogs.hpp"
#include "ObjectCatalog.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using hurrican::editor::MapDocument;
using hurrican::editor::MapObject;
using hurrican::editor::MapTile;
using hurrican::editor::ObjectCategory;
using hurrican::editor::ObjectDefinition;
using hurrican::editor::kObjectCatalog;

namespace {

struct AtlasTexture {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
};

struct EditorState {
    MapDocument map;
    std::vector<AtlasTexture> atlases;
    bool loaded = false;
    bool dirty = false;
    bool paint_mode = false;
    bool selection_mode = false;
    bool selecting = false;
    bool has_clipboard = false;
    int selection_x0 = 0;
    int selection_y0 = 0;
    int selection_x1 = 0;
    int selection_y1 = 0;
    std::uint32_t clipboard_width = 0;
    std::uint32_t clipboard_height = 0;
    std::vector<MapTile> tile_clipboard;
    char object_search[64] = "";
    int new_object_id = 1;
    int selected_object = -1;
    int drag_object = -1;
    int new_map_width = 64;
    int new_map_height = 40;
    char new_tileset_name[64] = "";
    int selected_x = 0;
    int selected_y = 0;
    int brush_layer = 0; // 0 = back layer, 1 = front layer
    int brush_tileset = 0;
    int brush_art = 0;
    float zoom = 0.5f;
    char map_path[1024] = "";
    char message[512] = "Type a .map path, then choose Open.";
};

void CopyToField(char* destination, std::size_t capacity, const char* source, std::size_t source_size) {
    std::size_t source_length = 0;
    while (source_length < source_size && source[source_length] != '\0')
        ++source_length;
    const std::size_t length = std::min(capacity - 1, source_length);
    std::memcpy(destination, source, length);
    destination[length] = '\0';
}

void CopyFromField(char* destination, std::size_t capacity, const char* source) {
    std::memset(destination, 0, capacity);
    std::strncpy(destination, source, capacity - 1);
}

bool EditFixedText(const char* label, char* field, std::size_t capacity) {
    char buffer[128];
    CopyToField(buffer, sizeof(buffer), field, capacity);
    if (!ImGui::InputText(label, buffer, sizeof(buffer)))
        return false;
    CopyFromField(field, capacity, buffer);
    return true;
}

const ObjectDefinition* FindObjectDefinition(std::uint32_t id) {
    for (const ObjectDefinition& definition : kObjectCatalog)
        if (definition.id == id)
            return &definition;
    return nullptr;
}

const char* ObjectCategoryLabel(ObjectCategory category) {
    switch (category) {
        case ObjectCategory::ItemOrEnemy: return "Items and enemies";
        case ObjectCategory::Boss: return "Bosses";
        case ObjectCategory::LevelObject: return "Level objects and triggers";
        case ObjectCategory::RuntimeOnly: return "Runtime-only objects";
    }
    return "Other";
}

bool ObjectMatchesFilter(const ObjectDefinition& definition, const char* filter) {
    if (filter == nullptr || filter[0] == '\0')
        return true;
    std::string needle(filter);
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string haystack = std::to_string(definition.id) + " " + definition.symbol + " " + definition.label;
    std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return haystack.find(needle) != std::string::npos;
}

bool DrawValueChoice(const char* label, int& value, const char* const* choices, std::size_t count) {
    char preview[96];
    if (value >= 0 && static_cast<std::size_t>(value) < count)
        std::snprintf(preview, sizeof(preview), "%d — %s", value, choices[value]);
    else
        std::snprintf(preview, sizeof(preview), "%d — custom value", value);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (std::size_t i = 0; i < count; ++i) {
            char item[96];
            std::snprintf(item, sizeof(item), "%zu — %s", i, choices[i]);
            if (ImGui::Selectable(item, value == static_cast<int>(i))) {
                value = static_cast<int>(i);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool DrawTriggerFields(MapObject& object) {
    bool changed = false;
    int value1 = object.value1;
    int value2 = object.value2;
    static const char* const particles[] = {
        "Rain", "Snow", "Smoke", "Sparks", "Bubbles", "Rising smoke",
        "Intermittent smoke", "Waterfall splash", "Ceiling drops", "Lightning and thunder",
        "Scroll speed", "Fog", "Quiet sparks", "Dark lightning", "Spider debris",
        "Leaves", "Dust", "Maggots", "Lava sparks", "Updraft"
    };
    static const char* const flame_directions[] = {
        "Flame direction 1", "Flame direction 2", "Flame direction 3", "Flame direction 4"
    };
    static const char* const fade_actions[] = {
        "Fade stage music out", "Fade stage music in", "Fade sound effect out", "Fade sound effect in"
    };
    static const char* const sound_modes[] = {
        "Loop with distance volume", "Play once near player", "Loop near player"
    };
    static const char* const flare_colors[] = {
        "White", "Red", "Green", "Blue", "Yellow", "Magenta", "Cyan"
    };
    static const char* const spawn_directions[] = {"Left", "Right"};
    static const char* const eye_modes[] = {"Track the player", "Look around slowly"};
    static const char* const tube_forms[] = {
        "Intact", "Broken", "Broken lower half", "Broken upper half", "Intact with mutant", "Mutant jumps out"
    };
    switch (object.id) {
        case 140: // Particle spawner
            changed |= DrawValueChoice("Particle type", value1, particles, std::size(particles));
            if (ImGui::InputInt("Spawn interval", &value2)) changed = true;
            break;
        case 141: { // Elevator
            bool elevator_mode = value1 == 0;
            if (ImGui::Checkbox("Elevator level mode", &elevator_mode)) {
                value1 = elevator_mode ? 0 : 1;
                changed = true;
            }
            ImGui::TextDisabled("Value 2 sets the lower stop position in elevator mode.");
            if (ImGui::InputInt("Lower stop Y (pixels)", &value2)) changed = true;
            break;
        }
        case 143: // Flame trap
            changed |= DrawValueChoice("Flame direction", value1, flame_directions, std::size(flame_directions));
            if (ImGui::InputInt("Pause length", &value2)) changed = true;
            break;
        case 144: // Fade music / sound trigger
            changed |= DrawValueChoice("Fade action", value1, fade_actions, std::size(fade_actions));
            if (ImGui::InputInt("Sound ID offset (for sound fades)", &value2)) changed = true;
            break;
        case 145: // End level
            if (ImGui::InputInt("Destination stage ID", &value1)) changed = true;
            if (ImGui::InputInt("Player exit action ID", &value2)) changed = true;
            break;
        case 146: // Moving platform
            if (ImGui::InputInt("Horizontal speed", &value1)) changed = true;
            if (ImGui::InputInt("Vertical speed", &value2)) changed = true;
            break;
        case 156: // Glubschi
        case 157: // Ceiling Glubschi
            changed |= DrawValueChoice("Gaze behavior", value1, eye_modes, std::size(eye_modes));
            if (ImGui::InputInt("Secondary parameter", &value2)) changed = true;
            break;
        case 159: // Timed lift
            if (ImGui::InputInt("Countdown speed", &value1)) changed = true;
            if (ImGui::InputInt("Starting Y position", &value2)) changed = true;
            break;
        case 163: // Switch
            if (ImGui::InputInt("Switch state value", &value1)) changed = true;
            if (ImGui::InputInt("Activation group ID", &value2)) changed = true;
            ImGui::TextDisabled("Objects sharing this group ID are activated together.");
            break;
        case 165: // Sound trigger
            if (ImGui::InputInt("Sound file ID", &value1)) changed = true;
            changed |= DrawValueChoice("Playback mode", value2, sound_modes, std::size(sound_modes));
            break;
        case 169: // Tutorial text
            if (ImGui::InputInt("Tutorial text ID", &value1)) changed = true;
            if (ImGui::InputInt("Tutorial parameter", &value2)) changed = true;
            break;
        case 170: // Light flare
            changed |= DrawValueChoice("Flare color", value2, flare_colors, std::size(flare_colors));
            if (ImGui::InputInt("Legacy value 1", &value1)) changed = true;
            break;
        case 176: { // Tube
            changed |= DrawValueChoice("Tube appearance", value1, tube_forms, std::size(tube_forms));
            bool contains_liquid = value2 != 0;
            if (ImGui::Checkbox("Contains liquid", &contains_liquid)) {
                value2 = contains_liquid ? 1 : 0;
                changed = true;
            }
            break;
        }
        case 174: // Barrel spawner
            changed |= DrawValueChoice("Spawn direction", value1, spawn_directions, std::size(spawn_directions));
            if (ImGui::InputInt("Spawn interval", &value2)) changed = true;
            break;
        default:
            if (ImGui::InputInt("Object value 1", &value1)) changed = true;
            if (ImGui::InputInt("Object value 2", &value2)) changed = true;
            break;
    }
    if (changed) {
        object.value1 = value1;
        object.value2 = value2;
    }
    return changed;
}

bool DrawObjectTypeCombo(const char* label, std::uint32_t& object_id, char* filter) {
    const ObjectDefinition* current = FindObjectDefinition(object_id);
    char preview[128];
    if (current)
        std::snprintf(preview, sizeof(preview), "%u — %s", current->id, current->label);
    else
        std::snprintf(preview, sizeof(preview), "%u — Unknown object", object_id);
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        char filter_label[64];
        std::snprintf(filter_label, sizeof(filter_label), "##%s-filter", label);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint(filter_label, "Search by name or ID", filter, 64);
        ObjectCategory previous = ObjectCategory::RuntimeOnly;
        bool first = true;
        for (const ObjectDefinition& definition : kObjectCatalog) {
            if (!ObjectMatchesFilter(definition, filter))
                continue;
            if (first || definition.category != previous) {
                if (!first) ImGui::Separator();
                ImGui::TextDisabled("%s", ObjectCategoryLabel(definition.category));
                previous = definition.category;
                first = false;
            }
            char item[128];
            std::snprintf(item, sizeof(item), "%u — %s", definition.id, definition.label);
            if (ImGui::Selectable(item, object_id == definition.id)) {
                object_id = definition.id;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

void ReleaseAtlases(EditorState& state) {
    for (const AtlasTexture& atlas : state.atlases) {
        if (atlas.texture != 0)
            glDeleteTextures(1, &atlas.texture);
    }
    state.atlases.clear();
}

std::filesystem::path FindTextureDirectory(const std::filesystem::path& map_path) {
    std::filesystem::path directory = map_path.parent_path();
    for (int depth = 0; depth < 5 && !directory.empty(); ++depth) {
        const std::filesystem::path candidate = directory / "textures";
        if (std::filesystem::is_directory(candidate))
            return candidate;
        directory = directory.parent_path();
    }
    return {};
}

void LoadAtlases(EditorState& state) {
    ReleaseAtlases(state);
    const std::filesystem::path texture_directory = FindTextureDirectory(state.map_path);
    const std::filesystem::path map_directory = std::filesystem::path(state.map_path).parent_path();
    state.atlases.resize(state.map.header.tileset_count);

    for (std::size_t i = 0; i < state.atlases.size(); ++i) {
        const char* name = state.map.header.tileset_names[i];
        std::size_t name_length = 0;
        while (name_length < sizeof(state.map.header.tileset_names[i]) && name[name_length] != '\0')
            ++name_length;
        if (name_length == 0)
            continue;

        const std::filesystem::path filename(std::string(name, name_length));
        std::filesystem::path image_path = map_directory / filename;
        if (!std::filesystem::is_regular_file(image_path) && !texture_directory.empty())
            image_path = texture_directory / filename;
        SDL_Surface* source = IMG_Load(image_path.string().c_str());
        if (source == nullptr)
            continue;
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(source);
        if (rgba == nullptr)
            continue;

        AtlasTexture& atlas = state.atlases[i];
        atlas.width = rgba->w;
        atlas.height = rgba->h;
        glGenTextures(1, &atlas.texture);
        glBindTexture(GL_TEXTURE_2D, atlas.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
        SDL_FreeSurface(rgba);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool LoadMap(EditorState& state) {
    std::string error;
    MapDocument loaded;
    if (!MapDocument::Load(state.map_path, loaded, error)) {
        std::snprintf(state.message, sizeof(state.message), "Open failed: %s", error.c_str());
        return false;
    }
    state.map = std::move(loaded);
    state.loaded = true;
    LoadAtlases(state);
    state.dirty = false;
    state.selected_x = 0;
    state.selected_y = 0;
    state.selected_object = -1;
    state.drag_object = -1;
    state.selection_x0 = state.selection_y0 = 0;
    state.selection_x1 = state.selection_y1 = 0;
    state.selecting = state.selection_mode = false;
    std::snprintf(state.message, sizeof(state.message), "Loaded %s (%u x %u, %zu objects)",
                  state.map_path, state.map.width(), state.map.height(), state.map.objects.size());
    return true;
}

bool SaveMap(EditorState& state) {
    if (!state.loaded || state.map_path[0] == '\0')
        return false;
    state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
    std::string error;
    if (!state.map.Save(state.map_path, error)) {
        std::snprintf(state.message, sizeof(state.message), "Save failed: %s", error.c_str());
        return false;
    }
    state.dirty = false;
    std::snprintf(state.message, sizeof(state.message), "Saved %s", state.map_path);
    return true;
}

void CreateNewMap(EditorState& state) {
    MapDocument created;
    std::string error;
    if (!MapDocument::CreateEmpty(static_cast<std::uint32_t>(state.new_map_width),
                                  static_cast<std::uint32_t>(state.new_map_height), created, error)) {
        std::snprintf(state.message, sizeof(state.message), "New map failed: %s", error.c_str());
        return;
    }
    ReleaseAtlases(state);
    state.map = std::move(created);
    state.loaded = state.dirty = true;
    state.selected_x = state.selected_y = 0;
    state.selected_object = 0;
    state.drag_object = -1;
    state.selection_x0 = state.selection_y0 = state.selection_x1 = state.selection_y1 = 0;
    state.selecting = state.selection_mode = false;
    state.brush_tileset = 0;
    state.brush_art = 0;
    state.map_path[0] = '\0';
    std::snprintf(state.message, sizeof(state.message), "Blank map created (%u x %u); use Save As.",
                  state.map.width(), state.map.height());
}

void CopySelection(EditorState& state) {
    const int x0 = std::min(state.selection_x0, state.selection_x1);
    const int y0 = std::min(state.selection_y0, state.selection_y1);
    const int x1 = std::max(state.selection_x0, state.selection_x1);
    const int y1 = std::max(state.selection_y0, state.selection_y1);
    state.clipboard_width = static_cast<std::uint32_t>(x1 - x0 + 1);
    state.clipboard_height = static_cast<std::uint32_t>(y1 - y0 + 1);
    state.tile_clipboard.clear();
    state.tile_clipboard.reserve(static_cast<std::size_t>(state.clipboard_width) * state.clipboard_height);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            state.tile_clipboard.push_back(state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)));
    state.has_clipboard = !state.tile_clipboard.empty();
    std::snprintf(state.message, sizeof(state.message), "Copied tile region (%u x %u).",
                  state.clipboard_width, state.clipboard_height);
}

void PasteClipboard(EditorState& state) {
    if (!state.loaded || !state.has_clipboard || state.tile_clipboard.empty()) return;
    const int base_x = state.selected_x;
    const int base_y = state.selected_y;
    bool changed = false;
    for (std::uint32_t y = 0; y < state.clipboard_height; ++y) {
        for (std::uint32_t x = 0; x < state.clipboard_width; ++x) {
            const int target_x = base_x + static_cast<int>(x);
            const int target_y = base_y + static_cast<int>(y);
            if (target_x < 0 || target_y < 0 || target_x >= static_cast<int>(state.map.width()) ||
                target_y >= static_cast<int>(state.map.height())) continue;
            state.map.tile(static_cast<std::uint32_t>(target_x), static_cast<std::uint32_t>(target_y)) =
                state.tile_clipboard[static_cast<std::size_t>(y) * state.clipboard_width + x];
            changed = true;
        }
    }
    if (changed) {
        state.selection_x0 = base_x;
        state.selection_y0 = base_y;
        state.selection_x1 = std::min(static_cast<int>(state.map.width()) - 1,
                                      base_x + static_cast<int>(state.clipboard_width) - 1);
        state.selection_y1 = std::min(static_cast<int>(state.map.height()) - 1,
                                      base_y + static_cast<int>(state.clipboard_height) - 1);
        state.selection_mode = true;
        state.dirty = true;
    }
}

void FillSelectionWithBrush(EditorState& state) {
    const int x0 = std::min(state.selection_x0, state.selection_x1);
    const int y0 = std::min(state.selection_y0, state.selection_y1);
    const int x1 = std::max(state.selection_x0, state.selection_x1);
    const int y1 = std::max(state.selection_y0, state.selection_y1);
    const std::uint8_t set = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
    const std::uint8_t art = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
    bool changed = false;
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        MapTile& tile = state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
        std::uint8_t& tile_set = state.brush_layer == 0 ? tile.tileset_back : tile.tileset_front;
        std::uint8_t& tile_art = state.brush_layer == 0 ? tile.art_back : tile.art_front;
        if (tile_set != set || tile_art != art) {
            tile_set = set;
            tile_art = art;
            changed = true;
        }
    }
    state.dirty = state.dirty || changed;
}

ImU32 TileColor(const MapTile& tile) {
    ImVec4 color;
    if (tile.tileset_back == 0 && tile.tileset_front == 0) {
        color = ImGui::ColorConvertU32ToFloat4(IM_COL32(29, 34, 43, 255));
    } else {
        const unsigned int seed = static_cast<unsigned int>(tile.tileset_back) * 71u +
                                  static_cast<unsigned int>(tile.art_back) * 17u +
                                  static_cast<unsigned int>(tile.tileset_front) * 31u + tile.art_front;
        color = ImColor::HSV(std::fmod(seed * 0.071f, 1.0f), 0.52f, 0.78f);
        if (tile.block_flags != 0) {
            color.x = std::min(1.0f, color.x + 0.20f);
            color.y *= 0.70f;
            color.z *= 0.70f;
        }
    }
    color.x *= tile.red / 255.0f;
    color.y *= tile.green / 255.0f;
    color.z *= tile.blue / 255.0f;
    color.w = tile.alpha / 255.0f;
    return ImGui::ColorConvertFloat4ToU32(color);
}

void DrawMapCanvas(EditorState& state) {
    ImGui::Begin("Level map");
    ImGui::TextDisabled("Uses game texture atlases when available; missing assets are shown schematically.");
    if (ImGui::Checkbox("Paint brush", &state.paint_mode) && state.paint_mode) {
        state.selection_mode = false;
        state.selecting = false;
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Zoom", &state.zoom, 0.20f, 2.0f, "%.2fx");

    if (!state.loaded) {
        ImGui::TextWrapped("Open a Hurrican .map file to inspect and edit it.");
        ImGui::End();
        return;
    }

    const float cell = 20.0f * state.zoom;
    ImGui::BeginChild("MapCanvas", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent(cell * state.map.width(), cell * state.map.height());
    ImGui::InvisibleButton("map-content", extent, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    const ImVec2 clip_max = ImGui::GetWindowDrawList()->GetClipRectMax();

    const int x0 = std::clamp(static_cast<int>((clip_min.x - origin.x) / cell), 0,
                              static_cast<int>(state.map.width()));
    const int y0 = std::clamp(static_cast<int>((clip_min.y - origin.y) / cell), 0,
                              static_cast<int>(state.map.height()));
    const int x1 = std::clamp(static_cast<int>(std::ceil((clip_max.x - origin.x) / cell)), 0,
                              static_cast<int>(state.map.width()));
    const int y1 = std::clamp(static_cast<int>(std::ceil((clip_max.y - origin.y) / cell)), 0,
                              static_cast<int>(state.map.height()));

    for (int x = x0; x < x1; ++x) {
        for (int y = y0; y < y1; ++y) {
            const MapTile& tile = state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
            const ImVec2 p0(origin.x + x * cell, origin.y + y * cell);
            const ImVec2 p1(p0.x + cell, p0.y + cell);
            draw->AddRectFilled(p0, p1, TileColor(tile));
            const auto draw_layer = [&](std::uint8_t tileset, std::uint8_t art) {
                if (tileset == 0 || tileset > state.atlases.size() || art >= 144)
                    return;
                const AtlasTexture& atlas = state.atlases[tileset - 1];
                if (atlas.texture == 0 || atlas.width < 240 || atlas.height < 240)
                    return;
                constexpr float tile_size = 20.0f;
                const float u0 = (art % 12) * tile_size / atlas.width;
                const float v0 = (art / 12) * tile_size / atlas.height;
                const float u1 = u0 + tile_size / atlas.width;
                const float v1 = v0 + tile_size / atlas.height;
                const ImU32 tint = IM_COL32(tile.red, tile.green, tile.blue, tile.alpha);
                draw->AddImage(static_cast<ImTextureID>(atlas.texture), p0, p1,
                               ImVec2(u0, v0), ImVec2(u1, v1), tint);
            };
            draw_layer(tile.tileset_back, tile.art_back);
            draw_layer(tile.tileset_front, tile.art_front);
            if (cell >= 18.0f)
                draw->AddRect(p0, p1, IM_COL32(12, 15, 20, 90));
            if (x == state.selected_x && y == state.selected_y)
                draw->AddRect(p0, p1, IM_COL32(255, 224, 80, 255), 0.0f, 0, 2.0f);
        }
    }

    if (state.selection_mode) {
        const int sx0 = std::min(state.selection_x0, state.selection_x1);
        const int sy0 = std::min(state.selection_y0, state.selection_y1);
        const int sx1 = std::max(state.selection_x0, state.selection_x1);
        const int sy1 = std::max(state.selection_y0, state.selection_y1);
        const ImVec2 p0(origin.x + sx0 * cell, origin.y + sy0 * cell);
        const ImVec2 p1(origin.x + (sx1 + 1) * cell, origin.y + (sy1 + 1) * cell);
        draw->AddRectFilled(p0, p1, IM_COL32(60, 170, 255, 35));
        draw->AddRect(p0, p1, IM_COL32(70, 190, 255, 240), 0.0f, 0, 2.0f);
    }
    for (std::size_t i = 0; i < state.map.objects.size(); ++i) {
        const MapObject& object = state.map.objects[i];
        const ImVec2 center(origin.x + (object.x / 20.0f + 0.5f) * cell,
                            origin.y + (object.y / 20.0f + 0.5f) * cell);
        if (center.x < clip_min.x || center.y < clip_min.y || center.x > clip_max.x || center.y > clip_max.y)
            continue;
        const float radius = std::max(4.0f, cell * 0.32f);
        draw->AddCircleFilled(center, radius,
            object.id == 0 ? IM_COL32(80, 240, 130, 255) : IM_COL32(255, 170, 60, 255));
        if (static_cast<int>(i) == state.selected_object)
            draw->AddCircle(center, radius + 2.0f, IM_COL32_WHITE, 0, 2.0f);
        if (cell >= 24.0f) {
            const std::string label = object.id == 0 ? "P" : std::to_string(object.id);
            draw->AddText(ImVec2(center.x + radius, center.y - radius), IM_COL32_WHITE, label.c_str());
        }
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int x = static_cast<int>((mouse.x - origin.x) / cell);
        const int y = static_cast<int>((mouse.y - origin.y) / cell);
        if (x >= 0 && y >= 0 && x < static_cast<int>(state.map.width()) &&
            y < static_cast<int>(state.map.height())) {
            state.selected_x = x;
            state.selected_y = y;
            if (state.selection_mode) {
                state.selection_x0 = state.selection_x1 = x;
                state.selection_y0 = state.selection_y1 = y;
                state.selecting = true;
                state.selected_object = -1;
                state.drag_object = -1;
            } else if (state.paint_mode) {
                MapTile& tile = state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                if (state.brush_layer == 0) {
                    tile.tileset_back = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
                    tile.art_back = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
                } else {
                    tile.tileset_front = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
                    tile.art_front = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
                }
                state.dirty = true;
            } else {
                state.selected_object = -1;
                state.drag_object = -1;
                const float hit_radius = std::max(10.0f, cell * 0.55f);
                float nearest = hit_radius;
                for (std::size_t i = 0; i < state.map.objects.size(); ++i) {
                    const MapObject& object = state.map.objects[i];
                    const ImVec2 center(origin.x + (object.x / 20.0f + 0.5f) * cell,
                                        origin.y + (object.y / 20.0f + 0.5f) * cell);
                    const float distance = std::hypot(mouse.x - center.x, mouse.y - center.y);
                    if (distance <= nearest) {
                        nearest = distance;
                        state.selected_object = static_cast<int>(i);
                    }
                }
                state.drag_object = state.selected_object;
            }
        }
    }
    if (state.selection_mode && state.selecting && hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        state.selection_x1 = std::clamp(static_cast<int>((mouse.x - origin.x) / cell), 0,
                                        static_cast<int>(state.map.width()) - 1);
        state.selection_y1 = std::clamp(static_cast<int>((mouse.y - origin.y) / cell), 0,
                                        static_cast<int>(state.map.height()) - 1);
    }
    if (state.paint_mode && hovered && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int x = static_cast<int>((mouse.x - origin.x) / cell);
        const int y = static_cast<int>((mouse.y - origin.y) / cell);
        if (x >= 0 && y >= 0 && x < static_cast<int>(state.map.width()) && y < static_cast<int>(state.map.height())) {
            MapTile& tile = state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
            const std::uint8_t set = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
            const std::uint8_t art = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
            std::uint8_t& tile_set = state.brush_layer == 0 ? tile.tileset_back : tile.tileset_front;
            std::uint8_t& tile_art = state.brush_layer == 0 ? tile.art_back : tile.art_front;
            if (tile_set != set || tile_art != art) {
                tile_set = set;
                tile_art = art;
                state.dirty = true;
            }
        }
    }
    if (state.drag_object >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hovered) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int x = std::clamp(static_cast<int>((mouse.x - origin.x) / cell), 0, static_cast<int>(state.map.width()) - 1);
        const int y = std::clamp(static_cast<int>((mouse.y - origin.y) / cell), 0, static_cast<int>(state.map.height()) - 1);
        MapObject& object = state.map.objects[static_cast<std::size_t>(state.drag_object)];
        const std::int32_t new_x = x * 20, new_y = y * 20;
        if (object.x != new_x || object.y != new_y) {
            object.x = new_x;
            object.y = new_y;
            state.dirty = true;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        state.drag_object = -1;
        state.selecting = false;
    }
    ImGui::EndChild();
    ImGui::End();
}

void DrawInspector(EditorState& state) {
    ImGui::Begin("Level properties and tools");
    ImGui::InputText("Map path", state.map_path, sizeof(state.map_path));
    if (ImGui::Button("Open…")) {
        if (state.dirty)
            std::snprintf(state.message, sizeof(state.message), "Save the current map before opening another one.");
        else if (hurrican::editor::ChooseMapToOpen(state.map_path, sizeof(state.map_path)))
            LoadMap(state);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.loaded || state.map_path[0] == '\0');
    if (ImGui::Button("Save")) SaveMap(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.loaded);
    if (ImGui::Button("Save As…")) {
        if (hurrican::editor::ChooseMapToSave(state.map_path, sizeof(state.map_path)) && SaveMap(state))
            LoadAtlases(state);
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextWrapped("%s%s", state.message, state.dirty ? "  [modified]" : "");
    if (!state.loaded) {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Level header");
    ImGui::Text("Dimensions: %u x %u tiles", state.map.width(), state.map.height());
    ImGui::Text("Tilesets: %u    Objects: %zu", state.map.header.tileset_count, state.map.objects.size());
    char description[101];
    CopyToField(description, sizeof(description), state.map.header.description,
                sizeof(state.map.header.description));
    if (ImGui::InputText("Description", description, sizeof(description))) {
        CopyFromField(state.map.header.description, sizeof(state.map.header.description), description);
        state.dirty = true;
    }
    int time_limit = static_cast<int>(state.map.header.time_limit);
    if (ImGui::InputInt("Time limit", &time_limit)) {
        state.map.header.time_limit = static_cast<std::uint32_t>(std::max(0, time_limit));
        state.dirty = true;
    }
    ImGui::Text("Selected tile: %d, %d", state.selected_x, state.selected_y);

    ImGui::SeparatorText("Background and level assets");
    bool header_changed = false;
    header_changed |= EditFixedText("Background image", state.map.header.background_file,
                                    sizeof(state.map.header.background_file));
    header_changed |= EditFixedText("Parallax A", state.map.header.parallax_a_file,
                                    sizeof(state.map.header.parallax_a_file));
    header_changed |= EditFixedText("Parallax B", state.map.header.parallax_b_file,
                                    sizeof(state.map.header.parallax_b_file));
    header_changed |= EditFixedText("Cloud layer", state.map.header.cloud_file,
                                    sizeof(state.map.header.cloud_file));
    bool scroll_background = state.map.header.scroll_background != 0;
    if (ImGui::Checkbox("Scroll background", &scroll_background)) {
        state.map.header.scroll_background = scroll_background ? 1 : 0;
        header_changed = true;
    }
    if (header_changed) state.dirty = true;

    ImGui::SeparatorText("Tileset files");
    bool tilesets_changed = false;
    for (int i = 0; i < state.map.header.tileset_count; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "Tileset %d", i + 1);
        if (EditFixedText(label, state.map.header.tileset_names[i], sizeof(state.map.header.tileset_names[i]))) {
            tilesets_changed = true;
            state.dirty = true;
        }
    }
    if (state.map.header.tileset_count < 64) {
        ImGui::InputText("New tileset PNG", state.new_tileset_name, sizeof(state.new_tileset_name));
        if (ImGui::Button("Add tileset") && state.new_tileset_name[0] != '\0') {
            CopyFromField(state.map.header.tileset_names[state.map.header.tileset_count],
                          sizeof(state.map.header.tileset_names[0]), state.new_tileset_name);
            ++state.map.header.tileset_count;
            state.brush_tileset = 1;
            state.new_tileset_name[0] = '\0';
            tilesets_changed = true;
            state.dirty = true;
        }
    }
    if (tilesets_changed) LoadAtlases(state);

    ImGui::SeparatorText("Music, liquids and extras");
    bool appendix_changed = false;
    appendix_changed |= EditFixedText("Stage music (.it)", state.map.appendix.songs[0], sizeof(state.map.appendix.songs[0]));
    appendix_changed |= EditFixedText("Boss music (.it)", state.map.appendix.songs[1], sizeof(state.map.appendix.songs[1]));
    int powerblock = state.map.appendix.powerblock_id;
    if (ImGui::InputInt("Powerblock ID", &powerblock)) {
        state.map.appendix.powerblock_id = powerblock;
        appendix_changed = true;
    }
    appendix_changed |= EditFixedText("Liquid color 1", state.map.appendix.liquid_color_1, sizeof(state.map.appendix.liquid_color_1));
    appendix_changed |= EditFixedText("Liquid color 2", state.map.appendix.liquid_color_2, sizeof(state.map.appendix.liquid_color_2));
    bool flashlight = state.map.appendix.flashlight != 0;
    if (ImGui::Checkbox("Flashlight effect", &flashlight)) {
        state.map.appendix.flashlight = flashlight ? 1 : 0;
        appendix_changed = true;
    }
    if (appendix_changed) state.dirty = true;

    ImGui::SeparatorText("Tile brush / selected tile");
    ImGui::Combo("Layer", &state.brush_layer, "Background\0Foreground\0");
    if (state.map.header.tileset_count == 0) {
        state.brush_tileset = 0;
        ImGui::TextDisabled("No tileset yet; add a PNG above before painting artwork.");
    } else {
        state.brush_tileset = std::clamp(state.brush_tileset, 1, static_cast<int>(state.map.header.tileset_count));
        char selected_tileset[17];
        CopyToField(selected_tileset, sizeof(selected_tileset),
                    state.map.header.tileset_names[state.brush_tileset - 1], 16);
        if (ImGui::BeginCombo("Tileset", selected_tileset)) {
            for (int i = 0; i < state.map.header.tileset_count; ++i) {
                char name[17];
                CopyToField(name, sizeof(name), state.map.header.tileset_names[i], 16);
                const bool selected = state.brush_tileset == i + 1;
                if (ImGui::Selectable(name, selected)) state.brush_tileset = i + 1;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    state.brush_art = std::clamp(state.brush_art, 0, 143);
    ImGui::InputInt("Tile art index", &state.brush_art);
    state.brush_art = std::clamp(state.brush_art, 0, 143);
    if (state.brush_tileset > 0 && state.brush_tileset <= static_cast<int>(state.atlases.size())) {
        const AtlasTexture& palette = state.atlases[state.brush_tileset - 1];
        if (palette.texture != 0) {
            ImGui::TextDisabled("Click a cell in the tileset to choose the brush tile.");
            ImGui::Image(static_cast<ImTextureID>(palette.texture), ImVec2(256.0f, 256.0f));
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 top_left = ImGui::GetItemRectMin();
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                const int atlas_x = static_cast<int>((mouse.x - top_left.x) * palette.width / 256.0f);
                const int atlas_y = static_cast<int>((mouse.y - top_left.y) * palette.height / 256.0f);
                const int col = atlas_x / 20;
                const int row = atlas_y / 20;
                if (col >= 0 && col < 12 && row >= 0 && row < 12)
                    state.brush_art = row * 12 + col;
            }
        }
    }
    if (state.selected_x >= 0 && state.selected_y >= 0) {
        MapTile& tile = state.map.tile(static_cast<std::uint32_t>(state.selected_x),
                                       static_cast<std::uint32_t>(state.selected_y));
        int tileset = state.brush_layer == 0 ? tile.tileset_back : tile.tileset_front;
        int art = state.brush_layer == 0 ? tile.art_back : tile.art_front;
        ImGui::Text("Current layer: tileset %d, art %d", tileset, art);
        float color[4] = {tile.red / 255.0f, tile.green / 255.0f, tile.blue / 255.0f, tile.alpha / 255.0f};
        if (ImGui::ColorEdit4("Tile tint", color)) {
            tile.red = static_cast<std::uint8_t>(std::lround(std::clamp(color[0], 0.0f, 1.0f) * 255.0f));
            tile.green = static_cast<std::uint8_t>(std::lround(std::clamp(color[1], 0.0f, 1.0f) * 255.0f));
            tile.blue = static_cast<std::uint8_t>(std::lround(std::clamp(color[2], 0.0f, 1.0f) * 255.0f));
            tile.alpha = static_cast<std::uint8_t>(std::lround(std::clamp(color[3], 0.0f, 1.0f) * 255.0f));
            state.dirty = true;
        }
        std::uint32_t flags = tile.block_flags;
        if (ImGui::InputScalar("Block flags (hex value)", ImGuiDataType_U32, &flags, nullptr, nullptr, "%08X")) {
            tile.block_flags = flags;
            state.dirty = true;
        }
        struct BlockFlag { const char* label; std::uint32_t value; };
        static constexpr BlockFlag block_flags[] = {
            {"Wall", 0x000001}, {"Enemy wall", 0x000002}, {"Platform", 0x000004},
            {"Lighting", 0x000008}, {"Hide player/object", 0x000010}, {"Animated back", 0x000020},
            {"Animated front", 0x000040}, {"Water", 0x000080}, {"Damage", 0x000100},
            {"Belt left", 0x000200}, {"Belt right", 0x000400}, {"Turn point", 0x000800},
            {"Destructible", 0x001000}, {"Move left", 0x002000}, {"Overlay lighting", 0x004000},
            {"Swamp", 0x008000}, {"Ice", 0x010000}, {"Move vertically", 0x020000},
            {"Waterfall", 0x040000}, {"Move right", 0x080000}, {"Slope left", 0x200000},
            {"Slope right", 0x400000}, {"Liquid", 0x800000},
        };
        if (ImGui::BeginTable("block-flag-grid", 2)) {
            for (std::size_t i = 0; i < std::size(block_flags); i += 2) {
                ImGui::TableNextRow();
                for (int col = 0; col < 2 && i + col < std::size(block_flags); ++col) {
                    ImGui::TableSetColumnIndex(col);
                    const BlockFlag& flag = block_flags[i + col];
                    bool enabled = (tile.block_flags & flag.value) != 0;
                    ImGui::PushID(static_cast<int>(i + col));
                    if (ImGui::Checkbox(flag.label, &enabled)) {
                        if (enabled) tile.block_flags |= flag.value;
                        else tile.block_flags &= ~flag.value;
                        state.dirty = true;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Tile indices are edited directly; texture names are preserved from the original map.");
    }

    ImGui::SeparatorText("Rectangular selection and clipboard");
    if (ImGui::Checkbox("Selection tool", &state.selection_mode) && state.selection_mode) {
        state.paint_mode = false;
        state.selecting = false;
    }
    ImGui::Text("Selection: %d x %d tiles",
                std::abs(state.selection_x1 - state.selection_x0) + 1,
                std::abs(state.selection_y1 - state.selection_y0) + 1);
    ImGui::BeginDisabled(!state.selection_mode);
    if (ImGui::Button("Copy selection")) CopySelection(state);
    ImGui::SameLine();
    if (ImGui::Button("Fill with brush")) FillSelectionWithBrush(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.has_clipboard);
    if (ImGui::Button("Paste at selected tile")) PasteClipboard(state);
    ImGui::EndDisabled();

    ImGui::SeparatorText("Objects");
    if (ImGui::Button("Add player start")) {
        MapObject object{};
        object.id = 0;
        object.x = state.selected_x * 20;
        object.y = state.selected_y * 20;
        object.change_light = 1;
        state.map.objects.push_back(object);
        state.selected_object = static_cast<int>(state.map.objects.size()) - 1;
        state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
        state.dirty = true;
    }
    std::uint32_t new_object_id = static_cast<std::uint32_t>(std::max(0, state.new_object_id));
    if (DrawObjectTypeCombo("New object type", new_object_id, state.object_search))
        state.new_object_id = static_cast<int>(new_object_id);
    ImGui::InputInt("Custom new object ID", &state.new_object_id);
    state.new_object_id = std::clamp(state.new_object_id, 0, 65535);
    if (ImGui::Button("Add object at selection")) {
        MapObject object{};
        object.id = static_cast<std::uint32_t>(state.new_object_id);
        object.x = state.selected_x * 20;
        object.y = state.selected_y * 20;
        object.change_light = 1;
        state.map.objects.push_back(object);
        state.selected_object = static_cast<int>(state.map.objects.size()) - 1;
        state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
        state.dirty = true;
    }
    if (state.selected_object >= 0 && state.selected_object < static_cast<int>(state.map.objects.size())) {
        MapObject& selected = state.map.objects[static_cast<std::size_t>(state.selected_object)];
        ImGui::SeparatorText("Selected object properties");
        if (DrawObjectTypeCombo("Object type", selected.id, state.object_search)) state.dirty = true;
        int id = static_cast<int>(selected.id);
        if (ImGui::InputInt("Custom object ID", &id)) {
            selected.id = static_cast<std::uint32_t>(std::max(0, id));
            state.dirty = true;
        }
        int x = selected.x;
        if (ImGui::InputInt("Object X (pixels)", &x)) { selected.x = x; state.dirty = true; }
        int y = selected.y;
        if (ImGui::InputInt("Object Y (pixels)", &y)) { selected.y = y; state.dirty = true; }
        bool receives_light = selected.change_light != 0;
        if (ImGui::Checkbox("Receives light", &receives_light)) {
            selected.change_light = receives_light ? 1 : 0;
            state.dirty = true;
        }
        int skill = selected.skill;
        if (ImGui::InputInt("Skill / difficulty", &skill)) {
            selected.skill = static_cast<std::uint8_t>(std::clamp(skill, 0, 255));
            state.dirty = true;
        }
        ImGui::SeparatorText("Type-specific parameters");
        if (DrawTriggerFields(selected)) state.dirty = true;
        if (ImGui::Button("Delete selected object")) {
            state.map.objects.erase(state.map.objects.begin() + state.selected_object);
            state.selected_object = -1;
            state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
            state.dirty = true;
        }
    }
    if (ImGui::BeginTable("objects", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                             ImGuiTableFlags_ScrollY, ImVec2(0, 180))) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("X");
        ImGui::TableSetupColumn("Y");
        ImGui::TableSetupColumn("Delete");
        ImGui::TableHeadersRow();
        int remove_index = -1;
        for (int i = 0; i < static_cast<int>(state.map.objects.size()); ++i) {
            MapObject& object = state.map.objects[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            if (ImGui::Selectable(std::to_string(i).c_str(), state.selected_object == i))
                state.selected_object = i;
            ImGui::TableSetColumnIndex(1);
            int id = static_cast<int>(object.id);
            if (ImGui::InputInt("##id", &id, 0, 0)) {
                object.id = static_cast<std::uint32_t>(std::max(0, id));
                state.dirty = true;
            }
            ImGui::TableSetColumnIndex(2);
            int x = object.x;
            if (ImGui::InputInt("##x", &x, 0, 0)) { object.x = x; state.dirty = true; }
            ImGui::TableSetColumnIndex(3);
            int y = object.y;
            if (ImGui::InputInt("##y", &y, 0, 0)) { object.y = y; state.dirty = true; }
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("Remove")) remove_index = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (remove_index >= 0) {
            state.map.objects.erase(state.map.objects.begin() + remove_index);
            if (state.selected_object == remove_index) state.selected_object = -1;
            else if (state.selected_object > remove_index) --state.selected_object;
            state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
            state.dirty = true;
        }
    }
    ImGui::End();
}

} // namespace

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & IMG_INIT_PNG) == 0)
        std::fprintf(stderr, "SDL_image could not initialize PNG support: %s\n", IMG_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Hurrican Map Editor — macOS Intel",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 150");

    bool running = true;
    EditorState state;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s &&
                (event.key.keysym.mod & (KMOD_GUI | KMOD_CTRL)) != 0 &&
                state.loaded && state.map_path[0] != '\0')
                SaveMap(state);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New blank map…")) ImGui::OpenPopup("New blank map");
                if (ImGui::MenuItem("Open map…", nullptr, false, !state.dirty)) {
                    if (hurrican::editor::ChooseMapToOpen(state.map_path, sizeof(state.map_path)))
                        LoadMap(state);
                }
                if (ImGui::MenuItem("Save", "⌘S", false,
                                    state.loaded && state.dirty && state.map_path[0] != '\0'))
                    SaveMap(state);
                if (ImGui::MenuItem("Save As…", nullptr, false, state.loaded)) {
                    if (hurrican::editor::ChooseMapToSave(state.map_path, sizeof(state.map_path)) && SaveMap(state))
                        LoadAtlases(state);
                }
                if (ImGui::MenuItem("Quit")) running = false;
                ImGui::EndMenu();
            }
            ImGui::TextUnformatted(state.dirty ? "  Unsaved changes" : "  Hurrican .map editor");
            ImGui::EndMainMenuBar();
        }
        if (ImGui::BeginPopupModal("New blank map", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Create an empty Hurrican .map level. Add a tileset and configure the level assets before testing in-game.");
            ImGui::InputInt("Width (tiles)", &state.new_map_width);
            ImGui::InputInt("Height (tiles)", &state.new_map_height);
            state.new_map_width = std::clamp(state.new_map_width, 1, static_cast<int>(MapDocument::kMaxWidth));
            state.new_map_height = std::clamp(state.new_map_height, 1, static_cast<int>(MapDocument::kMaxHeight));
            if (state.dirty)
                ImGui::TextColored(ImVec4(1, 0.35f, 0.25f, 1), "Save the current map before creating another.");
            ImGui::BeginDisabled(state.dirty);
            if (ImGui::Button("Create blank map")) {
                CreateNewMap(state);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        DrawInspector(state);
        DrawMapCanvas(state);

        ImGui::Render();
        int display_w = 0, display_h = 0;
        SDL_GL_GetDrawableSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.09f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ReleaseAtlases(state);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
