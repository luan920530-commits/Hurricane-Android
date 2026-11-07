cmake_minimum_required(VERSION 3.24)

# Set Apple target properties before enabling C++/Objective-C++ compilers.
set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build Intel macOS application")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
project(HurricanMapEditor VERSION 0.1.0 LANGUAGES CXX OBJCXX)

# First native target is Intel-only as requested and runs on macOS Monterey 12 or later.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# Dear ImGui core and the SDL2/OpenGL backends are vendored at a fixed upstream version.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

add_executable(HurricanMapEditor MACOSX_BUNDLE
    src/AppMain.cpp
    src/FileDialogs.mm
    src/MapDocument.cpp
)
target_include_directories(HurricanMapEditor PRIVATE src)
target_compile_definitions(HurricanMapEditor PRIVATE SDL_MAIN_HANDLED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
target_link_libraries(HurricanMapEditor PRIVATE imgui SDL2::SDL2 SDL2_image::SDL2_image OpenGL::GL ${APPKIT_FRAMEWORK})
set_target_properties(HurricanMapEditor PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Hurrican Map Editor"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.hurrican.mapeditor"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(map-format-tests tests/MapFormatTests.cpp src/MapDocument.cpp)
    add_test(NAME map_format_roundtrip
        COMMAND map-format-tests "${CMAKE_CURRENT_LIST_DIR}/../Hurrican/data/levels")
endif()
