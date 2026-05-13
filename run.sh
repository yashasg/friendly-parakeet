#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
./build.sh

if [[ "${1:-}" == "test" ]]; then
    shift
    ./build/shapeshifter_tests "$@"
    python3 -m unittest discover -s tools -p "test_*.py"
    if command -v node >/dev/null 2>&1; then
        (cd tools/beatmap-editor && node --test test/*.test.js)
        node --test tests/service_worker_cache_policy.test.cjs
    else
        echo "SKIPPED: JavaScript tests (node executable not found)"
    fi
elif [[ "${1:-}" == "bench" ]]; then
    shift
    ./build/shapeshifter_tests "[bench]" "$@"
else
    ./build/shapeshifter "$@"
fi
