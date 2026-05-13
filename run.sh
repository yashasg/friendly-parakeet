#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
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
