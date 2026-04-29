# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

## Learnings

- Title screen generated layout (`app/ui/generated/title_layout.h`) can remain read-only while the active controller (`app/ui/screen_controllers/title_screen_controller.cpp`) performs runtime overrides for text readability and control labeling.
- For centered hero text in raylib/raygui screens, use `DrawText` + `MeasureText` against `TITLE_LAYOUT_WIDTH` instead of relying on `GuiLabel` rectangles; this avoids clipping/alignment drift from undersized generated bounds.
- If generated button text is truncated ("SET"), keep the state wiring (`SettingsButtonPressed`) but relabel and resize in controller runtime (`"SETTINGS"` with explicit rectangle) so behavior stays intact and intent is clear.
- Pause screen (`app/ui/generated/paused_layout.h`) had the same default `GuiLabel` failure mode as pre-fix Song Complete (small, not centered labels); using a local centered-label helper with scoped `TEXT_SIZE` + `LABEL/TEXT_ALIGNMENT` fixes readability without touching button dispatch.
- Keep pause layout source and export aligned when text bounds change: update both `content/ui/screens/paused.rgl` and `app/ui/generated/paused_layout.h` together so future regen does not regress sizing.
- `app/archetypes/` is now legacy for player creation; canonical player factory lives in `app/entities/player_entity.{h,cpp}` and tests should include `entities/player_entity.h` directly (no shim header).

### 2026-04-29 — Title Screen UI Fix (first implementation, rejected)

**Session:** Title Screen UI Fix
**Scope:** Fix off-center title text and relocate settings button
**Verdict:** ❌ REJECTED

Centered `SHAPESHIFTER` and `TAP TO START` with manual `DrawText` + `MeasureText` calls; renamed top-left button from "SET" to "SETTINGS". Validation passed (build, tests).

However, preserved the runtime override block in controller and kept settings button at top-left (only renamed it). Redfoot's acceptance criteria required *removing* the override entirely and moving settings to bottom-right. This rejection triggered lockout per charter protocol.

**Assigned to:** Hockney (independent revision, not locked out)

## 2026-04-29T09:55:21Z — Pause Screen Text Fix Attempt (Rejected, Locked Out)

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Task:** Implement first pause-screen active-path fix per Redfoot's acceptance criteria.

**Approach:** Added `PausedLayout_DrawCenteredLabel()` helper with correct save/restore pattern (matching Song Complete), routed all three text labels through it, kept buttons/actions unchanged.

**Validation (passed):**
- Build: zero warnings (clang -Wall -Wextra -Werror)
- Tests: 2148 assertions, 756 test cases — all pass
- Structural quality: helper pattern correct, no legacy paths reintroduced

**Result:** ❌ REJECTED — Numeric AC values NOT met. Six individual AC items failed:

| Label | AC Requirement | Actual | Result |
|---|---|---|---|
| "PAUSED" font size | **56** | 48 | ❌ |
| "PAUSED" rect | ~(90, 420, 540, 80) | (160, 430, 400, 72) | ❌ |
| "TAP RESUME TO CONTINUE" font size | **24** | 22 | ❌ |
| "TAP RESUME TO CONTINUE" rect width | **≥540** | 500 | ❌ |
| "OR RETURN TO MAIN MENU" font size | **24** | 22 | ❌ |
| "OR RETURN TO MAIN MENU" rect width | **≥540** | 500 | ❌ |

**Kujan's correction guidance:** Update three label call-site arguments to exact values. Update `content/ui/screens/paused.rgl` geometry to match. Buttons remain untouched.

**Lockout:** Per reviewer lockout protocol: Keaton locked out for this revision cycle. Next reviser must be **different from Keaton**. Recommended: agent who landed Song Complete fix.

**Orchestration:** `.squad/orchestration-log/2026-04-29T09:55:21Z-keaton-pause.md`

## 2026-04-29: Startup/Shutdown Invalid Free Fix

**Task:** Diagnose `bash run.sh` aborting with `pointer being freed was not allocated` / `Abort trap: 6`.

**Root Cause:**
- The startup/shutdown smoke reproduced the crash with `./build/shapeshifter_startup_shutdown_smoke --frames 0`.
- ASAN narrowed it to `camera::unload_shape_meshes()`: the code called `UnloadShader(sm.material.shader)` and then `UnloadMaterial(sm.material)`.
- In raylib 5.5, `UnloadMaterial()` unloads `material.shader`; the separate `UnloadShader()` call double-freed the shader location array allocated by `LoadShaderFromMemory()`.

**Fix:**
- Removed the explicit `UnloadShader(sm.material.shader)` from `app/systems/camera_system.cpp`.
- Left mesh unloads explicit and let `UnloadMaterial()` own the shader/material teardown.

**Validation:**
- Before fix: `./build/shapeshifter_startup_shutdown_smoke --frames 0` aborted with malloc invalid free.
- ASAN before fix: double-free at `UnloadShader` via `UnloadMaterial`.
- After fix: startup/shutdown smoke passes with `--frames 0`.
- Full build and existing tests pass warning-free.

**Learning:**
- For raylib `Material` ownership, do not manually unload `material.shader` immediately before `UnloadMaterial()`; `UnloadMaterial()` already owns that shader teardown path.
- HUD shape input now uses direct rectangular `gameplay.rgl` slot bounds via raygui state (`GameplayHudLayout_*ButtonBounds`) with no expanded-circle acceptance filter; semantic shape `ButtonPressEvent` dispatch still happens in `gameplay_hud_apply_button_presses` during `Playing` only.
- ECS tap hit-testing surface was removed from runtime (`hit_test`, `active_tag`, `UIActiveCache`, and related components); raw swipe routing remains via `gesture_routing_handle_input` on Tier-1 `InputEvent` dispatch.

## 2026-04-29T23:54:05Z — Guard-Clause Refactor Implementation Complete

Orchestration log written. Guard-clause early-exit refactor successfully implemented across scoped files. Full test suite validation passed (2181 assertions / 777 tests). Team review (Kujan) approved no regressions.

Decision #169 captured in decisions.md.
