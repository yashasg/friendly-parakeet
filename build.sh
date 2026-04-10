#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
SCRIPT_DIR="$(pwd)"

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
    "-DVCPKG_OVERLAY_PORTS=${SCRIPT_DIR}/vcpkg-overlay"
)

# Honour CC/CXX env vars when set on all platforms.
if [[ -n "${CC:-}" ]]; then
    CMAKE_ARGS+=("-DCMAKE_C_COMPILER=${CC}")
fi
if [[ -n "${CXX:-}" ]]; then
    CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=${CXX}")
fi

# Forward vcpkg triplet to CMake so the toolchain uses the correct static/dynamic
# variant.  VCPKG_DEFAULT_TRIPLET is read by the vcpkg CLI but NOT by the vcpkg
# CMake toolchain file, which requires the CMake variable VCPKG_TARGET_TRIPLET.
# Without this, vcpkg auto-detects "x64-windows" (dynamic DLL) on Windows even
# when the CI workflow sets VCPKG_DEFAULT_TRIPLET=x64-windows-static-md.
if [[ -n "${VCPKG_DEFAULT_TRIPLET:-}" ]]; then
    CMAKE_ARGS+=("-DVCPKG_TARGET_TRIPLET=${VCPKG_DEFAULT_TRIPLET}")
fi

# On Windows (MSYS/MinGW shell used by GitHub Actions), use the Ninja
# generator so that the compiler installed by setup-cpp (Clang or MSVC)
# is used directly instead of the default Visual Studio generator.
case "$(uname -s)" in
    MINGW*|CYGWIN*|MSYS*)
        CMAKE_ARGS+=("-G" "Ninja")
        ;;
esac

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
