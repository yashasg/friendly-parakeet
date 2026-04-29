# SKILL: rGuiLayout Title Runtime Override

## When to use
When a generated rGuiLayout header is intentionally read-only, but exported title-label/button rectangles are visually wrong in production (clipped text, truncated labels).

## Pattern
1. Keep generated header untouched (`app/ui/generated/*.h`).
2. In the screen controller (`app/ui/screen_controllers/*_controller.cpp`), skip generated render for affected controls.
3. Render hero text with `DrawText` + `MeasureText` for true horizontal centering across `*_LAYOUT_WIDTH`.
4. Keep generated state fields (e.g., `SettingsButtonPressed`) and only change runtime label/rectangle values.
5. Restore any temporary GUI style overrides to avoid cross-screen bleed.

## Why
- Preserves generator ownership boundary.
- Fixes UX immediately without regeneration churn.
- Avoids `GuiLabel` clipping behavior from undersized generated rectangles.
