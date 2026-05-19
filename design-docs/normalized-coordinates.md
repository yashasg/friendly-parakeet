# Normalized Coordinates — Feature Spec (Draft)

> **Status**: Parked — partially implemented for UI/layout; remaining gameplay
> math migration is captured for future implementation.
> Acceptance/checklist traceability: `docs/acceptance-traceability.md`.

## Problem

The codebase still has gameplay math tied to the 720×1280 virtual framebuffer:
lane positions, player/obstacle distances, speeds, collision margins, vision
ranges, and ring thresholds. HUD, scene, and overlay layout have already started
using normalized viewport constants (`*_N`) derived from the same virtual
framebuffer, so the remaining risk is the hybrid state: some systems are
resolution-relative while core gameplay logic is still pixel-locked.

## Constraint from Stakeholder

> "We can enforce an aspect ratio, but not a resolution."

The game should work on any resolution that matches the enforced aspect ratio.
All remaining gameplay math should use **normalized coordinates** (0.0–1.0) or
aspect-ratio-relative units instead of raw pixels.

## Scope (when we come back to this)

```
  Remaining gameplay pixels        Target (normalized)
  ─────────────────────            ─────────────────────
  PLAYER_Y    = 920.0f             PLAYER_Y    = 0.719  (920/1280)
  LANE_X[1]   = 360.0f             LANE_X[1]   = 0.500  (360/720)
  SPAWN_Y     = -120.0f            SPAWN_Y     = -0.094 (-120/1280)
  JUMP_HEIGHT = 160.0f             JUMP_HEIGHT = 0.125  (160/1280)
  PLAYER_SIZE = 64.0f              PLAYER_SIZE = 0.050  (64/1280)
  scroll_speed = 520 px/s          scroll_speed = 0.406 /s (520/1280)
  ...                              ...
```

## What Needs to Change

- [x] `constants.h` — HUD, scene, and overlay layout have normalized `*_N`
      companions for draw-time conversion to virtual pixels
- [ ] `constants.h` — remaining gameplay-space values become ratios
      (0.0–1.0 of screen H or W) or explicit aspect-relative units
- [ ] Collision margins, proximity-ring radii, vision ranges — all normalized
- [ ] Gameplay render systems — convert normalized gameplay coords at the
      rendering boundary
- [ ] Input system — divide touch coords by screen dimensions before gameplay
      decisions
- [ ] Test player — vision_range and all distance thresholds become normalized
- [ ] Ring zone thresholds — distance math uses normalized units
- [ ] Beatmap format — lane/position values become normalized or symbolic
- [ ] Aspect ratio enforcement — separate feature, define which ratio(s) supported

## Notes

- The existing `RenderTexture2D` virtual framebuffer (720×1280 → window blit)
  already handles display scaling, but core gameplay logic is still pixel-locked.
- HUD, scenes, and overlays use normalized viewport constants in `constants.h`
  and convert them back to virtual pixels at draw/update boundaries.
- Normalized coords decouple logic from display — the render layer is the
  only place that needs to know actual pixel dimensions.
- Aspect ratio spec TBD — likely 9:16 (portrait mobile) enforced.
