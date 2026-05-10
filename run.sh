#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
./build.sh

if [[ "${1:-}" == "test" ]]; then
    shift
    ./build/shapeshifter_tests "$@"
    python3 -m unittest discover -s tools -p "test_*.py"
elif [[ "${1:-}" == "bench" ]]; then
    shift
    ./build/shapeshifter_tests "[bench]" "$@"
else
    ./build/shapeshifter "$@"
fi
