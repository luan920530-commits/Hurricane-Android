#include "MapDocument.hpp"
#include "FileDialogs.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

using hurrican::editor::MapDocument;
using hurrican::editor::MapObject;
using hurrican::editor::MapTile;

namespace {

struct EditorState {
    MapDocument map;
    bool loaded = false;
    bool dirty = false;
    bool paint_mode = false;
    int new_object_id = 1;
    int selected_x = 0;
    int selected_y = 0;
    int brush_layer = 0; // 0 = back layer, 1 = front layer
    int brush_tileset = 1;
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

bool LoadMap(EditorState& state) {
    std::string error;
    MapDocument loaded;
    if (!MapDocument::Load(state.map_path, loaded, error)) {
        std::snprintf(state.message, sizeof(state.message), "Open failed: %s", error.c_str());
        return false;
    }
    state.map = std::move(loaded);
    state.loaded = true;
    state.dirty = false;
    state.selected_x = 0;
    state.selected_y = 0;
    std::snprintf(state.message, sizeof(state.message), "Loaded %s (%u x %u, %zu objects)",
                  state.map_path, state.map.width(), state.map.height(), state.map.objects.size());
    return true;
}

bool SaveMap(EditorState& state) {
    if (!state.loaded)
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

ImU32 TileColor(const MapTile& tile) {
    if (tile.tileset_back == 0 && tile.tileset_front == 0)
        return IM_COL32(29, 34, 43, 255);
    const unsigned int seed = static_cast<unsigned int>(tile.tileset_back) * 71u +
                              static_cast<unsigned int>(tile.art_back) * 17u +
                              static_cast<unsigned int>(tile.tileset_front) * 31u + tile.art_front;
    const float hue = std::fmod(seed * 0.071f, 1.0f);
    ImVec4 color = ImColor::HSV(hue, 0.52f, 0.78f);
    if (tile.block_flags != 0) {
        color.x = std::min(1.0f, color.x + 0.20f);
        color.y *= 0.70f;
        color.z *= 0.70f;
    }
    return ImGui::ColorConvertFloat4ToU32(color);
}

void DrawMapCanvas(EditorState& state) {
    ImGui::Begin("Level map");
    ImGui::TextDisabled("Schematic tile preview; texture-atlas rendering is the next editor milestone.");
    ImGui::Checkbox("Paint brush", &state.paint_mode);
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
            if (cell >= 18.0f)
                draw->AddRect(p0, p1, IM_COL32(12, 15, 20, 90));
            if (x == state.selected_x && y == state.selected_y)
                draw->AddRect(p0, p1, IM_COL32(255, 224, 80, 255), 0.0f, 0, 2.0f);
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
            if (state.paint_mode) {
                MapTile& tile = state.map.tile(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
                if (state.brush_layer == 0) {
                    tile.tileset_back = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
                    tile.art_back = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
                } else {
                    tile.tileset_front = static_cast<std::uint8_t>(std::clamp(state.brush_tileset, 0, 255));
                    tile.art_front = static_cast<std::uint8_t>(std::clamp(state.brush_art, 0, 255));
                }
                state.dirty = true;
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void DrawInspector(EditorState& state) {
    ImGui::Begin("Level properties and tools");
    ImGui::InputText("Map path", state.map_path, sizeof(state.map_path));
    if (ImGui::Button("Open…")) {
        if (hurrican::editor::ChooseMapToOpen(state.map_path, sizeof(state.map_path)))
            LoadMap(state);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.loaded);
    if (ImGui::Button("Save"))
        SaveMap(state);
    ImGui::SameLine();
    if (ImGui::Button("Save As…")) {
        if (hurrican::editor::ChooseMapToSave(state.map_path, sizeof(state.map_path)))
            SaveMap(state);
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

    ImGui::SeparatorText("Tile brush / selected tile");
    ImGui::Combo("Layer", &state.brush_layer, "Background\0Foreground\0");
    ImGui::InputInt("Tileset index", &state.brush_tileset);
    ImGui::InputInt("Tile art index", &state.brush_art);
    if (state.selected_x >= 0 && state.selected_y >= 0) {
        MapTile& tile = state.map.tile(static_cast<std::uint32_t>(state.selected_x),
                                       static_cast<std::uint32_t>(state.selected_y));
        int tileset = state.brush_layer == 0 ? tile.tileset_back : tile.tileset_front;
        int art = state.brush_layer == 0 ? tile.art_back : tile.art_front;
        ImGui::Text("Current layer: tileset %d, art %d", tileset, art);
        std::uint32_t flags = tile.block_flags;
        if (ImGui::InputScalar("Block flags (hex value)", ImGuiDataType_U32, &flags, nullptr, nullptr, "%08X")) {
            tile.block_flags = flags;
            state.dirty = true;
        }
        ImGui::TextDisabled("Tile indices are edited directly; texture names are preserved from the original map.");
    }

    ImGui::SeparatorText("Objects");
    if (ImGui::Button("Add player start")) {
        MapObject object{};
        object.id = 0;
        object.x = state.selected_x * 20;
        object.y = state.selected_y * 20;
        object.change_light = 1;
        state.map.objects.push_back(object);
        state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
        state.dirty = true;
    }
    ImGui::InputInt("New object ID", &state.new_object_id);
    state.new_object_id = std::clamp(state.new_object_id, 1, 65535);
    if (ImGui::Button("Add object at selection")) {
        MapObject object{};
        object.id = static_cast<std::uint32_t>(state.new_object_id);
        object.x = state.selected_x * 20;
        object.y = state.selected_y * 20;
        object.change_light = 1;
        state.map.objects.push_back(object);
        state.map.header.object_count = static_cast<std::uint32_t>(state.map.objects.size());
        state.dirty = true;
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
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
            ImGui::TableSetColumnIndex(1);
            int id = static_cast<int>(object.id);
            ImGui::PushID(i);
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
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open map…")) {
                    if (hurrican::editor::ChooseMapToOpen(state.map_path, sizeof(state.map_path)))
                        LoadMap(state);
                }
                if (ImGui::MenuItem("Save", "⌘S", state.loaded && state.dirty)) SaveMap(state);
                if (ImGui::MenuItem("Save As…", nullptr, false, state.loaded)) {
                    if (hurrican::editor::ChooseMapToSave(state.map_path, sizeof(state.map_path)))
                        SaveMap(state);
                }
                if (ImGui::MenuItem("Quit")) running = false;
                ImGui::EndMenu();
            }
            ImGui::TextUnformatted(state.dirty ? "  Unsaved changes" : "  Hurrican .map editor");
            ImGui::EndMainMenuBar();
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
