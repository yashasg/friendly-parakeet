cmake_minimum_required(VERSION 3.20)
project(shapeshifter VERSION 0.1 LANGUAGES CXX)

# C++20 required: designated initializers are used throughout the codebase
# (MSVC rejects them in C++17 mode with error C7555)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Version (major.minor.githash) ────────────────────────────
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/app/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# ── Dependencies ─────────────────────────────────────────────
# Prefer vcpkg-provided packages (via CMAKE_TOOLCHAIN_FILE).
# Fall back to FetchContent for local builds without vcpkg.

find_package(EnTT CONFIG QUIET)
find_package(SDL2 CONFIG QUIET)
find_package(Catch2 3 CONFIG QUIET)

if(NOT EnTT_FOUND OR NOT SDL2_FOUND OR NOT Catch2_FOUND)
    message(STATUS "vcpkg packages not found — falling back to FetchContent")
    include(FetchContent)

    if(NOT EnTT_FOUND)
        FetchContent_Declare(EnTT
            GIT_REPOSITORY https://github.com/skypjack/entt.git
            GIT_TAG        v3.14.0
        )
        FetchContent_MakeAvailable(EnTT)
    endif()

    if(NOT SDL2_FOUND)
        FetchContent_Declare(SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-2.30.12
        )
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL2)
    endif()

    if(NOT Catch2_FOUND)
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()

# Normalize SDL2 target names (vcpkg uses SDL2::SDL2, FetchContent uses SDL2::SDL2-static)
if(TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    add_library(SDL2::SDL2-static ALIAS SDL2::SDL2)
endif()
if(NOT TARGET SDL2::SDL2main)
    # vcpkg may provide SDL2::SDL2main or not — create a dummy if missing
    add_library(sdl2main_stub INTERFACE)
    add_library(SDL2::SDL2main ALIAS sdl2main_stub)
endif()

# ── Game library (shared between exe and tests) ──────────────
file(GLOB SYSTEM_SOURCES app/systems/*.cpp)
# Exclude render_system.cpp from the library (requires SDL renderer)
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "render_system\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "input_system\\.cpp$")

add_library(shapeshifter_lib STATIC ${SYSTEM_SOURCES})
target_include_directories(shapeshifter_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(shapeshifter_lib PUBLIC
    EnTT::EnTT
    SDL2::SDL2-static
)

# ── Main executable ──────────────────────────────────────────
add_executable(shapeshifter app/main.cpp app/systems/render_system.cpp app/systems/input_system.cpp)
target_include_directories(shapeshifter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(shapeshifter PRIVATE
    shapeshifter_lib
    SDL2::SDL2main
)

# ── Tests + Benchmarks ───────────────────────────────────────
enable_testing()

file(GLOB TEST_SOURCES tests/*.cpp benchmarks/*.cpp)
add_executable(shapeshifter_tests ${TEST_SOURCES})
target_include_directories(shapeshifter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(shapeshifter_tests PRIVATE
    shapeshifter_lib
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(shapeshifter_tests)
