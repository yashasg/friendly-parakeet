#!/usr/bin/env bash
# Launch a local server for the beatmap editor and open it in the browser.
# Usage: ./serve.sh [port]

PORT="${1:-8042}"
DIR="$(cd "$(dirname "$0")" && pwd)"

echo "🎵 SHAPESHIFTER Beatmap Editor"
echo "   Serving from: $DIR"
echo "   URL: http://localhost:${PORT}/index.html"
echo ""
echo "   Press Ctrl+C to stop."
echo ""

# Open browser after a short delay
(sleep 1 && open "http://localhost:${PORT}/index.html" 2>/dev/null \
         || xdg-open "http://localhost:${PORT}/index.html" 2>/dev/null) &

# Serve from the repo root so content/constants.json resolves
cd "$DIR/../.." || exit 1
python3 -m http.server "$PORT" --directory . --bind 127.0.0.1 2>&1 | \
    sed "s|^|   |"
