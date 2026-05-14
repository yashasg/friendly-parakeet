#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Local pre-PR smoke test for the WebAssembly build (issue #1080). Skips the
# native build path entirely because the WASM artifacts come from build-web/.
if [[ "${1:-}" == "wasm-e2e" ]]; then
    shift
    exec ./tools/wasm_e2e.sh "$@"
fi

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
./build.sh

if [[ "${1:-}" == "test" ]]; then
    shift
    ctest --test-dir build --output-on-failure "$@"
elif [[ "${1:-}" == "bench" ]]; then
    shift
    ./build/shapeshifter_tests "[bench]" "$@"
else
    ./build/shapeshifter "$@"
fi
