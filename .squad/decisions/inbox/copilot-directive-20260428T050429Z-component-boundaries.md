### 2026-04-28T05:04:29Z: User directive
**By:** yashasg (via Copilot)
**What:** `app/components/shape_vertices.h` is not needed because raylib has `Draw<shape>` helpers for this purpose. `app/components/settings.h` is not a component or an entity; it holds audio/settings state and should move out of `app/components/` into an audio/music/settings domain header.
**Why:** User request — captured for team memory
