---
name: "raygui-centered-label-helper"
description: "Use scoped style helper to fix generated GuiLabel readability/alignment without touching button behavior."
domain: "ui-rendering"
confidence: "high"
source: "observed"
---

## Context
Use this when a generated raygui layout is active in runtime, but labels render too small or left-aligned due to default `GuiLabel` style and tight rectangles.

## Patterns
- Add a small helper in the generated layout header that:
  - snapshots `DEFAULT/TEXT_SIZE` and `LABEL/TEXT_ALIGNMENT`
  - sets desired size + `TEXT_ALIGN_CENTER`
  - calls `GuiLabel`
  - restores styles immediately
- Call the helper only for affected labels; leave `GuiButton` wiring and pressed-state fields unchanged.
- When review asks for exact geometry/size corrections, update only label call-site arguments (not button controls or controller dispatch) and mirror those bounds in the `.rgl` source immediately.
- Mirror any bound updates back into the `.rgl` source so regeneration keeps the fix.

## Examples
- `app/ui/generated/paused_layout.h`: `PausedLayout_DrawCenteredLabel(...)`
- `app/ui/generated/song_complete_layout.h`: `SongCompleteLayout_DrawCenteredLabel(...)`
- Source sync: `content/ui/screens/paused.rgl`

## Anti-Patterns
- Global `GuiSetStyle()` overrides around whole-screen render (style bleed across widgets/screens).
- Controller-only rectangle duplication when the generated layout is still the active text path.
