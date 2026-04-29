# Standalone Generated Files (NOT BUILT)

These are the original rguilayout exports with `main()` and `RAYGUI_IMPLEMENTATION`.

**DO NOT include these in CMakeLists.txt or any build target.**

They are archived here for reference only. They contain:
- `int main()` function with full raylib initialization and event loop
- `#define RAYGUI_IMPLEMENTATION` directive
- Standalone executables (would conflict with shapeshifter binary)

## Usage

These files are **reference artifacts only**. They demonstrate:
1. The drawing code structure (GuiLabel, GuiButton calls)
2. The initialization pattern (anchors, button state)
3. The control placement logic

## Embeddable Headers

For **build-safe** embeddable layouts, see `../title_layout.h` and other `*_layout.h` files in the parent directory.

Embeddable headers:
- ✅ Header-only (stb-style)
- ✅ No `main()` function
- ✅ No `RAYGUI_IMPLEMENTATION` (caller defines it)
- ✅ C/C++ compatible with `extern "C"` guards
- ✅ Safe to include in adapters

## Regeneration

To regenerate standalone files (for reference):

```bash
cd /Users/yashasgujjar/dev/bullethell

# Generate standalone C file
tools/rguilayout/rguilayout.app/Contents/MacOS/rguilayout \
    --input content/ui/screens/title.rgl \
    --output app/ui/generated/standalone/title.c

# Generate standalone H file (actually also a C file with main)
tools/rguilayout/rguilayout.app/Contents/MacOS/rguilayout \
    --input content/ui/screens/title.rgl \
    --output app/ui/generated/standalone/title.h
```

**Note:** The `.h` export is NOT a header - it's a full C file with `main()`.
