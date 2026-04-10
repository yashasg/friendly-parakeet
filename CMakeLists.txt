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

if(NOT EMSCRIPTEN)
    # Native builds: raylib and Catch2 from vcpkg
    find_package(raylib CONFIG REQUIRED)
    find_package(Catch2 3 CONFIG REQUIRED)
else()
    # Emscripten: build raylib from source (no vcpkg triplet available).
    # Mirrors raylib's own build_webassembly.yml.
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    ON
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(PLATFORM Web CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(raylib)
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
set(_system_deps EnTT::EnTT raylib)
if(NOT EMSCRIPTEN)
    list(APPEND _system_deps Catch2::Catch2 Catch2::Catch2WithMain)
endif()
foreach(_dep ${_system_deps})
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
elseif(UNIX AND NOT EMSCRIPTEN)
    target_link_libraries(shapeshifter PRIVATE GL X11 Xrandr m pthread dl)
endif()

# ── Emscripten linker flags ──────────────────────────────────
if(EMSCRIPTEN)
    target_compile_definitions(shapeshifter PRIVATE PLATFORM_WEB)

    # Generate a minimal HTML shell at configure time — no static file to maintain.
    set(_shell_html "${CMAKE_BINARY_DIR}/shell.html")
    file(WRITE "${_shell_html}" [=[<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>SHAPESHIFTER</title>
  <style>
    *{margin:0;padding:0;box-sizing:border-box}
    body{background:#0f0f19;display:flex;justify-content:center;align-items:center;height:100vh;overflow:hidden;font-family:monospace;color:#ccc}
    #canvas{display:block;border:1px solid #333}
    #status{position:absolute;bottom:16px;text-align:center;width:100%;font-size:14px}
  </style>
</head>
<body>
  <canvas id="canvas" oncontextmenu="event.preventDefault()" tabindex="-1"></canvas>
  <div id="status">Loading…</div>
  <script>
    var Module={
      canvas:document.getElementById('canvas'),
      setStatus:function(t){document.getElementById('status').innerText=t},
      monitorRunDependencies:function(left){if(left===0)document.getElementById('status').style.display='none'}
    };
  </script>
  {{{ SCRIPT }}}
</body>
</html>]=])

    target_link_options(shapeshifter PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        --shell-file ${_shell_html}
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
    )
    set_target_properties(shapeshifter PROPERTIES SUFFIX ".html")
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

# ── Tests + Benchmarks (native only) ─────────────────────────
if(NOT EMSCRIPTEN)
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
endif()
