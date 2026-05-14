#!/usr/bin/env bash
# Local pre-PR validation for the WebAssembly build (issue #1080).
#
# Wraps tests/wasm_runtime_smoke.cjs (the same Playwright harness CI uses) so
# developers can validate that the WASM build boots, walks Title -> LevelSelect
# -> Tutorial -> Playing, and stays responsive in a real browser before opening
# a PR. Mirrors the steps performed by .github/workflows/ci-wasm.yml.
set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

BUILD_DIR="${WASM_BUILD_DIR:-build-web}"
SMOKE_SCRIPT="tests/wasm_runtime_smoke.cjs"
PLAYWRIGHT_VERSION="${PLAYWRIGHT_CORE_VERSION:-1.55.0}"
REQUIRED_ASSETS=(index.html index.js index.wasm index.data sw.js)

# ── Build artifact checks ────────────────────────────────────
if [[ ! -d "$BUILD_DIR" ]]; then
    cat >&2 <<EOF
error: WASM build directory '$BUILD_DIR' not found.

Build the WASM target first (requires emscripten and vcpkg):

  emcmake cmake -B $BUILD_DIR -S . \\
      -DCMAKE_BUILD_TYPE=Release \\
      -DCMAKE_TOOLCHAIN_FILE=\$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \\
      -DVCPKG_OVERLAY_PORTS=$REPO_ROOT/vcpkg-overlay \\
      -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=\$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \\
      -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \\
      -DSHAPESHIFTER_WASM_SMOKE_MARKERS=ON
  cmake --build $BUILD_DIR -- -j\$(nproc)
EOF
    exit 1
fi

for asset in "${REQUIRED_ASSETS[@]}"; do
    if [[ ! -s "$BUILD_DIR/$asset" ]]; then
        echo "error: required WASM artifact '$BUILD_DIR/$asset' missing or empty." >&2
        echo "       Re-run the emcmake build (see message from prior failure or README §WASM)." >&2
        exit 1
    fi
done

if ! grep -q '^SHAPESHIFTER_WASM_SMOKE_MARKERS:BOOL=ON' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    cat >&2 <<EOF
error: $BUILD_DIR was configured without -DSHAPESHIFTER_WASM_SMOKE_MARKERS=ON.

The smoke test reads document.title markers ([LevelSelect], [Playing],
[Lane:N]) emitted only when this flag is on, and will hang waiting for
them otherwise. Re-configure with the flag and rebuild.
EOF
    exit 1
fi

# ── Tooling checks ───────────────────────────────────────────
if ! command -v node >/dev/null 2>&1; then
    echo "error: node is required (Node.js 20+). Install node and retry." >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 is required to serve $BUILD_DIR. Install python3 and retry." >&2
    exit 1
fi

# ── Chromium auto-detect ─────────────────────────────────────
if [[ -z "${CHROMIUM_EXECUTABLE_PATH:-}" ]]; then
    candidates=(
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
        "/Applications/Google Chrome Beta.app/Contents/MacOS/Google Chrome Beta"
        "/Applications/Chromium.app/Contents/MacOS/Chromium"
        "$(command -v google-chrome 2>/dev/null || true)"
        "$(command -v google-chrome-stable 2>/dev/null || true)"
        "$(command -v chromium 2>/dev/null || true)"
        "$(command -v chromium-browser 2>/dev/null || true)"
    )
    for candidate in "${candidates[@]}"; do
        if [[ -n "$candidate" ]] && [[ -x "$candidate" ]]; then
            CHROMIUM_EXECUTABLE_PATH="$candidate"
            break
        fi
    done
fi

if [[ -z "${CHROMIUM_EXECUTABLE_PATH:-}" ]] || [[ ! -x "${CHROMIUM_EXECUTABLE_PATH}" ]]; then
    cat >&2 <<EOF
error: could not locate a Chrome/Chromium executable.

Install Google Chrome or Chromium, or set CHROMIUM_EXECUTABLE_PATH to the
absolute path of the binary, e.g.:

  CHROMIUM_EXECUTABLE_PATH=/path/to/chrome ./run.sh wasm-e2e
EOF
    exit 1
fi
export CHROMIUM_EXECUTABLE_PATH

# ── playwright-core install ──────────────────────────────────
if [[ ! -d node_modules/playwright-core ]]; then
    if ! command -v npm >/dev/null 2>&1; then
        echo "error: npm is required to install playwright-core (~$PLAYWRIGHT_VERSION). Install npm and retry." >&2
        exit 1
    fi
    echo "installing playwright-core@$PLAYWRIGHT_VERSION (no-save) into node_modules..."
    npm install --no-save --no-audit --no-fund "playwright-core@${PLAYWRIGHT_VERSION}" >/dev/null
fi

# ── Pick a free localhost port ───────────────────────────────
pick_free_port() {
    python3 - <<'PY'
import socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind(("127.0.0.1", 0))
    print(s.getsockname()[1])
PY
}
PORT="${WASM_E2E_PORT:-$(pick_free_port)}"

# ── Start serving build-web in the background ────────────────
SERVER_LOG="$(mktemp -t shapeshifter-wasm-e2e-server.XXXXXX.log)"
python3 -m http.server "$PORT" --directory "$BUILD_DIR" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$SERVER_LOG"
}
trap cleanup EXIT INT TERM

# Wait for HTTP availability
URL="http://127.0.0.1:${PORT}/index.html"
for _ in $(seq 1 40); do
    if curl -fsS "$URL" -o /dev/null 2>/dev/null; then
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "error: local HTTP server exited before serving $URL." >&2
        cat "$SERVER_LOG" >&2 || true
        exit 1
    fi
    sleep 0.25
done
if ! curl -fsS "$URL" -o /dev/null 2>/dev/null; then
    echo "error: timed out waiting for $URL to respond." >&2
    cat "$SERVER_LOG" >&2 || true
    exit 1
fi

echo "→ Chromium:    $CHROMIUM_EXECUTABLE_PATH"
echo "→ Build dir:   $BUILD_DIR"
echo "→ Serving at:  $URL"
echo "→ Running:     $SMOKE_SCRIPT"
echo

# ── Run the smoke harness ────────────────────────────────────
node "$SMOKE_SCRIPT" --build-dir "$BUILD_DIR" --seed-stale-build-cache "$URL"
