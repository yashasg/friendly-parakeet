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
# generator so that Clang (installed by setup-cpp) is used directly
# instead of the default Visual Studio generator with MSVC.
case "$(uname -s)" in
    MINGW*|CYGWIN*|MSYS*)
        CMAKE_ARGS+=("-G" "Ninja")
        ;;
esac

# Always run CMake configure, but in CI skip vcpkg manifest install when
# packages are already restored from cache — vcpkg re-runs install on every
# configure, rebuilding packages if the runner's vcpkg version changed.
# Gated to CI only to avoid silently using stale packages in local builds.
if [[ "${CI:-}" == "true" ]] && [[ -d build/vcpkg_installed ]] && [[ -f build/CMakeCache.txt ]]; then
    # Packages already installed from cache — tell vcpkg not to re-install.
    # This prevents vcpkg from rebuilding all deps when the runner's
    # pre-installed vcpkg version differs from what built the cache.
    CMAKE_ARGS+=("-DVCPKG_MANIFEST_INSTALL=OFF")
    echo "vcpkg packages found in cache, skipping vcpkg install."
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
