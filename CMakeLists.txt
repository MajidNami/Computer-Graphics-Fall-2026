cmake_minimum_required(VERSION 3.20)

project(CityLab VERSION 1.0.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

# ---------------------------------------------------------------------------
# Options
#
#   -DCITYLAB_TEAMS=team07          build only your own zone (faster, and a
#                                   broken neighbour cannot stop your build)
#   -DCITYLAB_SKIP_TEAMS=team12     leave out zones that do not compile
#   -DCITYLAB_WERROR=ON             treat warnings as errors (CI)
# ---------------------------------------------------------------------------
set(CITYLAB_TEAMS "all" CACHE STRING "Team zones to build: 'all' or a list such as team07;team08")
set(CITYLAB_SKIP_TEAMS "" CACHE STRING "Team zones to leave out")
option(CITYLAB_WERROR "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
# The program is built straight into build/ (build/Release/ with Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(OpenGL REQUIRED)

# Warnings shared by the city and the teams.
add_library(citylab_warnings INTERFACE)
if(MSVC)
    target_compile_options(citylab_warnings INTERFACE /W4 /utf-8 /permissive- /wd4100 /wd4244 /wd4267 /wd4305)
    target_compile_definitions(citylab_warnings INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    if(CITYLAB_WERROR)
        target_compile_options(citylab_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(citylab_warnings INTERFACE -Wall -Wextra -Wno-unused-parameter
                                                      $<$<COMPILE_LANGUAGE:OBJCXX>:-Wno-deprecated-declarations>)
    if(CITYLAB_WERROR)
        target_compile_options(citylab_warnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# The SDK teams compile against: citylab/*.h and GLM, nothing else.
# ---------------------------------------------------------------------------
add_library(citylab_sdk STATIC sdk/cpp/citylab/Shapes.cpp sdk/cpp/citylab/Props.cpp)
target_include_directories(citylab_sdk PUBLIC sdk/cpp)
target_include_directories(citylab_sdk SYSTEM PUBLIC core/external/glm)
target_link_libraries(citylab_sdk PUBLIC OpenGL::GL PRIVATE citylab_warnings)

# ---------------------------------------------------------------------------
# The city (staff code)
# ---------------------------------------------------------------------------
if(WIN32)
    set(CITYLAB_PLATFORM core/framework/platform/PlatformWindow_win32.cpp)
elseif(APPLE)
    set(CITYLAB_PLATFORM core/framework/platform/PlatformWindow_macos.mm)
else()
    set(CITYLAB_PLATFORM core/framework/platform/PlatformWindow_x11.cpp)
endif()

add_library(citylab_core STATIC
    ${CITYLAB_PLATFORM}
    core/engine/Camera.cpp
    core/engine/Clock.cpp
    core/engine/Collision.cpp
    core/engine/Font.cpp
    core/engine/Hud.cpp
    core/engine/Json.cpp
    core/engine/Log.cpp
    core/engine/Options.cpp
    core/engine/Screenshot.cpp
    core/engine/Settings.cpp
    core/engine/Sky.cpp
    core/engine/Texture.cpp
    core/engine/VertexCount.cpp
    core/city/CityArchitecture.cpp
    core/city/Chunks.cpp
    core/city/CityScene.cpp
    core/city/Geometry.cpp
    core/city/Layout.cpp
    core/city/Life.cpp
    core/city/Materials.cpp
    core/city/Props.cpp
    core/city/Registry.cpp
    core/zones/ZoneHost.cpp
    core/app/Application.cpp
    core/app/Checker.cpp
    core/app/Overlay.cpp
    core/app/Tour.cpp
    core/external/stb/stb_impl.cpp
)
target_include_directories(citylab_core PUBLIC core)
target_include_directories(citylab_core SYSTEM PRIVATE core/external)
target_link_libraries(citylab_core PUBLIC citylab_sdk PRIVATE citylab_warnings)
target_compile_definitions(citylab_core PRIVATE
    CITYLAB_VERSION="${PROJECT_VERSION}"
    CITYLAB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    CITYLAB_BINARY_DIR="${CMAKE_BINARY_DIR}")
set_source_files_properties(core/external/stb/stb_impl.cpp PROPERTIES COMPILE_OPTIONS
    "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")

if(WIN32)
    target_link_libraries(citylab_core PUBLIC gdi32 user32)
elseif(APPLE)
    target_link_libraries(citylab_core PUBLIC "-framework Cocoa")
else()
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(citylab_core PUBLIC X11::X11 Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Team zones: one OBJECT library per team in teams/teamNN/src.
# Each team sees only the SDK and its own folder, and gets CITYLAB_TEAM_ID.
# See cmake/CityLabTeams.cmake.
# ---------------------------------------------------------------------------
include(cmake/CityLabTeams.cmake)
citylab_collect_teams(CITYLAB_TEAM_OBJECTS)

add_executable(citylab core/app/main.cpp ${CITYLAB_TEAM_OBJECTS})
target_link_libraries(citylab PRIVATE citylab_core citylab_warnings)
target_compile_definitions(citylab PRIVATE CITYLAB_VERSION="${PROJECT_VERSION}")
if(MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT citylab)
    set_property(TARGET citylab PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(APPLE)
    set_target_properties(citylab PROPERTIES XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
