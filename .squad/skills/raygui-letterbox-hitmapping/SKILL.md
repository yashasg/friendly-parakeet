# Skill: raygui-letterbox-hitmapping

## Problem
raygui controls render in virtual-space coordinates, but mouse coordinates remain window-space when the game uses letterboxing/scaling. Small controls can appear clickable but never register.

## Pattern
When rendering raygui in a virtual render target:
1. Read `ScreenTransform { offset_x, offset_y, scale }`.
2. Before any raygui calls, apply:
   - `SetMouseOffset(-offset_x, -offset_y)`
   - `SetMouseScale(1/scale, 1/scale)`
3. Render raygui controls.
4. Immediately restore defaults:
   - `SetMouseOffset(0, 0)`
   - `SetMouseScale(1, 1)`

## Guardrails
- Only scope the transform around UI rendering to avoid breaking gameplay/input systems that already convert coordinates.
- Keep a `scale > 0` guard.

## Applied in this repo
- `app/systems/ui_render_system.cpp`
