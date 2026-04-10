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
# EnTT: header-only ECS framework (vcpkg on all platforms).
# raylib: rendering, windowing, input, audio (vcpkg on native platforms).
# Catch2: test framework (vcpkg on Windows/macOS, system package on Linux).

find_package(EnTT CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

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
foreach(_dep EnTT::EnTT raylib Catch2::Catch2 Catch2::Catch2WithMain)
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
    raylib
    shapeshifter_warnings
)

# ── Platform link dependencies for raylib (RGFW backend) ─────
# RGFW uses native platform APIs directly — no GLFW.
if(APPLE)
    target_link_libraries(shapeshifter PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreVideo"
    )
elseif(WIN32)
    target_link_libraries(shapeshifter PRIVATE opengl32 gdi32 winmm)
elseif(UNIX)
    target_link_libraries(shapeshifter PRIVATE GL X11 Xrandr m pthread dl)
endif()

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
