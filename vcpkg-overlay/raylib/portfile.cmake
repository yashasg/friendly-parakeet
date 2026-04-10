if(VCPKG_TARGET_IS_LINUX)
    message(
    "raylib (RGFW) currently requires the following libraries from the system package manager:\n\
    libgl1-mesa-dev\n\
    libx11-dev\n\
    libxrandr-dev\n\
These can be installed on Ubuntu systems via sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev"
    )
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO raysan5/raylib
    REF "${VERSION}"
    SHA512 503483a5436e189ad67533dc6c90be592283b84fbd57c86ab457dd1507b1dd11c897767ea9efa83affaf236f2711ec59e56658cf6fcad582a790a5fdc01b5ace
    HEAD_REF master
    PATCHES
        android.diff
        fix-link-path.patch
)

# ── Patch CMake to add RGFW platform support ─────────────────
# raylib 5.5 ships the RGFW backend source (rcore_desktop_rgfw.c) but the
# CMake build system only added the RGFW option post-5.5. We patch it in.

# 1. Add RGFW to the platform enum in CMakeOptions.txt
vcpkg_replace_string("${SOURCE_PATH}/CMakeOptions.txt"
    "\"Desktop;Web;Android;Raspberry Pi;DRM;SDL\""
    "\"Desktop;Web;Android;Raspberry Pi;DRM;SDL;RGFW\""
)

# 2. Skip GLFW in GlfwImport.cmake when PLATFORM=RGFW
vcpkg_replace_string("${SOURCE_PATH}/cmake/GlfwImport.cmake"
    [[elseif("${PLATFORM}" STREQUAL "DRM")
    MESSAGE(STATUS "No GLFW required on PLATFORM_DRM")]]
    [[elseif("${PLATFORM}" STREQUAL "DRM")
    MESSAGE(STATUS "No GLFW required on PLATFORM_DRM")
elseif("${PLATFORM}" STREQUAL "RGFW")
    MESSAGE(STATUS "No GLFW required on PLATFORM_RGFW")]]
)

# 3. Add RGFW case to LibraryConfigurations.cmake (before the final endif)
vcpkg_replace_string("${SOURCE_PATH}/cmake/LibraryConfigurations.cmake"
    [[elseif ("${PLATFORM}" MATCHES "SDL")
    find_package(SDL2 REQUIRED)
    set(PLATFORM_CPP "PLATFORM_DESKTOP_SDL")
    set(LIBS_PRIVATE SDL2::SDL2)

endif ()]]
    [[elseif ("${PLATFORM}" MATCHES "SDL")
    find_package(SDL2 REQUIRED)
    set(PLATFORM_CPP "PLATFORM_DESKTOP_SDL")
    set(LIBS_PRIVATE SDL2::SDL2)

elseif ("${PLATFORM}" STREQUAL "RGFW")
    set(PLATFORM_CPP "PLATFORM_DESKTOP_RGFW")

    if (APPLE)
        set(GRAPHICS "GRAPHICS_API_OPENGL_33")
        find_library(COCOA Cocoa)
        find_library(OPENGL_LIBRARY OpenGL)
        set(LIBS_PRIVATE ${COCOA} ${OPENGL_LIBRARY})
        if (NOT CMAKE_SYSTEM STRLESS "Darwin-18.0.0")
            add_definitions(-DGL_SILENCE_DEPRECATION)
        endif ()
    elseif (WIN32)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        find_package(OpenGL REQUIRED)
        set(LIBS_PRIVATE ${OPENGL_LIBRARIES} gdi32 winmm)
    elseif (UNIX)
        find_package(X11 REQUIRED)
        find_package(OpenGL REQUIRED)
        set(LIBS_PRIVATE ${X11_LIBRARIES} ${OPENGL_LIBRARIES} m pthread dl)
    endif ()

endif ()]]
)

file(GLOB vendored_headers RELATIVE "${SOURCE_PATH}/src/external"
    "${SOURCE_PATH}/src/external/cgltf.h"
    "${SOURCE_PATH}/src/external/nanosvg*.h"
    "${SOURCE_PATH}/src/external/qoi.h"
    "${SOURCE_PATH}/src/external/s*fl.h"  # from mmx
    "${SOURCE_PATH}/src/external/stb_*"
)
file(GLOB vendored_audio_headers RELATIVE "${SOURCE_PATH}/src/external"
    "${SOURCE_PATH}/src/external/dr_*.h"
    "${SOURCE_PATH}/src/external/miniaudio.h"
)
set(optional_vendored_headers
    "stb_image_resize2.h"  # not yet in vcpkg
)
foreach(header IN LISTS vendored_headers vendored_audio_headers)
    unset(vcpkg_file)
    find_file(vcpkg_file NAMES "${header}" PATHS "${CURRENT_INSTALLED_DIR}/include" PATH_SUFFIXES mmx nanosvg NO_DEFAULT_PATH NO_CACHE)
    if(header IN_LIST vendored_audio_headers AND NOT "audio" IN_LIST FEATURES)
        message(STATUS "Emptying '${header}' (audio disabled)")
        file(WRITE "${SOURCE_PATH}/src/external/${vcpkg_file}" "# audio disabled")
    elseif(vcpkg_file)
        message(STATUS "De-vendoring '${header}'")
        file(COPY "${vcpkg_file}" DESTINATION "${SOURCE_PATH}/src/external")
    elseif(header IN_LIST optional_vendored_headers)
        message(STATUS "Not de-vendoring '${header}' (absent in vcpkg)")
    else()
        message(FATAL_ERROR "No replacement for vendored '${header}'")
    endif()
endforeach()

# ── Platform selection ───────────────────────────────────────
# Use RGFW backend instead of GLFW. RGFW is a single-header windowing
# library bundled inside raylib's source tree — no external dependency.
set(PLATFORM_OPTIONS "")
if(VCPKG_TARGET_IS_ANDROID)
    list(APPEND PLATFORM_OPTIONS -DPLATFORM=Android -DUSE_EXTERNAL_GLFW=OFF)
elseif(VCPKG_TARGET_IS_EMSCRIPTEN)
    list(APPEND PLATFORM_OPTIONS -DPLATFORM=Web -DUSE_EXTERNAL_GLFW=OFF)
else()
    # RGFW: no GLFW dependency — uses Cocoa (macOS), X11 (Linux), WinAPI (Windows)
    list(APPEND PLATFORM_OPTIONS -DPLATFORM=RGFW -DUSE_EXTERNAL_GLFW=OFF)
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        audio SUPPORT_MODULE_RAUDIO
        audio USE_AUDIO
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DCMAKE_POLICY_DEFAULT_CMP0072=NEW  # Prefer GLVND
        -DCUSTOMIZE_BUILD=ON
        ${PLATFORM_OPTIONS}
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/${PORT})
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/raylib.h" "defined(USE_LIBTYPE_SHARED)" "1")
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
