### 2026-04-28T05:10:05Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/ui_layout_cache.h` is not needed. It stores fixed HUD/UI element positions derived from JSON, but those positions do not change after the JSON creates the respective UI elements; load once and render the same entities repeatedly instead of keeping a separate layout cache struct.
**Why:** User request — captured for team memory
