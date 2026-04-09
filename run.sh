#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

cmake -B build -S .
cmake --build build

if [[ "${1:-}" == "test" ]]; then
    ./build/shapeshifter_tests "$@"
else
    ./build/shapeshifter
fi
