#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

BUILD_TYPE="${1:-Release}"

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo "Error: VCPKG_ROOT is not set. vcpkg is required to build." >&2
    echo "  Install vcpkg: https://vcpkg.io" >&2
    echo "  Then set: export VCPKG_ROOT=/path/to/vcpkg" >&2
    exit 1
fi

CMAKE_ARGS=(
    -B build
    -S .
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    "-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
)

# Honour CC/CXX env vars when set (Linux/macOS).
# On Windows (MSYS/MinGW shell used by GitHub Actions) skip the override
# so CMake uses the default Visual Studio generator with MSVC.
case "$(uname -s)" in
    MINGW*|CYGWIN*|MSYS*)
        ;;
    *)
        if [[ -n "${CC:-}" ]]; then
            CMAKE_ARGS+=("-DCMAKE_C_COMPILER=${CC}")
        fi
        if [[ -n "${CXX:-}" ]]; then
            CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=${CXX}")
        fi
        ;;
esac

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
