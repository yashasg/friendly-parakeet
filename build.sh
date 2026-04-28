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

# In CI, skip vcpkg manifest install only when the cached install is known to
# match the current manifest inputs.  We store a hash of vcpkg.json +
# vcpkg-overlay contents as a stamp file inside the build directory; the stamp
# travels with the cached build/ tree.  A stale restore (e.g. after adding
# magic-enum) has no stamp or a mismatched hash, so vcpkg runs normally.
_VCPKG_STAMP="build/.vcpkg_manifest_stamp"
_MANIFEST_HASH=$(
    { cat vcpkg.json; find vcpkg-overlay -type f 2>/dev/null | sort | xargs cat 2>/dev/null || true; } \
    | (sha256sum 2>/dev/null || shasum -a 256 2>/dev/null) \
    | cut -d' ' -f1
)

if [[ "${CI:-}" == "true" ]] && [[ -d build/vcpkg_installed ]] && [[ -f build/CMakeCache.txt ]]; then
    if [[ -f "$_VCPKG_STAMP" ]] && [[ "$(cat "$_VCPKG_STAMP")" == "$_MANIFEST_HASH" ]]; then
        CMAKE_ARGS+=("-DVCPKG_MANIFEST_INSTALL=OFF")
        echo "vcpkg packages found in cache and manifest unchanged, skipping vcpkg install."
    else
        CMAKE_ARGS+=("-DVCPKG_MANIFEST_INSTALL=ON")
        echo "vcpkg cache present but manifest changed or stamp missing; running full vcpkg install."
    fi
else
    CMAKE_ARGS+=("-DVCPKG_MANIFEST_INSTALL=ON")
fi

cmake "${CMAKE_ARGS[@]}"
# Write/refresh the stamp so the next cached run can safely skip install.
mkdir -p build
echo "$_MANIFEST_HASH" > "$_VCPKG_STAMP"
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
