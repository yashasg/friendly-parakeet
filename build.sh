#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

BUILD_TYPE="${1:-Release}"

CMAKE_ARGS=(-B build -S . -DCMAKE_BUILD_TYPE="$BUILD_TYPE")

# Use vcpkg toolchain if available
if [[ -n "${VCPKG_ROOT:-}" ]]; then
    CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build build --config "$BUILD_TYPE" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
