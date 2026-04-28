# Normalized Coordinates — Feature Spec (Draft)

> **Status**: Parked — captured for future implementation.

## Problem

The codebase currently uses fixed pixel values (720×1280) for all game logic:
positions, distances, speeds, collision margins, vision ranges, etc. This
ties gameplay to a single resolution and breaks on devices with different
screen sizes.

## Constraint from Stakeholder

> "We can enforce an aspect ratio, but not a resolution."

The game should work on any resolution that matches the enforced aspect ratio.
All gameplay math should use **normalized coordinates** (0.0–1.0) or
aspect-ratio-relative units instead of raw pixels.

## Scope (when we come back to this)

```
  Current (pixels)                 Target (normalized)
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

- [ ] `constants.h` — all values become ratios (0.0–1.0 of screen H or W)
- [ ] Collision margins, proximity-ring radii, vision ranges — all normalized
- [ ] Render system — multiply normalized coords by actual screen dimensions
- [ ] Input system — divide touch coords by screen dimensions
- [ ] Test player — vision_range and all distance thresholds become normalized
- [ ] Ring zone thresholds — distance math uses normalized units
- [ ] Beatmap format — lane/position values become normalized or symbolic
- [ ] Aspect ratio enforcement — separate feature, define which ratio(s) supported

## Notes

- The existing `RenderTexture2D` virtual framebuffer (720×1280 → window blit)
  already handles display scaling, but game LOGIC is still pixel-locked.
- Normalized coords decouple logic from display — the render layer is the
  only place that needs to know actual pixel dimensions.
- Aspect ratio spec TBD — likely 9:16 (portrait mobile) enforced.
