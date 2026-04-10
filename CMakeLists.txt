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
# On Linux:   libsdl2-dev and catch2 are expected as system packages (apt).
#             vcpkg only provides EnTT (header-only, fast).
# On Windows: all three come from vcpkg (x64-windows-static-md triplet).
# Pass -DCMAKE_TOOLCHAIN_FILE or set VCPKG_ROOT so vcpkg is used for EnTT.

find_package(EnTT CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# SDL2_ttf — required for text rendering.
# System package on Linux (libsdl2-ttf-dev), vcpkg on Windows.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_TTF IMPORTED_TARGET SDL2_ttf)
endif()
if(NOT SDL2_TTF_FOUND)
    find_package(SDL2_ttf CONFIG QUIET)
endif()
# Create a unified target alias if found via pkg-config
if(SDL2_TTF_FOUND AND NOT TARGET SDL2_ttf::SDL2_ttf)
    if(TARGET PkgConfig::SDL2_TTF)
        add_library(SDL2_ttf::SDL2_ttf ALIAS PkgConfig::SDL2_TTF)
    endif()
endif()
# vcpkg static triplets create SDL2_ttf::SDL2_ttf-static instead of
# SDL2_ttf::SDL2_ttf.  Alias the static target so downstream code can
# always link against SDL2_ttf::SDL2_ttf.
if(TARGET SDL2_ttf::SDL2_ttf-static AND NOT TARGET SDL2_ttf::SDL2_ttf)
    add_library(SDL2_ttf::SDL2_ttf ALIAS SDL2_ttf::SDL2_ttf-static)
endif()
# find_package sets SDL2_ttf_FOUND (mixed-case); pkg_check_modules sets
# SDL2_TTF_FOUND (upper-case).  Accept either.
if(NOT SDL2_TTF_FOUND AND NOT SDL2_ttf_FOUND AND NOT TARGET SDL2_ttf::SDL2_ttf)
    message(FATAL_ERROR "SDL2_ttf is required. Install libsdl2-ttf-dev (apt) or add sdl2-ttf via vcpkg.")
endif()

# Normalize SDL2 target names (vcpkg may expose SDL2::SDL2 without SDL2::SDL2-static)
if(TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    add_library(SDL2::SDL2-static ALIAS SDL2::SDL2)
endif()
if(NOT TARGET SDL2::SDL2main)
    # vcpkg may provide SDL2::SDL2main or not — create a dummy if missing
    add_library(sdl2main_stub INTERFACE)
    add_library(SDL2::SDL2main ALIAS sdl2main_stub)
endif()

# ── Warnings-as-errors (project targets only) ────────────────
# Applied via an INTERFACE target so it never leaks into third-party code.
# Third-party include directories are marked SYSTEM so that headers we don't
# own can never produce errors under -Werror / /WX.
add_library(shapeshifter_warnings INTERFACE)
target_compile_options(shapeshifter_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
)

# Mark third-party include directories as SYSTEM so warnings from their
# headers are suppressed and cannot be promoted to errors by -Werror / /WX.
foreach(_dep EnTT::EnTT SDL2::SDL2-static Catch2::Catch2 Catch2::Catch2WithMain SDL2_ttf::SDL2_ttf SDL2_ttf::SDL2_ttf-static PkgConfig::SDL2_TTF)
    if(TARGET ${_dep})
        # Resolve alias targets — set_target_properties doesn't work on aliases
        get_target_property(_real ${_dep} ALIASED_TARGET)
        if(NOT _real)
            set(_real ${_dep})
        endif()
        get_target_property(_inc ${_real} INTERFACE_INCLUDE_DIRECTORIES)
        if(_inc)
            set_target_properties(${_real} PROPERTIES
                INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_inc}")
        endif()
    endif()
endforeach()

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
target_link_libraries(shapeshifter_lib PRIVATE shapeshifter_warnings)

# ── Platform capability flags ────────────────────────────────
# PLATFORM_DESKTOP is set for Windows / macOS / Linux builds.
# Mobile (Android / iOS) gets no keyboard handling compiled in.
# We use CMAKE_SYSTEM_NAME rather than SDL platform macros so that
# app/components/input.h stays portable (no SDL.h dependency in headers).
if(CMAKE_SYSTEM_NAME MATCHES "Windows|Darwin|Linux")
    target_compile_definitions(shapeshifter_lib PUBLIC PLATFORM_DESKTOP)
endif()

# ── Main executable ──────────────────────────────────────────
add_executable(shapeshifter
    app/main.cpp
    app/systems/render_system.cpp
    app/systems/input_system.cpp
    app/text_renderer.cpp
)
target_include_directories(shapeshifter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
)
target_link_libraries(shapeshifter PRIVATE
    shapeshifter_lib
    SDL2::SDL2main
    shapeshifter_warnings
)
target_link_libraries(shapeshifter PRIVATE SDL2_ttf::SDL2_ttf)

# Copy font assets next to the executable so text_init() can find them
file(GLOB FONT_FILES ${CMAKE_SOURCE_DIR}/assets/fonts/*.ttf)
if(FONT_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/assets/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FONT_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/assets/fonts/"
        COMMENT "Copying font assets"
    )
endif()

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
    shapeshifter_warnings
)

include(CTest)
include(Catch)
catch_discover_tests(shapeshifter_tests)
