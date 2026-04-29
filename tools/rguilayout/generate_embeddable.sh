#!/bin/bash
# Generate embeddable rguilayout header manually from a standalone scratch export
# Usage: ./generate_embeddable.sh <screen_name>
# Example: ./generate_embeddable.sh title

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <screen_name>"
    echo "Example: $0 title"
    exit 1
fi

SCREEN_NAME="$1"
RGL_FILE="content/ui/screens/${SCREEN_NAME}.rgl"
SCRATCH_DIR="build/rguilayout-scratch"
STANDALONE_C="${SCRATCH_DIR}/${SCREEN_NAME}_temp.c"
OUTPUT_H="app/ui/generated/${SCREEN_NAME}_layout.h"

if [ ! -f "$RGL_FILE" ]; then
    echo "ERROR: $RGL_FILE not found"
    exit 1
fi

echo "==> Generating standalone C file (temp)..."
mkdir -p "$SCRATCH_DIR"
tools/rguilayout/rguilayout.app/Contents/MacOS/rguilayout \
    --input "$RGL_FILE" \
    --output "$STANDALONE_C"

echo ""
echo "==> ⚠️  MANUAL STEP REQUIRED ⚠️"
echo ""
echo "1. Open $STANDALONE_C"
echo "2. Extract the initialization code (lines ~40-45):"
echo "   - Vector2 Anchor01 = { 0, 0 };"
echo "   - bool <Button>Pressed = false;"
echo ""
echo "3. Extract the drawing code (lines ~70-75):"
echo "   - GuiLabel(...)"
echo "   - GuiButton(...)"
echo ""
echo "4. Create $OUTPUT_H using app/ui/generated/title_layout.h as template"
echo "5. Replace struct fields and function bodies with extracted code"
echo ""
echo "Standalone scratch C file is at: $STANDALONE_C"
echo "Do not commit scratch standalone exports."
echo "Target embeddable header: $OUTPUT_H"
echo ""
echo "See tools/rguilayout/INTEGRATION.md for full manual generation guide."
