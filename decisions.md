# Decisions Log

**Last updated:** 2026-05-08T18:08:07Z

## 2026-05-08: Shape Geometry Audit Post-Cleanup

### Decision
Keep shape_vertices.h CIRCLE table; schedule HEXAGON/SQUARE/TRIANGLE arrays for removal in future cleanup.

### Rationale
- CIRCLE table actively used by app/systems/game_render_system.cpp draw_floor_rings()
- Annulus triangles rendered via raylib 2D APIs inside 3D camera path cannot use direct raylib calls
- HEXAGON/SQUARE/TRIANGLE arrays only referenced in tests/benchmarks; removable without breaking game logic

### Owner
Architecture Review (Keaton, Keyser)

### Status
✅ Approved. No immediate action.
