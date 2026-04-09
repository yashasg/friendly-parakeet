#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

./build.sh

if [[ "${1:-}" == "test" ]]; then
    shift
    ./build/shapeshifter_tests "$@"
elif [[ "${1:-}" == "bench" ]]; then
    shift
    ./build/shapeshifter_tests "[bench]" "$@"
else
    ./build/shapeshifter
fi
