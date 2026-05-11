#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
./build.sh

levels=(1_stomper 2_drama 3_mental_corruption)
difficulties=(easy medium hard)
for level in "${levels[@]}"; do
  for difficulty in "${difficulties[@]}"; do
    ./build/shapeshifter --test-player bad --level "$level" --difficulty "$difficulty"
  done
done
