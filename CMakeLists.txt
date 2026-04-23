cmake_minimum_required(VERSION 3.20)

# Auto-detect vcpkg toolchain from VCPKG_ROOT when no toolchain is specified.
# Referencing CMAKE_TOOLCHAIN_FILE here also prevents the CMake warning
# "Manually-specified variables were not used" on reconfigure.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(shapeshifter VERSION 0.1 LANGUAGES CXX)

# C++20 required: designated initializers are used throughout the codebase
# (MSVC rejects them in C++17 mode with error C7555)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(SHAPESHIFTER_UNITY_BUILD "Enable unity builds for faster compilation" OFF)
set(CMAKE_UNITY_BUILD ${SHAPESHIFTER_UNITY_BUILD})

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
# raylib: rendering, windowing, input, audio (vcpkg on all platforms, including
#         wasm32-emscripten via the vcpkg-overlay portfile).
# Catch2: test framework (vcpkg on Windows/macOS, system package on Linux).
# nlohmann-json: JSON parsing for beat map loading (vcpkg on all platforms).

find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)

if(NOT EMSCRIPTEN)
    find_package(Catch2 3 CONFIG REQUIRED)
endif()

# ── Warnings-as-errors (project targets only) ────────────────
# Applied via an INTERFACE target so it never leaks into third-party code.
# Third-party include directories are marked SYSTEM so that headers we don't
# own can never produce errors under -Werror.
add_library(shapeshifter_warnings INTERFACE)
target_compile_options(shapeshifter_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Werror>
)

# Mark third-party include directories as SYSTEM so warnings from their
# headers are suppressed and cannot be promoted to errors by -Werror.
set(_system_deps EnTT::EnTT raylib nlohmann_json::nlohmann_json)
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
# Exclude render_system.cpp from the library (requires raylib renderer)
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "render_system\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "/input_system\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "song_playback_system\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "text_renderer\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "camera_system\\.cpp$")
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "shape_mesh_builder\\.cpp$")

add_library(shapeshifter_lib STATIC ${SYSTEM_SOURCES})
target_include_directories(shapeshifter_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(shapeshifter_lib PUBLIC
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)
target_link_libraries(shapeshifter_lib PRIVATE shapeshifter_warnings)

# ── Platform capability flags ────────────────────────────────
# PLATFORM_DESKTOP is set for Windows / macOS / Linux builds.
# PLATFORM_WEB is set for Emscripten builds.
# PLATFORM_HAS_KEYBOARD is set for platforms with keyboard input.
# Mobile (Android / iOS) gets no keyboard handling compiled in.
if(CMAKE_SYSTEM_NAME MATCHES "Windows|Darwin|Linux")
    target_compile_definitions(shapeshifter_lib PUBLIC PLATFORM_DESKTOP PLATFORM_HAS_KEYBOARD)
elseif(EMSCRIPTEN)
    target_compile_definitions(shapeshifter_lib PUBLIC PLATFORM_WEB PLATFORM_HAS_KEYBOARD)
endif()

# ── Main executable ──────────────────────────────────────────
add_executable(shapeshifter
    app/main.cpp
    app/game_loop.cpp
    app/platform_display.cpp
    app/systems/camera_system.cpp
    app/systems/shape_mesh_builder.cpp
    app/systems/render_system.cpp
    app/systems/input_system.cpp
    app/systems/song_playback_system.cpp
    app/systems/text_renderer.cpp
    app/util/file_logger.cpp
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
    configure_file(
        ${CMAKE_SOURCE_DIR}/app/sw.js
        ${CMAKE_BINARY_DIR}/generated/sw.js
        @ONLY
    )

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
    html,body{height:100%;overflow:hidden;touch-action:none}
    body{background:#0f0f19;display:flex;flex-direction:column;justify-content:center;align-items:center;font-family:monospace;color:#ccc;position:relative}
    #canvas{display:block;width:100%;height:100%;touch-action:none}
    #loader{position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);text-align:center;z-index:10}
    #loader h1{color:#50b4ff;font-size:28px;margin-bottom:24px;letter-spacing:4px}
    #bar-bg{width:300px;height:8px;background:#1a1a2e;border-radius:4px;overflow:hidden;margin:0 auto 12px}
    #bar-fg{width:0%;height:100%;background:linear-gradient(90deg,#50b4ff,#64ff64);border-radius:4px;transition:width .2s}
    #pct{font-size:14px;color:#888}
  </style>
</head>
<body>
  <div id="loader">
    <h1>SHAPESHIFTER</h1>
    <div id="bar-bg"><div id="bar-fg"></div></div>
    <div id="pct">Downloading 0%</div>
  </div>
  <canvas id="canvas" oncontextmenu="event.preventDefault()" tabindex="0"></canvas>
  <script>
    var c=document.getElementById('canvas');
    var bar=document.getElementById('bar-fg');
    var pct=document.getElementById('pct');
    var loader=document.getElementById('loader');
    function showCanvas(){loader.style.display='none';c.focus();window.dispatchEvent(new Event('resize'))}
    var Module={
      canvas:c,
      setStatus:function(t){
        if(!t){showCanvas();return}
        var m=t.match(/(\d+(?:\.\d+)?)\/(\d+)/);
        if(m){
          var p=Math.round(parseInt(m[1])/parseInt(m[2])*100);
          bar.style.width=p+'%';
          var mb=(parseInt(m[1])/1048576).toFixed(1);
          var total=(parseInt(m[2])/1048576).toFixed(0);
          pct.innerText='Downloading '+p+'% ('+mb+'/'+total+' MB)';
        }else{pct.innerText=t}
      },
      monitorRunDependencies:function(left){
        if(left===0){showCanvas()}
      }
    };
    function tryResumeAudio(){
      try{
        if(Module&&typeof Module.resumeAudioContext==='function'){Module.resumeAudioContext();return;}
        if(Module&&Module.SDL2&&Module.SDL2.audioContext&&Module.SDL2.audioContext.state==='suspended'){Module.SDL2.audioContext.resume();return;}
        if(Module&&Module.AL&&Module.AL.currentCtx&&Module.AL.currentCtx.audioCtx&&Module.AL.currentCtx.audioCtx.state==='suspended'){Module.AL.currentCtx.audioCtx.resume();return;}
      }catch(_){}
    }
    c.addEventListener('pointerdown',function(){c.focus();tryResumeAudio();});
    c.addEventListener('click',function(){c.focus();tryResumeAudio()});
    c.addEventListener('touchstart',function(e){e.preventDefault();c.focus();tryResumeAudio();},{passive:false});
    c.addEventListener('touchend',function(e){e.preventDefault();},{passive:false});
    document.addEventListener('touchmove',function(e){e.preventDefault()},{passive:false});
    if('serviceWorker' in navigator){
      navigator.serviceWorker.register('sw.js',{updateViaCache:'none'})
        .then(function(reg){reg.update().catch(function(){})})
        .catch(function(){})
    }
  </script>
  {{{ SCRIPT }}}
</body>
</html>]=])

    target_link_options(shapeshifter PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sDYNAMIC_EXECUTION=0
        --shell-file ${_shell_html}
    )
    # Use LINK_FLAGS (raw string) for the two --preload-file entries so that
    # CMake does not deduplicate the identical flag prefixes, which would strip
    # the second entry and leave its value as an orphaned positional argument.
    set_property(TARGET shapeshifter APPEND_STRING PROPERTY LINK_FLAGS
        " --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets --preload-file ${CMAKE_SOURCE_DIR}/content@/content"
    )
    set_target_properties(shapeshifter PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
    )
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/generated/sw.js
            $<TARGET_FILE_DIR:shapeshifter>/sw.js
        COMMENT "Copying service worker"
    )
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

# Copy beatmap JSON files next to the executable
file(GLOB BEATMAP_FILES ${CMAKE_SOURCE_DIR}/content/beatmaps/*.json)
if(BEATMAP_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/content/beatmaps"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BEATMAP_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/beatmaps/"
        COMMENT "Copying beatmap assets"
    )
endif()

# Copy audio files next to the executable
file(GLOB AUDIO_FILES ${CMAKE_SOURCE_DIR}/content/audio/*.flac)
if(AUDIO_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/content/audio"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AUDIO_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/audio/"
        COMMENT "Copying audio assets"
    )
endif()

# Copy UI screen JSON files next to the executable
file(GLOB UI_SCREEN_FILES ${CMAKE_SOURCE_DIR}/content/ui/screens/*.json)
file(GLOB UI_ROOT_FILES ${CMAKE_SOURCE_DIR}/content/ui/*.json)
if(UI_SCREEN_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/content/ui/screens"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${UI_SCREEN_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/ui/screens/"
        COMMENT "Copying UI screen assets"
    )
endif()
if(UI_ROOT_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${UI_ROOT_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/ui/"
        COMMENT "Copying UI route config"
    )
endif()

# Copy shared constants JSON next to the executable
add_custom_command(TARGET shapeshifter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:shapeshifter>/content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/content/constants.json"
        "$<TARGET_FILE_DIR:shapeshifter>/content/constants.json"
    COMMENT "Copying shared constants"
)

# ── Tests + Benchmarks (native only) ─────────────────────────
if(NOT EMSCRIPTEN)
    enable_testing()

    file(GLOB TEST_SOURCES tests/*.cpp benchmarks/*.cpp)
    add_executable(shapeshifter_tests
        ${TEST_SOURCES}
        app/util/file_logger.cpp
        app/systems/camera_system.cpp
        app/systems/shape_mesh_builder.cpp
        app/systems/song_playback_system.cpp
        app/platform_display.cpp
    )
    target_include_directories(shapeshifter_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app
    )
    target_link_libraries(shapeshifter_tests PRIVATE
        shapeshifter_lib
        raylib
        Catch2::Catch2WithMain
        shapeshifter_warnings
    )
    if(APPLE)
        target_link_libraries(shapeshifter_tests PRIVATE
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreFoundation"
            "-framework CoreVideo"
        )
    elseif(WIN32)
        target_link_libraries(shapeshifter_tests PRIVATE opengl32 gdi32 winmm)
    elseif(UNIX)
        target_link_libraries(shapeshifter_tests PRIVATE GL X11 Xrandr m pthread dl)
    endif()

    include(CTest)
    include(Catch)
    catch_discover_tests(shapeshifter_tests)
endif()
