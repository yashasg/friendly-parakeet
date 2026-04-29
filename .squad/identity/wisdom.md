---
last_updated: 2026-04-29T05:03:05Z
---

# Team Wisdom

Reusable patterns and heuristics learned through work. NOT transcripts — each entry is a distilled, actionable insight.

## Patterns

**Pattern:** Portable generated C as the UI integration contract.

**Context:** When designing screen UI integrations with rGuiLayout or similar code generators. The generated `.h` file (portable C, upstream-owned, do-not-edit) is the architectural boundary. Game-specific screen controllers (downstream glue) consume the generated layout and translate input/state into game events. Do not copy rectangles, mirror state into ECS, or hand-edit generated exports.

**Implementation:** File layout: `app/ui/generated/<screen>_layout.h` (generated), `app/ui/screen_controllers/<screen>_screen_controller.cpp` (game glue). API shape: `Init()` / `State` / `Render()`. Event effects applied around render, not within generated code. Single RAYGUI_IMPLEMENTATION site to avoid ODR violations.

**Reference:** Decision #190 in `.squad/decisions.md` and spec `design-docs/rguilayout-portable-c-integration.md`.

**Pattern:** Template-based boilerplate abstraction for generated-code wiring.

**Context:** When wrapping generated code (rguilayout, protobuf, flatbuffers, etc.) requires identical runtime lifecycle (init, render, cleanup) across 3+ implementations. Extract shared boilerplate into a reusable template or trait before writing duplicate adapters/wrappers.

**Implementation:** Use C++17 non-type template parameters or CRTP to capture the generated code's function pointers and state type. Define shared lifecycle once; each adapter becomes a lightweight declaration (2–5 lines). Threshold: 3 files with identical structure = mandatory abstraction.

**Example:** Screen controller migration reduced 377 lines of adapter boilerplate (8 files × 47 lines each) to 50 lines of template + 8 lightweight declarations. Result: single maintenance point, compile-time enforcement, zero code duplication.

**Reference:** Screen controller migration (2026-04-29), Keaton's implementation in `app/ui/screen_controllers/screen_controller_base.h`.

<!-- Append entries below. Format: **Pattern:** description. **Context:** when it applies. -->
