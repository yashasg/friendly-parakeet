### 2026-04-28T05:34:48Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/audio.h` and `app/components/music.h` should not exist as component-boundary wrappers when raylib already provides `Sound` and `Music` structs that serve the purpose.
**Why:** User request — captured to guide cleanup away from unnecessary audio/music component wrappers and toward raylib-native data.
