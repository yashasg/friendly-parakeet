#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  preflight_cfbundle_version.sh --current <int> [--previous <int>] [--tag-prefix <prefix>]

Behavior:
  - Fails if --current is not an integer.
  - Resolves previous build number from --previous, or from git tags matching <prefix><int>.
  - Fails if current is not strictly greater than previous.
EOF
}

current=""
previous=""
tag_prefix="ios-build-"

while [ $# -gt 0 ]; do
  case "$1" in
    --current)
      current="${2:-}"
      shift 2
      ;;
    --previous)
      previous="${2:-}"
      shift 2
      ;;
    --tag-prefix)
      tag_prefix="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "::error::Unknown argument: $1"
      usage
      exit 2
      ;;
  esac
done

if ! [[ "$current" =~ ^[0-9]+$ ]]; then
  echo "::error::CFBundleVersion must be a non-negative integer; got '$current'"
  exit 1
fi

resolve_previous_from_tags() {
  git tag --list "${tag_prefix}*" \
    | sed -nE "s/^${tag_prefix}([0-9]+)$/\1/p" \
    | sort -n \
    | tail -n 1
}

if [ -z "$previous" ]; then
  previous="$(resolve_previous_from_tags || true)"
fi

if [ -z "$previous" ]; then
  previous="0"
  echo "No prior ${tag_prefix}<n> tag or explicit previous build provided; using baseline ${previous}."
fi

if ! [[ "$previous" =~ ^[0-9]+$ ]]; then
  echo "::error::Previous build number must be an integer; got '$previous'"
  exit 1
fi

if [ "$current" -le "$previous" ]; then
  echo "::error::CFBundleVersion preflight failed: current=$current must be strictly greater than previous=$previous"
  exit 1
fi

echo "CFBundleVersion preflight passed: current=$current > previous=$previous"
