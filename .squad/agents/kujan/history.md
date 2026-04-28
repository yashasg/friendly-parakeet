# Kujan — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Reviewer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

<!-- Append learnings below -->

### 2026-04-26 — Review: issue #92 (same-beat beatmap validation)

**Scope:** Ralph implementation plus Coordinator tightening for valid same-beat beatmap entries.

**Findings:**
- Initial pass needed ComboGate shape-stacking tightened because ComboGate + ShapeGate/SplitPath could be unresolvable at runtime.
- Coordinator rejected ComboGate stacked with any other same-beat shape-bearing obstacle and added tests.
- Same-beat shape conflicts now report the precise same-beat error instead of also tripping the shape-gap rule.
- Focused `[validate],[parse],[load],[beat_scheduler]` tests passed: 191 assertions / 91 cases.
- Full native non-benchmark suite passed: 2582 assertions / 782 cases.

**Verdict:** APPROVED

### 2026-04-26 — Review: issue #99 (Energy Bar tuning)

**Scope:** Game Designer target plus Coordinator implementation for TestFlight survivability.

**Findings:**
- Initial review caught a stale `game.md` forgiveness table still using old 20%/10% values.
- Coordinator fixed the table to 12%/6% values and 8-miss/9-miss target.
- Constants, tests, and `energy-bar.md` are internally consistent.
- Focused `[energy],[death_model],[scoring]` tests passed: 129 assertions / 73 cases.
- Full native non-benchmark suite passed after the tuning change: 2586 assertions / 785 cases.

**Verdict:** APPROVED

### 2026-04-26 — Review: issue #111 (session log MISS)

**Scope:** Coordinator regression coverage for `session_log_on_scored()` after earlier production fix.

**Findings:**
- `MissTag` is correctly used as the session-log MISS authority.
- Added fatal and non-fatal fixtures that write/read an actual `SessionLog` file.
- Both fixtures assert `result=MISS` and no `result=CLEAR`.
- Focused `[session_logger],[test_player]` tests passed: 18 assertions / 14 cases.
- Full native non-benchmark suite passed: 2592 assertions / 787 cases.

**Verdict:** APPROVED

### 2026-04-26 — Review: issue #114 (SongResults total_notes)

**Scope:** Coordinator regression coverage for total note counts in shipped play sessions.

**Findings:**
- Production logic sets `SongResults.total_notes` from loaded beatmap size.
- Added all shipped song/difficulty coverage through `setup_play_session()`.
- Added UI source resolver assertion for Song Complete stats.
- Focused `[play_session],[song_results],[high_score]` tests passed: 80 assertions / 17 cases.
- Full native non-benchmark suite passed: 2619 assertions / 788 cases.

**Verdict:** APPROVED

### 2026-04-26 — First Diagnostics Pass

**Scope:** Full repo scan (app/, tests/, design-docs/, CI, content/beatmaps)

**Structural patterns to remember:**
- `player_input_system.cpp` only wires Direction::Left/Right. Direction::Up/Down (jump/slide) are explicitly "disabled" — tests say so. Any beatmap work must avoid LowBar/HighBar until IC-01 is resolved.
- `ui_render_system.cpp` dispatches on three element types only: `text`, `button`, `shape`. New element types in JSON (e.g. `stat_table`) are silently dropped. Always check the dispatch table before adding JSON-driven UI elements.
- `SongResults` is populated but never rendered. The struct exists as future scaffolding; `total_notes` is never written.
- `COLLISION_MARGIN = 40.0f` is a dual local constexpr in `collision_system.cpp` and `beat_scheduler_system.cpp` — must stay in sync or rhythm timing breaks.
- High score is in-memory only (`ScoreState.high_score`). No persistence.
- Freeplay random spawner (`obstacle_spawn_system`) is effectively dormant: `song.playing` is always true after level select, causing the early-return bypass. But `std::rand()` is unseeded if it ever activates.
- `APPROACH_DIST = 1040.0f` is duplicated in `constants.h` and local to `song_state_compute_derived` in `song_state.h`.
- Architecture doc `LANE_X` values are stale: doc says {180, 360, 540}, code is {60, 360, 660}.
- MSVC `/W4 /WX` is stated policy but missing from CMake's `shapeshifter_warnings` interface target.
- No intentional pause trigger for touch-only players (mobile = target platform). Pause only fires on window focus loss.
- FTUE/tutorial fully absent despite `game-flow.md` specification.

**IC count:** 13 issues filed (4 release-blocking, 5 pre-TestFlight, 4 v1.1)

### 2026-04-26 — Review: issue #125 (LowBar/HighBar pipeline)

**Scope:** Rabin impl + Baer tests for shipping low_bar/high_bar on hard difficulty.

**Stale learning corrected:**
- My earlier note "Direction::Up/Down disabled, avoid LowBar/HighBar until IC-01" is now **wrong**.
  `player_input_system.cpp:72-80` handles both directions; `input_system.cpp:79-80` maps W/S/arrows
  and swipe-up/down. IC-01 must have been resolved between my first scan and this review.

**Findings:**
- Full runtime chain is intact: loader parses → scheduler spawns with RequiredVAction → collision resolves.
- Hard beatmaps: all 3 pass bar coverage (1_stomper: 1L/3H, 2_drama: 2L/2H, 3_mental: 7L/7H).
- Easy/medium: zero contamination confirmed by direct kind check.
- All 2357 test assertions pass, including 10 new `[low_high_bar]` tests.
- `validate_beatmap_bars.py --difficulty easy` misleadingly reports FAIL for correct easy beatmaps
  that intentionally omit bars. Non-blocking cosmetic issue with script scope.

**Verdict:** APPROVED

### 2026-04-27 — Review: issue #134 (min_shape_change_gap violations in shipped beatmaps)

**Scope:** Rabin generator/content fix + Baer C++ regression test.

**Verification performed:**
- `python3 tools/check_shape_change_gap.py`: all 9 beatmap×difficulty combos pass (exits 0).
- `python3 tools/check_bar_coverage.py`: #125 bar coverage intact (stomper 1/3, drama 2/2, mental 7/7).
- No empty difficulties; beat counts 100–206 across all three songs.
- `test_shipped_beatmap_shape_gap.cpp` wired into `shapeshifter_tests` via CMake build artifacts.
- All 2360 assertions pass (743 test cases) — 3 more than pre-#134 baseline of 2357.
- CI runs `./build/shapeshifter_tests "~[bench]"` on every push; new `[shipped_beatmaps]` tag included — future violations will fail CI.

**Pipeline ordering note:** `clean_shape_change_gap` at position 649 runs before final `clean_two_lane_jumps` at 650. Since `clean_two_lane_jumps` doesn't mutate shapes this is safe, but it's worth watching if that pass is ever extended.

**Non-blocking note:** `check_shape_change_gap.py` is not wired into any CI yaml step — only the C++ test gates PRs. Python script is a dev convenience tool. Acceptable; C++ coverage is authoritative.

### 2026-04-26 — Session closure: issue #134 review approved

**Scope:** Rabin generator/content fix + Baer C++ regression test for shipped beatmap min_shape_change_gap enforcement.

**Verification performed:**
- `python3 tools/check_shape_change_gap.py`: all 9 beatmap×difficulty combos pass (exits 0)
- `python3 tools/check_bar_coverage.py`: #125 bar coverage intact (stomper 1/3, drama 2/2, mental 7/7)
- No empty difficulties; beat counts 100–206 across all three songs
- `test_shipped_beatmap_shape_gap.cpp` wired into `shapeshifter_tests` via CMake build artifacts
- All 2360 assertions pass (725 test cases) — 3 new tests from #134 baseline
- CI runs `./build/shapeshifter_tests "~[bench]"` on every push; new `[shipped_beatmaps]` tag included — future violations fail CI

**Pipeline ordering:** `clean_shape_change_gap` at position 649 runs before final `clean_two_lane_jumps` at 650. Safe as `clean_two_lane_jumps` doesn't mutate shapes; worth watching if that pass is extended.

**Non-blocking note:** `check_shape_change_gap.py` not wired into CI yaml — only C++ test gates PRs. Python script is dev convenience tool. Acceptable; C++ coverage is authoritative.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T07:03:05Z-kujan.md`
**Session log:** `.squad/log/2026-04-26T07:03:05Z-issue-134-shape-gap.md`
**Decision merged** to `.squad/decisions.md` under "#134 — Enforce min_shape_change_gap in Shipped Beatmaps"
**Verdict:** APPROVED for merge

**GitHub comment:** Posted approval to issue #134

### 2026-04-27 — Review: issue #135 (easy variety / medium LanePush ramp)

**Scope:** Rabin implementation (level_designer.py, beatmaps) + Baer tests (validate_difficulty_ramp.py, test_shipped_beatmap_difficulty_ramp.cpp).

**Verdict:** REJECTED

**Blocking finding #1 — Easy has lane_push: violates #125 contract and Saul's design target**
- Confirmed in beatmaps: 1_stomper 1.6%, 2_drama 4.1%, 3_mental_corruption 3.8% lane_push on easy.
- `apply_lanepush_ramp` explicitly injects lane_push into easy (back half, ≤4 pushes).
- `decisions.md` #125 (APPROVED): "easy = shape_gate" is the settled contract.
- Saul's design target for #135: "Easy gets variety inside the shape_gate space only and must NOT acquire lane_push or bars (#125 contract)."
- Code comment falsely claims "Saul's design constraints were not finalised at implementation time."

**Blocking finding #2 — Tests don't guard the #125 easy=shape_gate contract**
- Easy tests check shape variety (3 distinct shapes, ≤65% dominance) only.
- No test asserts easy has zero lane_push. The contract violation from finding #1 passes all tests silently.

**What passes:**
- Medium LanePush in-bounds (9.3–19.5%), max consecutive ≤3 ✓
- #125 bar coverage intact (hard only) ✓
- #134 min_shape_change_gap intact ✓
- C++ tests compile and execute clean ✓

**Revision ownership (per locked-out rules):**
- Implementation fix (level_designer.py, beatmaps): Rabin locked out → McManus (or coordinator to route)
- Test fix (add easy zero-lane_push assertion): Baer locked out → Verbal

### 2026-04-27 — Review: issue #135 revision (McManus impl + Verbal tests)

**Scope:** McManus level_designer.py + beatmap regeneration; Verbal validate_difficulty_ramp.py + C++ test.

**Verdict:** APPROVED

**Verification performed:**
- `LANEPUSH_RAMP["easy"] = None` and `DIFFICULTY_KINDS["easy"] = {"shape_gate"}` confirmed in level_designer.py — easy injection bypassed at generation time.
- Direct beatmap JSON inspection: all 3 songs easy difficulty = 100% shape_gate. Zero lane_push/low_bar/high_bar.
- Easy shape variety: all 3 songs have 3 distinct shapes, dominant ≤65% (drama square exactly 60%).
- Medium lane_push: 9.3–19.5%, all within [5%, 25%]. start_progress 0.30 respected (2_drama first push at beat 150 = 32.1% of song).
- #125 bar coverage intact: stomper 1/3, drama 2/2, mental 7/7.
- #134 min_shape_change_gap: all 9 combos pass.
- `validate_difficulty_ramp.py` exits 0; new `check_easy_shape_gate_only()` + `EASY_BANNED_KINDS` wired.
- `[shape_gate_only]` C++ test asserts zero banned kinds for easy; all 2366 assertions pass (730 test cases).

**Non-blocking gap:** C++ medium test validates pct and consecutive but NOT start_progress=0.30 boundary. Generator enforces it; Python validator doesn't re-check it either. Future hardening: add start_progress C++ assertion.

**Learning:** When reviewing regenerated content, always inspect JSON directly by kind-count per difficulty — do not trust only test pass/fail. In the prior #135 rejection this would have caught the issue immediately.

---

### 2026-04-28 — Review: issue #137 (offset semantics / off-beat collision fix)

**Scope:** Fenster implementation (level_designer.py, beat_map.h, beat_scheduler_system.cpp, validate_beatmap_offset.py, beatmap regeneration) + Baer tests (test_beat_scheduler_offset.cpp, validate_offset_semantics.py).

**Verdict:** APPROVED

**Verification performed:**
- `build_beatmap()` anchors `offset = beats[anchor_idx] - anchor_idx * beat_period` where `anchor_idx = min(authored beat indices across all difficulties)` — NOT `beats[0]`.
- `beat_map.h` comment explicitly documents semantics: offset = audio time of beat_index=0; scheduler formula = `offset + beat_index * beat_period`.
- `beat_scheduler_system.cpp` comment matches; no formula discrepancy.
- `validate_beatmap_offset.py`: reconstructs expected offset from analysis and checks within 2ms — exits 0 for all 3 shipped beatmaps.
- `validate_offset_semantics.py`: 7-suite pipeline-side validation including corrected-offset drift test — exits 0.
- C++ tests: 9 tests tagged `[beat_scheduler][offset][issue137]`, pin the linear formula, non-zero first-beat-index guard, global-shift property, BPM-scaling guard — all 2392 assertions pass (757 test cases).
- #125 (bars): PASSED. #135 (difficulty ramp + easy=shape_gate): PASSED. All beatmap offsets valid.
- Stomper offset changed 2.270→2.269 (1ms); drama/mental unchanged — consistent with Rabin's report.

**Learning:** Offset anchoring bugs are invisible to test-pass/fail alone; must cross-check expected_offset against analysis `beats[anchor_idx]` directly in validation tooling. `validate_beatmap_offset.py` now closes this gap for regression.

## 2026-04-27 — Issue #137 Complete: Review Gate APPROVED

- **Verdict:** ✅ APPROVED for merge. Issue #137 (offset semantics & off-beat collision fix) is complete.
- **Full chain verified:** Semantics defined (beat_map.h + level_designer.py), runtime consistent (beat_scheduler_system.cpp), validation coverage (validate_beatmap_offset.py + validate_offset_semantics.py + 9 C++ tests).
- **Content gates:** All pass — #125 (low_bar/high_bar), #134 (shape-gap), #135 (difficulty-ramp), #137 (offset).
- **Assertion count:** 2392 assertions, 757 test cases, 100% pass.
- **Content delta:** Stomper 1ms, drama/mental zero — minimal gameplay impact.
- **Blocking issues:** None. Non-blocking notes: None.
- **Routing:** GitHub comment posted to issue #137; ready for merge.
- **Decisions merged:** `.squad/decisions/decisions.md` includes Kujan's review decision + approval rationale.

---

### 2026-04-27 — Gate Review: ENUM_MACRO proposal (no PR, design-time gate)

**Scope:** User-proposed global replacement of all `enum class` types using a fixed-arity C preprocessor macro.

**Proposed macro:**
```c
#define ENUM_MACRO(name, v1, v2, v3, v4, v5, v6, v7)\
    enum name { v1, v2, v3, v4, v5, v6, v7};\
    const char *name##Strings[] = { #v1, #v2, #v3, #v4, #v5, #v6, #v7};
```

**Verdict: ❌ REJECTED — exact macro is globally incompatible. Do not implement.**

---

#### Blocking Issue #1 — Fixed arity (exactly 7) matches zero enums in this codebase

Surveyed all 22 enum types in `app/`. Enumerator counts:

| Enum | Count | Verdict |
|---|---|---|
| `SFX` | 11 | Over 7 — cannot fit |
| `HapticEvent` | 13 | Over 7 — cannot fit |
| `ObstacleKind` | 8 | Over 7 — cannot fit |
| `MenuActionKind` | 7 | Exactly 7 — only one that fits |
| `GamePhase`, `ActiveScreen` | 6 | Under 7 — would need dummy padding |
| `BurnoutZone`, `EndScreenChoice`, `Direction`, `Layer`, `WindowPhase`, `DeathCause`, `VMode`, `TextAlign`, `MeshType`, `Shape`, `TimingTier`, `UIElementType` | 2–5 | Under 7 — would need dummy padding |

Of 22 enums, exactly 1 (`MenuActionKind`) has the required arity. All others need either padding with sentinel dummies (polluting the enum body) or cannot fit at all. This makes global application impossible without extensive enum value surgery.

#### Blocking Issue #2 — `enum name` vs `enum class name : uint8_t` — ABI and type safety destroyed

The macro generates an unscoped `enum`, not `enum class`. Every component enum in this codebase declares `enum class T : uint8_t` for three reasons:
1. **Scoping:** Uses like `GamePhase::Playing`, `Shape::Circle`, `BurnoutZone::Dead` are pervasive across all systems. Replacing with unscoped enums removes the `T::` qualifier requirement — existing code would still compile but newly written code loses the guard and name collision risk rises to compile-time.
2. **Underlying type:** `uint8_t` backing is the basis for component struct sizing. `BurnoutState`, `PlayerShape`, `ShapeWindow`, `Obstacle`, `DrawLayer` and others embed these enums directly as struct members. Removing `: uint8_t` forces the compiler to default to `int` (4 bytes on all targets), silently changing struct sizes, invalidating ECS component memory layouts, and breaking serialization round-trips (beatmap JSON parsing casts to `uint8_t`-backed enums).
3. **No implicit int conversion:** `enum class` prevents silent integer promotion. Loss of this property under `-Wall -Wextra -Werror` would likely produce new warnings or comparisons that the policy treats as errors.

`FontSize : int` is the only enum using `int` intentionally; the rest are `uint8_t` by design.

#### Blocking Issue #3 — String array ODR violation — linker failure on every multi-TU build

`const char *name##Strings[]` without `static`, `inline`, or `constexpr` in a header = external linkage definition. All component headers are included in multiple `.cpp` translation units. This would produce a multiple-definition linker error on every enum placed in a header. C++17 `inline` variables or `static constexpr` would fix this specific defect, but the fix is not part of the proposed macro.

#### Blocking Issue #4 — Unscoped enumerator name collisions — compile failures

Unscoped enums inject all enumerator names directly into the enclosing namespace. This codebase has:
- `None` in `BurnoutZone`, `EndScreenChoice`, `InputSource`, `DeathCause` — four definitions of `::None`.
- `Shape` as both an enumerator in `MeshType` and a standalone type in `player.h`.
- `Left`, `Right` in `Direction` and `TextAlign`.
- `Text`, `Button` in `UIElementType` conflict with common identifier usage.
- `Restart` in `EndScreenChoice` and `MenuActionKind` simultaneously.

Promotion to global namespace would produce "redefinition of enumerator" compile errors on any translation unit that includes two affected headers.

#### Blocking Issue #5 — Forward declarations broken

Three headers forward-declare scoped enums with explicit underlying type:
- `player.h:29` — `enum class WindowPhase : uint8_t;`
- `game_loop.h:5` — `enum class TestPlayerSkill : uint8_t;`
- `all_systems.h:15` — `enum class TestPlayerSkill : uint8_t;`

A forward declaration of an unscoped `enum` without underlying type is not valid C++17/20; the underlying type must be specified. Replacing definitions with the proposed macro while keeping these forward declarations as-is = compile error. Removing the forward declarations requires understanding every translation unit's include order. This is not a cosmetic change — it's a dependency graph surgery task.

#### Blocking Issue #6 — Explicit enumerator values lost

Several enums use explicit ordinal values intentional to their semantics:
- `GamePhase::Title = 0` through `SongComplete = 5` — used as phase index/mask.
- `EndScreenChoice::None = 0, Restart = 1, LevelSelect = 2, MainMenu = 3`.
- `TestPlayerSkill::Pro = 0, Good = 1, Bad = 2` — indexed into `SKILL_TABLE[]`.
- `SFX::COUNT` — used as `AudioQueue` array size sentinel; must be last.
- `HapticEvent` members — comment documentation tied to ordinal position.

The macro syntax `v1, v2, ...` provides no mechanism for `v = value` or a COUNT sentinel. These would have to be reconstituted as the last argument or removed, both of which change meaning.

#### Non-blocking observation — Codebase already has a better pattern

`SHAPE_LIST(X)`, `OBSTACLE_KIND_LIST(X)`, `TIMING_TIER_LIST(X)` implement variadic X-macros that already generate `enum class T : uint8_t` plus `ToString()` via switch. This is the established, zero-warning, type-safe, ODR-clean pattern for enum-to-string in this codebase. Any effort to add string support to remaining enums should extend this pattern, not replace it with an inferior fixed-arity macro.

---

#### Acceptance criteria for any implementation (if user proceeds despite rejection)

If the user explicitly wants to proceed, these are the minimum gates before any PR can be accepted:

1. **Must use `enum class T : underlying_type`** — never `enum T`. No ABI regression.
2. **String array must be `inline constexpr` or `static constexpr`** — not raw external-linkage const.
3. **Must support arbitrary arity** — variadic macro or X-macro pattern. Exactly-7 is never acceptable in this codebase.
4. **Must support explicit `name = value` syntax** — required for COUNT sentinels and ordinal-indexed enums.
5. **Must not touch forward-declared enums** — `WindowPhase`, `TestPlayerSkill` forward declarations must remain valid.
6. **Zero-warning CI gate must pass** on all three targets (clang-macOS, MSVC, Emscripten).
7. **Proof of no namespace collision** — build a TU that includes all affected headers simultaneously.

#### Suggested revision owner

If the user wants string conversion extended to remaining enums, route to **McManus** (owns enum-adjacent component/system work) with **Keaton** (C++ patterns, zero-warning policy) for the safe X-macro expansion approach. The implementation is a 2-3 hour task under the existing pattern. Do not author a new macro from scratch when `SHAPE_LIST` already demonstrates the correct idiom.

**This gate closes the request. The exact macro must not be implemented.**

### 2026-04-27 — Review: issue #138 (56–64 beat silent gaps in shipped beatmaps)

**Scope:** Ralph + Coordinator fix for mid-song silent gaps exceeding per-difficulty thresholds (Easy ≤40, Medium ≤32, Hard ≤30 beats).

**Verdict:** ✅ APPROVED

**Verification performed:**
- Ralph's `fill_large_gaps()` pass runs post-all-cleaners; conservative (fills only violations)
- Gap-filling distributes across lanes; preserves musical density
- Beatmaps regenerated: all max-gap violations resolved within per-difficulty limits
- Stomper: easy 32, medium 9, hard 13; Drama: easy 16, medium 8, hard 10; Mental: easy 33, medium 31, hard 23
- Validator `validate_max_beat_gap.py` passes (exit 0) for all 3 beatmaps
- C++ regression test (`test_shipped_beatmap_max_gap.cpp`) wired into CI; 2 test cases, 3 assertions each
- All #125 (bars), #134 (shape-gap), #135 (difficulty-ramp) contracts intact
- Full test suite: 2395 assertions / 759 test cases pass

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #138 with final metrics.

### 2026-04-28 — Review: issue #141 (gap-one readability in shipped beatmaps)

**Scope:** Ralph initial impl + Coordinator finalization (Catch2 alignment, content regeneration) for gap=1 readability enforcement across 9 beatmap×difficulty combos.

**Verdict:** ✅ APPROVED

**Verification performed:**
- `clean_gap_one_readability()` logic enforces difficulty-stratified policy:
  - Easy: zero gap=1 (no consecutive single-beat obstacles)
  - Medium: gap=1 ≤1 per run; only post-30% progress; identical shape_gate same shape+lane; away from LanePush/bar ≥2 beats
  - Hard: gap=1 ≤2 per run; from beat 11+; same family/neighbour rules
- Python validator `validate_gap_one_readability.py` mirrors policy definitions; exits 0 for all 9 combos
- C++ regression test `test_shipped_beatmap_gap_one_readability.cpp` wired via beat_map_loader; 2 test cases, 3 assertions
- All beatmaps regenerated and validated:
  - Stomper, Drama, Mental_corruption × Easy/Medium/Hard
  - All pass: no gap=1 violations detected
- No regression to #125 (bars), #134 (shape-gap), #135 (difficulty-ramp), #137 (offset), #138 (max-gap)
- Full test suite: 2398 assertions / 761 test cases pass
- All 6 Python validators pass (gap_one_readability, max_beat_gap, difficulty_ramp, shape_change_gap, bar_coverage, offset_semantics)
- CMake build: zero warnings (native, MSVC, Emscripten flags verified)

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #141; ready for merge.

### 2026-04-26 — Review: issue #44 (dependency-review workflow trigger fix)

**Scope:** Ralph + Coordinator fix for `.github/workflows/dependency-review.yml` event trigger.

**Verdict:** ✅ APPROVED

**Verification performed:**
- Event trigger: `push` → `pull_request` ✓
- Action version: `actions/dependency-review-action@v4` preserved ✓
- Severity: `fail-on-severity: high` preserved ✓
- License allowlist: intact ✓
- Permissions: `contents: read` unchanged ✓
- YAML syntax: valid ✓

**Non-blocking notes:** None.

**Routing:** Issue #44 approved; ready for merge.

### 2026-04-26 — Review: issue #46 (release workflows)

**Scope:** Ralph + Coordinator implementation of GitHub release workflows for push-to-main and insider-branch releases.

**Verdict:** ✅ APPROVED

**Verification performed:**
- YAML syntax: valid (both workflows)
- Workflow structure: identical pattern (checkout/cache → deps → build → test → release)
- Release creation: explicit tag-skip, error propagation (no `|| echo` fallback)
- Artifact management: `build/shapeshifter` only (no test binaries)
- Build environment: VCPKG_ROOT set, Linux deps installed, cache configured
- Test gate: `./build/shapeshifter_tests "~[bench]"` excludes benchmarks correctly
- Sanity checks: build command correct, test command correct, no stub/TODO markers

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #46; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:44:55Z-kujan.md`

### 2026-04-26 — Review: issue #47 (squad CI/preview workflows)

**Scope:** Ralph implementation + Coordinator validation of `.github/workflows/squad-ci.yml` and `.github/workflows/squad-preview.yml` workflows.

**Verdict:** ✅ APPROVED

**Verification performed:**
- YAML syntax: valid (both workflows)
- Workflow triggers: squad-ci (push to dev, PR into dev), squad-preview (push to preview) ✓
- Build environment: Linux deps installed, VCPKG_ROOT set, cache configured ✓
- Build gate: `./build.sh` ✓
- Test gate: `./build/shapeshifter_tests "~[bench]"` excludes benchmarks ✓
- Artifact handling: binary uploaded on success, test reporter artifacts captured ✓
- Preview validation: confirms `build/shapeshifter` exists and non-empty ✓
- Pattern adherence: matches ci-linux.yml exactly ✓
- Sanity checks: no stub/TODO/`|| true`/`|| echo` patterns ✓

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #47; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:52:25Z-kujan.md`

### 2026-04-26 — Review: issue #48 (Windows LLVM pin)

**Scope:** Ralph + Coordinator fix for `.github/workflows/ci-windows.yml` and `README.md` Windows LLVM version pinning.

**Verdict:** ✅ APPROVED

**Verification performed:**
- Coordinator verified `llvm 20.1.4` Chocolatey package availability (safe, stable version)
- Pin format: `llvm --version=20.1.4` correct ✓
- No unsafe unpinned fallback present ✓
- Clang 20.x version assertion retained and valid ✓
- CC/CXX environment variables: clang, clang++ (correct) ✓
- Cache key: clang20 applicable ✓
- README Windows platform matrix: updated to document `Clang 20.1.4 (Chocolatey LLVM)` ✓
- Alignment with ci-{linux,macos,wasm}.yml Clang 20 baseline ✓
- `git diff --check` passed; YAML parse valid ✓

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #48; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T08:56:22Z-kujan.md`

### 2026-04-26 — Review: issue #50 (WASM deploy silent-fallback fix)

**Scope:** Ralph + Coordinator strengthening `.github/workflows/ci-wasm.yml` deploy-main and deploy-pr jobs with pre-copy artifact validation and elimination of silent fallback chains.

**Verification performed:**
- Pre-copy validation: index.html, index.js, index.wasm, index.data, sw.js all exist and non-empty ✓
- Copy commands: direct (no `|| true` patterns, no fallback chains) — fail cleanly on error ✓
- Post-copy validation: destination files validated non-empty before git add/commit/push ✓
- YAML syntax: Ruby/Psych parse valid ✓
- git diff --check: no trailing whitespace ✓
- Sanity checks: no stub/TODO/silent-fallback patterns ✓

**Verdict:** ✅ APPROVED

**Rationale:** Silent-fallback elimination and pre/post-copy validation eliminate deployment risk. Deploy jobs now fail fast with clear error reporting instead of silently succeeding with incomplete artifacts.

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #50; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:00:21Z-kujan.md`

### 2026-04-26 — Review: issue #54 (MSVC warning policy)

**Scope:** Ralph + Coordinator fix for CMake warning policy on MSVC.

**Verdict:** ✅ APPROVED

**Verification performed:**
- `shapeshifter_warnings` now includes MSVC `/W4` and `/WX` as separate generator-expression options
- Existing GNU/Clang/AppleClang `-Wall -Wextra -Werror` policy unchanged
- `shapeshifter_warnings` remains linked by `shapeshifter_lib`, `shapeshifter`, and `shapeshifter_tests`
- Third-party include directories remain `SYSTEM`
- No warning suppression flags added
- Coordinator resolved a validation blocker by linking non-logger utility sources into `shapeshifter_lib`
- Build passed for `shapeshifter` and `shapeshifter_tests`
- Full non-benchmark suite passed: 2404 assertions in 745 test cases

**Non-blocking notes:** Native linker still emits a pre-existing duplicate raylib library warning; unrelated to #54.

**Routing:** GitHub comment posted to issue #54; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:08:09Z-kujan.md`

### 2026-04-26 — Review: issue #56 (vcpkg overlay cache keys)

**Scope:** Ralph + Coordinator fix for CI cache invalidation when `vcpkg-overlay/**` changes.

**Verdict:** ✅ APPROVED

**Verification performed:**
- All vcpkg/CMake cache keys in platform CI include `vcpkg-overlay/**`
- Squad workflow cache keys in the current worktree include `vcpkg-overlay/**`
- No stale exact `hashFiles('CMakeLists.txt', 'vcpkg.json')` pattern remains
- YAML syntax valid for all `.github/workflows/*.yml`
- Platform/compiler cache prefixes and scoped restore keys preserved
- No broad fallback introduced

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #56; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:12:58Z-kujan.md`

### 2026-04-26 — Review: issue #59 (WASM NO_EXIT_RUNTIME)

**Scope:** Ralph + Coordinator fix for the browser build missing `-sNO_EXIT_RUNTIME=1`.

**Verdict:** ✅ APPROVED

**Verification performed:**
- `-sNO_EXIT_RUNTIME=1` is placed in the authoritative Emscripten `target_link_options(shapeshifter PRIVATE ...)` block
- The option is gated by `if(EMSCRIPTEN)` and does not affect native builds
- The option appears exactly once and is not duplicated in CI or shell scripts
- Existing WASM flags, including `-sDYNAMIC_EXECUTION=0`, remain unchanged
- README documents the `emscripten_set_main_loop()` callback and runtime-lifetime rationale
- Native configure/build/tests passed; targeted CMake/README sanity checks passed

**Non-blocking notes:** `emscripten_set_main_loop(..., simulate_infinite_loop=1)` already avoids the normal return path, but `NO_EXIT_RUNTIME=1` is correct defensive runtime-lifetime practice.

**Routing:** GitHub comment posted to issue #59; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:18:54Z-kujan.md`

### 2026-04-26 — Review: issue #62 (WASM CI tests)

**Scope:** Ralph + Coordinator fix for WASM CI missing test execution.

**Verdict:** ✅ APPROVED

**Verification performed:**
- `Catch2` is required for all platforms and `shapeshifter_tests` builds under Emscripten
- Native test discovery remains unchanged through `catch_discover_tests`
- WASM registers explicit CTest target `shapeshifter_tests_wasm`
- WASM test command runs via CMake cross-emulator or required Node fallback
- CI workflow runs `ctest --verbose --output-on-failure` after build and before artifact upload
- `-sNODERAWFS=1` is scoped to WASM test target only
- Emscripten `-fexceptions` is justified by existing JSON/settings/high-score try/catch paths
- Native tests passed: 2404 assertions in 745 test cases
- Real local WASM CTest passed: 2392 assertions in 733 test cases

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #62; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:37:50Z-kujan.md`

### 2026-04-26 — Review: issue #64 (shipped beatmap CI validation)

**Scope:** Ralph + Coordinator fix for shipped beatmap JSON files not being validated in CI.

**Verdict:** ✅ APPROVED

**Verification performed:**
- Runtime test discovers every `content/beatmaps/*_beatmap.json`
- Test covers `easy`, `medium`, and `hard` for each shipped beatmap
- Empty/missing content directory cannot silently pass
- Difficulty fallback cannot silently pass because loader errors must remain empty and `map.difficulty` must match the requested difficulty
- Schema validation reports all errors and asserts the validator boolean
- All platform CI workflows no longer ignore `content/**`
- Native tests, WASM CTest, and malformed-beatmap negative simulation passed

**Non-blocking notes:** `_analysis.json` files are intentionally excluded by the `*_beatmap.json` glob.

**Routing:** GitHub comment posted to issue #64; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:48:12Z-kujan.md`

### 2026-04-26 - Review: issue #66 (product scope contradiction)

**Scope:** Ralph + Coordinator documentation fix for TestFlight product-scope consistency.

**Verdict:** APPROVED

**Verification performed:**
- TestFlight primary mode is documented as Song-Driven Rhythm Bullet Hell
- README, GDD, and flow docs list Stomper, Drama, and Mental Corruption with easy/medium/hard difficulty selection
- Freeplay/random spawning is explicitly post-release/dev fallback only
- Master app flow routes Title -> Level Select -> Gameplay
- Transition section and timing constants match the Level Select flow
- Stale architecture endless-runner wording was removed

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #66; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T09:58:41Z-kujan.md`

### 2026-04-26 - Review: issue #67 (death model consistency)

**Scope:** Ralph + Coordinator fix for contradictory death model docs/tests/runtime behavior.

**Verdict:** APPROVED

**Verification performed:**
- Energy Bar is documented as the sole death authority
- Burnout Dead-zone is documented and tested as x5 scoring feedback, not instant GameOver
- Miss and Bad timing drains clamp depleted energy using `ENERGY_DEPLETED_EPSILON`
- `energy_system` owns the GameOver transition and stops song playback at depletion
- Focused death/energy tests and full native non-benchmark suite passed
- Epsilon sizing is safe relative to the observed float residue and smallest gameplay energy delta

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #67; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:15:16Z-kujan.md`

### 2026-04-26 - Review: issue #68 (platform scope lock)

**Scope:** Ralph + Coordinator documentation fix for TestFlight/App Store v1 platform scope.

**Verdict:** APPROVED after one revision

**Verification performed:**
- TestFlight and App Store v1 are documented as iOS-only customer-facing targets
- macOS/Linux/Windows/WebAssembly are documented as CI/dev validation surfaces only
- Platform-specific input, build/distribution, and QA requirements are documented
- `normalized-coordinates.md` now uses explicit iOS portrait reference target
- Initial review requested fixing duplicated `7.` in the GDD game-loop list
- Re-review confirmed the numbered list now runs 1-9 correctly

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #68; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:24:40Z-kujan.md`

### 2026-04-26 - Review: issue #71 (persistence backend policy)

**Scope:** Ralph + Coordinator documentation fix for per-platform persistence policy.

**Verdict:** APPROVED after one revision

**Verification performed:**
- High scores/settings are documented as JSON persistence through the shared filesystem abstraction.
- iOS/TestFlight/App Store v1 requires app-sandbox path resolution without claiming iCloud, Core Data, or NSUserDefaults support.
- Desktop and WASM are documented as CI/dev-only persistence surfaces.
- WASM docs avoid current IDBFS durability claims.
- FTUE run count is documented as a v1 requirement, not a current implementation.
- Initial review requested fixing the high-score JSON example to include the top-level `scores` wrapper.
- Re-review confirmed the example matches `high_score_persistence.cpp`.

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #71; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:38:40Z-kujan.md`

### 2026-04-26 - Review: issue #72 (LaneBlock/LanePush taxonomy)

**Scope:** Ralph + Coordinator code/docs/tooling fix for lane obstacle taxonomy drift.

**Verdict:** APPROVED

**Verification performed:**
- Shipping lane obstacles are documented and validated as `lane_push_left` / `lane_push_right`.
- `lane_block` / `LaneBlock` is legacy, deprecated, and non-shipping.
- Parser/runtime support for `lane_block` remains only for old fixtures/prototype compatibility.
- Shipping beatmap validation rejects `lane_block`.
- Editor and Python tooling reject or avoid `lane_block`.
- Shipped beatmaps contain no `lane_block` references.
- Coordinator validation included focused taxonomy tests, Python validators, and full native non-benchmark tests.

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #72; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T10:51:51Z-kujan.md`

### 2026-04-26 - Review: issue #73 (difficulty ownership)

**Scope:** Ralph + Coordinator code/docs/test fix for song-mode difficulty vs freeplay time-ramp ownership.

**Verdict:** APPROVED

**Verification performed:**
- Song-mode difficulty is authored chart selection (`easy`, `medium`, `hard`).
- Level Select labels map to runtime difficulty keys.
- `setup_play_session()` passes the selected difficulty key to beatmap loading.
- `difficulty_system()` and `obstacle_spawn_system()` bypass freeplay behavior while `SongState.playing == true`.
- Freeplay/random spawning owns the elapsed-time `DifficultyConfig` ramp.
- Stale speed-ramp/speed-bar current-game references were removed or scoped as removal notes.
- Coordinator validation included focused tests and the full native non-benchmark suite.

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #73; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T11:04:15Z-kujan.md`

### 2026-04-26 - Review: issue #74 (pause/resume audio sync)

**Scope:** Ralph + Coordinator code/docs/test fix for audio-driven pause/resume behavior.

**Verdict:** APPROVED after one requested change

**Verification performed:**
- Music pause/resume state is tracked through `MusicContext.paused`.
- `song_playback_system()` pauses music and freezes `song_time` outside `GamePhase::Playing`.
- Beat scheduling, cleanup/miss processing, lifetime timers, and particle gravity do not advance during pause.
- Resume is explicit, has no countdown/grace, and clears queued gameplay input.
- Focus loss while `Playing` enters `Paused`; focus regain does not auto-resume.
- Docs include TestFlight app interruption QA plus desktop/WASM focus-loss proxy behavior.

**Requested change:** Cleanup miss energy clamping needed to use `ENERGY_DEPLETED_EPSILON` instead of a raw zero comparison.

**Resolution:** Coordinator applied the clamp fix, added regression coverage, and revalidated focused/full native tests.

**Routing:** GitHub comment posted to issue #74; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T11:23:19Z-kujan.md`

### 2026-04-26 - Review: issue #76 (FTUE/tutorial)

**Scope:** Ralph + Coordinator implementation of minimal first-run FTUE with persisted completion.

**Verdict:** APPROVED

**Verification performed:**
- `SettingsState` persists `ftue_run_count`, defaults legacy settings to incomplete, rejects malformed types, and clamps 0..5.
- Title Confirm routes first-time users to Tutorial and completed users to Level Select.
- Tutorial START marks FTUE complete, saves settings, clears queued input, and transitions to Level Select.
- Tutorial hitbox matches the visible JSON START button.
- `GamePhase::Tutorial` / `ActiveScreen::Tutorial` are wired through navigation, music stop handling, and automated test-player flow.
- Docs accurately describe minimal TestFlight v1 FTUE and keep the full 5-run sequence as future expansion.
- Coordinator validation included JSON parse checks, focused tests, built tutorial asset copy check, and full native non-benchmark tests.

**Non-blocking notes:** None.

**Routing:** GitHub comment posted to issue #76; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T21:50:49Z-kujan.md`

### 2026-04-26 - Review: issue #82 (MISS semantics)

**Scope:** Ralph + Coordinator doc/test fix for MISS vs Energy Bar death semantics.

**Verdict:** APPROVED

**Verification performed:**
- Rhythm and Energy Bar docs consistently state that `EnergyState` is the sole death authority.
- MISS drains `ENERGY_DRAIN_MISS` and never directly requests GameOver.
- `energy_system()` transitions to GameOver only on zero/epsilon energy depletion.
- Cleanup/offscreen misses use the same MISS semantics and are documented with the current system-order caveat.
- Runtime behavior matches docs in collision, cleanup, scoring, energy, and passive lane-push paths.
- New death-model tests cover single MISS drain, deferred GameOver, timing recovery, Burnout Dead-zone behavior, depletion clamping, and cleanup/offscreen MISS behavior.

**Non-blocking notes:** A future explicit `TimingTier::Bad` energy-drain test would further document Bad-timing behavior.

**Routing:** GitHub comment posted to issue #82; ready for merge.

**Orchestration logged:** `.squad/orchestration-log/2026-04-26T21:58:33Z-kujan.md`

### 2026-04-27 — Diagnostic: fresh reviewer-gate sweep (#163-#165 equivalent: #236, #237, #238)

**Scope:** Full codebase diagnostic — app/, tests/, tools/, .github/workflows/, design-docs/, content/ui/. Checked issues #44–#162 for duplicates before filing.

**Method:**
- Read gameplay systems: scoring_system, collision_system, cleanup_system, energy_system, burnout_system, song_playback_system, player_input_system, player_movement_system, beat_scheduler_system, scroll_system, game_state_system, play_session
- Read components: song_state, scoring, settings, rhythm, audio
- Read UI pipeline: ui_render_system, ui_source_resolver, content/ui/screens/song_complete.json, game_over.json
- Read design specs: game.md, game-flow.md, architecture.md, feature-specs.md, decisions.md

**Issues filed:**

**#236 — `haptics_enabled` setting is non-functional (TestFlight)**
- `haptics_enabled` is persisted and toggled but zero gameplay/platform code reads it for haptic triggering
- game-flow.md Appendix B specifies 8 haptic events with patterns and intensities; none implemented
- Milestone: test-flight
- Owner: McManus (wiring) + Hockney (iOS backend)

**#237 — Song Complete/Game Over missing peak burnout and max chain (AppStore)**
- `SongResults.best_burnout` not in `resolve_ui_int_source` → cannot be referenced from UI JSON
- `SongResults.max_chain` IS in resolver but absent from song_complete.json and game_over.json
- Neither results screen shows the core Burnout metric, breaking the player feedback loop
- Milestone: AppStore
- Owner: Redfoot (UI) + Keyser (resolver)

**#238 — `SongResults.total_notes` inflated by passive LanePush beats (AppStore)**
- `total_notes = beatmap.beats.size()` includes LanePush entries
- Collision_system never increments P/G/O/B/M for LanePush (passive path, no timing grade)
- `perfect + good + ok + bad + miss < total_notes` in any medium/hard chart
- Existing test masks the bug (asserts total_notes == beatmap.beats.size())
- Milestone: AppStore
- Owner: Keyser (count fix) + Baer (test update)

**Duplicates skipped:**
- Reviewed all #44–#162; no filed issue covered haptics implementation, best_burnout display, or LanePush note count inflation
- BurnoutBank SFX for LanePush (audio side of #120) — same root cause as #120, intentionally not filed separately; fix for #120 should include audio skip

**Cross-domain patterns noted:**
- LanePush is a recurring "leaks through wrong code path" pattern: it appears in #91 (burnout meter), #95 (mid-transition drop), #120 (zero popup), and now #238 (total_notes). The common thread is LanePush being passive but processed through systems designed for active obstacles.
- Haptics and accessibility (reduce_motion) are both settings-only stubs with no gameplay wiring — likely the same gap in the platform abstraction layer.


---

### 2026 — Review: issue #167 (bank-on-action burnout multiplier, commit e82b8d1)

**Scope:** McManus's implementation of bank-on-action burnout (BankedBurnout component, burnout_helpers.h, scoring_system, player_input_system, 11 new tests).

**Decision: REJECTED**

**Blocking issue:**
AC#5 test (`[burnout_bank][first_commit_locks]`) is tautological. The test manually emplaces `BankedBurnout` then checks `if (!reg.any_of<BankedBurnout>(gate))` — which is always false at that point. The second-press branch (Danger bank) is unreachable by construction. The test cannot fail even if `bank_burnout`'s `any_of` guard were deleted from `player_input_system`. Saul's AC requires "shape press at dist=600 followed by lane swipe at dist=200 on the same ComboGate via the system."

**All other ACs passed:**
- AC#1: 4 tiers reachable (Safe/Risky/Danger/Dead) ✅
- AC#2: best_burnout tracks highest bank ✅
- AC#3: no-op clears → MULT_SAFE (not MULT_CLUTCH) ✅
- AC#4: LanePush fully excluded from chain/popup/best_burnout ✅
- AC#6: Existing [scoring],[player],[player_rhythm] tests pass (140 assertions, 72 test cases) ✅
- AC#7: Zero-warning build (Clang, macOS arm64) ✅
- AC#8: Decision recorded in decisions.md ✅

**Non-blocking observations:**
- `BankedBurnout.zone` stored but not consumed in scoring_system (popup uses `tier_for_multiplier(burnout_mult)` instead); functionally equivalent.
- LowBar/HighBar banking intentionally deferred (bars are disabled features per existing tests); documented by McManus; acceptable.

**Revision owner:** Verbal (McManus locked out; fix is a narrow test replacement for AC#5).

**Comment:** https://github.com/yashasg/friendly-parakeet/issues/167#issuecomment-4323298779

---

### 2026-04-26 — Re-review: issue #167 AC#5 (Verbal revision)

**Scope:** Re-review only. Prior rejection (comment #4323298779) rejected McManus's AC#5 as tautological (manual `BankedBurnout` emplace + vacuous branch). Verbal revised.

**Findings:**

All four approval criteria met:

1. **Real system calls:** Both passes use `eq.push_press` / `eq.push_go` + `player_input_system(reg, 0.016f)`. No manual emplace precedes the system run.

2. **Correct sequence:** Gate at dist=600 (Safe) → shape press banks Safe. Gate repositioned to dist=200 (Danger) → GoEvent Right fires. Confirmed `Direction::Right` path in `player_input_system.cpp:156–161` includes `ComboGate` in its predicate. `make_player` sets `Lane::current = 1`, satisfying `lane.current < LANE_COUNT - 1 = 2`, so Pass 2 is not a no-op.

3. **Guard dependency:** Without the first-commit-lock at line 64, `reg.emplace<BankedBurnout>` is called on an already-component-holding entity — EnTT debug assertion crash in debug builds; Danger-zone overwrite in release. Either path is a test failure.

4. **Credible scope:** `[burnout_bank][first_commit_locks]`; 26 assertions / 11 tests + 62 assertions / 34 `[player]` tests all passing.

**Decision:** ✅ APPROVED
**Comment:** https://github.com/yashasg/friendly-parakeet/issues/167#issuecomment-4323312186

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Diagnostics filed 3 new issues: #236 (haptics non-functional, test-flight blocker), #237 (results screen gaps), #238 (total_notes inflation).
- Kujan finding: Cross-domain LanePush passive-leakage pattern (#91, #95, #120, #238) noted for review discipline.
- Findings merged to `.squad/decisions.md`; all merged decisions approved/implemented.

**Final Wave (2026-04-26):**
- #167 Re-Review: Approved bank-on-action burnout multiplier after Verbal's revision (all tests pass, zero warnings)

---

### 2026-05-XX — Cleanup Pass Review: dirty worktree (branch user/yashasg/ecs_refactor)

**Scope:** Reviewer cleanup pass over current dirty worktree. Modified tracked files: `song_state.h`, `ui_element.h`, `game_loop.cpp`, `collision_system.cpp`, `energy_system.cpp`, `play_session.cpp`, `ui_navigation_system.cpp`, `ui_render_system.cpp`, `3_mental_corruption_beatmap.json`, `game_over.json`, `gameplay.json`, `rhythm-spec.md`, `test_burnout_bank_on_action.cpp`, `level_designer.py`. Numerous new untracked files (systems, tests, tools, squad infra).

**Outcome:** ✅ APPROVED — no build blockers, no warning-policy violations, no silent behavior regressions found.

**Findings logged:**
- HIGH (non-blocking): `test_death_model_unified.cpp` does not emplace `GameOverState`, so cause-assignment by `collision_system`/`energy_system` is never asserted in the death model suite. Covered by `test_redfoot_testflight_ui.cpp` but cross-file contract ownership is fragile.
- HIGH (non-blocking): Extensive root-level artifact clutter (session beatmaps, ISSUE_*_SOLUTION.md files, runtime data like `high_scores.json`, `haptics_enabled\``) is untracked. `.gitignore` hardening needed before any bulk `git add .`.
- MEDIUM: `content/beatmaps/3_mental_corruption_beatmap.json` missing trailing newline after `enforce_first_collision_floor` regen.
- MEDIUM: `enforce_first_collision_floor` in `level_designer.py` has no guard against exhausting all obstacles. Shipped-beatmap test provides coverage but edge-case is implicit.
- LOW: `CONFIGURE_DEPENDS` absent from `SYSTEM_SOURCES` and `TEST_SOURCES` GLOBs. New `.cpp` files (ui_source_resolver, sfx_bank) require manual CMake reconfigure locally. Pre-existing; CI is unaffected (CMake runs from scratch in CI).
- LOW: New `.github/workflows/squad-*.yml` files are untracked. Require explicit decision before commit — committing them would activate CI automations without review.

**Conventions confirmed:**
- `GameOverState` null-safe pattern (`ctx().find<>()`) correctly used in all three consumers.
- ECS `text_dynamic` path correctly wired: spawn creates `UIText + UIDynamicText` entity; render reads `UIDynamicText` and resolves via `ui_source_resolver`.
- `render_elements` (JSON-path renderer, LevelSelect only) does NOT need `text_dynamic` handling — GameOver uses the ECS entity path exclusively.
- `test_redfoot_testflight_ui.cpp` is NOT in the CMakeLists EXCLUDE list → included in CI gate ✓.

---

### 2026-05-XX — Final Gate Review: cleanup pass (f98732e + 09c7f4e)

**Scope:** Final reviewer gate on commits `f98732e` (Keyser refactor) and `09c7f4e` (Baer test fixes), plus uncommitted `.gitignore` changes and dirty working tree.

**Commits reviewed:**
- `f98732e refactor(cpp): cleanup pass — size_hint guards, TIMING_TIER_NONE, flush, casts` — behavior-preserving. All changes correct.
- `09c7f4e test(baer): fix all 13 failing tests — cleanup pass complete` — cleanup_system miss-tagging, collision cause tagging, high score wiring, window_scale_for_tier inversion (#223), level_designer passes.

---

**⛔ VERDICT: REJECTED**

**Blocker — Incomplete commit; clean checkout fails to compile:**

`GameOverState` and `DeathCause` are referenced in **committed** files:
- `app/systems/collision_system.cpp` (uses `GameOverState`, `DeathCause`)
- `app/systems/play_session.cpp` (emplaces `GameOverState{}`)
- `tests/test_helpers.h` (`make_registry()` emplaces `GameOverState`)

But `GameOverState` / `DeathCause` are defined **only** in the dirty (uncommitted) modifications to `app/components/song_state.h`. The coordinator's build passed because it compiled the dirty working tree; a `git clone` or `git checkout` of HEAD fails with undefined type errors.

The same pattern applies to `UIDynamicText` (defined in dirty `ui_element.h`, used in dirty `ui_navigation_system.cpp`) — however since both of those are dirty, that pair is self-consistent in the worktree but still uncommitted.

**Revision owner: Coordinator**

Required action: commit all pending dirty source/header/content/test files in a single follow-on commit. Minimum set to unblock compilation:

- `app/components/song_state.h` — defines `GameOverState`, `DeathCause`
- `app/components/ui_element.h` — defines `UIDynamicText`
- `app/game_loop.cpp` — emplaces `GameOverState`
- `app/systems/energy_system.cpp` — `GameOverState` fallback cause
- `app/systems/player_input_system.cpp` — delta refactor (non-breaking)
- `app/systems/player_movement_system.cpp` — ctx lookup hoist (non-breaking)
- `app/systems/ui_navigation_system.cpp` — `skip_for_platform`, `text_dynamic`
- `content/ui/screens/game_over.json` — reason element
- `content/ui/screens/gameplay.json` — energy_label
- `design-docs/rhythm-spec.md` — first-collision floor spec
- `tests/test_burnout_bank_on_action.cpp` — test improvement

---

**Non-blocking notes:**

1. `.gitignore` gap: pattern `/WASM_TEST_*.md` does not cover `WASM_TEST_CHANGES.txt` (a `.txt` file). File is untracked and on disk. Add `/WASM_TEST_CHANGES.txt` or broaden to `/WASM_TEST_*`.
2. `cleanup_system` miss path ordering: `scoring_system` runs at game_loop.cpp L127, `cleanup_system` at L131. For scrolled-past misses, energy drains immediately (correct), but chain is not broken until the following frame (scoring_system sees the entity next tick). 1-frame delay in chain reset for cleanup-derived misses. Not a correctness bug, but worth noting for future instrumentation.
3. `scoring_system` uses `< 0.0f` as energy floor on BAD drain (L101) vs `< 1e-6f` everywhere else. Pre-existing inconsistency; not introduced by this pass.


---

### 2026-05-XX — Final Re-Review Gate: cleanup pass (HEAD 2b48ffd)

**Scope:** Final reviewer gate after Coordinator's two fix commits (`b969a7e`, `2b48ffd`) resolving the prior REJECTED verdict.

**Checks performed:**

| Check | Result |
|-------|--------|
| Clean-checkout compile (coordinator-validated, fresh worktree) | ✅ Pass |
| Test suite (2067 assertions / 649 test cases, coordinator-validated) | ✅ Pass |
| No dirty tracked source/header/test files outside `.squad/` | ✅ Confirmed |
| `b969a7e` commits all files referenced by committed source | ✅ `song_state.h`, `ui_element.h`, all systems, tests, content committed |
| `2b48ffd` vcpkg overlay auto-wire (`CMakeLists.txt`) | ✅ Clean build finds overlay without external env var |
| `.gitignore` hardened for `/WASM_TEST_*.txt`, `/*.json`, `/haptics_enabled*`, `/ISSUE_*`, `/RALPH_*` etc. | ✅ All artifact patterns covered |
| Untracked `squad-*.yml` workflow files NOT committed (no unsafe automation activation) | ✅ Still untracked |
| No generated artifacts accidentally committed (build/, *.log, high_scores.json, etc.) | ✅ None found |
| Zero-warning policy: build confirmed warning-free (Coordinator report) | ✅ |

**Dirty working tree (tracked):**
- `.squad/agents/hockney/history.md` (modified)
- `.squad/agents/kobayashi/history.md` (modified)
- `.squad/decisions/inbox/hockney-ios-testflight-readiness.md` (deleted)

All three are `.squad/` documentation files with zero compile impact. A clean checkout does not carry these changes. Non-blocking.

---

**✅ VERDICT: APPROVED**

**Non-blocking notes:**
1. The three dirty `.squad/` files (two history updates + one deleted inbox decision) are orphaned squad-doc changes. A housekeeping commit would tidy them but is not required.
2. `squad-ci.yml`, `squad-docs.yml`, `squad-heartbeat.yml`, `squad-issue-assign.yml`, `squad-label-enforce.yml`, `squad-preview.yml`, `squad-promote.yml`, `squad-triage.yml`, `sync-squad-labels.yml` remain untracked. Committing any of these activates live GitHub automations — requires explicit team decision before commit. Prior flag stands.
3. Prior non-blockers from the cleanup pass review (cleanup_system 1-frame chain-reset delay, `< 0.0f` vs `< 1e-6f` energy floor inconsistency, `CONFIGURE_DEPENDS` absent from CMake GLOBs) remain open; not introduced by this pass.

---

### 2026-04-27 — Review: issue #304 (WASM lifecycle / hidden registry global) — commit a5cad3d

**Scope:** Kobayashi implementation — explicit WASM shutdown path, idempotency guard, dangling global clear, `game_loop_should_quit` extraction, camera GLSL shader variants, lifecycle tests.

**Verdict:** ✅ APPROVED — #304 can be closed.

**Criteria verified:**

| Criterion | Result |
|-----------|--------|
| WASM frame_callback detects quit and calls shutdown | ✅ `game_loop_should_quit` checked every frame; `emscripten_cancel_main_loop()` + `wasm_shutdown_once()` called on true |
| Browser unload path covered | ✅ `emscripten_set_beforeunload_callback` registered → `on_web_unload` → `wasm_shutdown_once()` |
| Idempotency guarded | ✅ `wasm_shutdown_once()` nulls `g_state.reg` before calling shutdown; any re-entrant call is a no-op |
| Dangling registry pointer eliminated | ✅ `g_state.reg = nullptr; g_state.accumulator = 0.0f` set before `game_loop_shutdown()` call |
| Native run loop preserved | ✅ `while (!game_loop_should_quit(reg))` — equivalent to prior `WindowShouldClose()` plus adds `quit_requested` path |
| `game_loop_should_quit` correctness | ✅ Checks `WindowShouldClose()` first, then `input->quit_requested`; null-safe via `find<InputState>()` |
| camera_system.cpp changes necessary and safe | ✅ Adds `#ifdef PLATFORM_WEB` GLSL 100 shaders for WebGL; `camera::shutdown()` unaffected; no conflict with #265 RAII |
| Tests — quit predicate coverage | ✅ 4 `[lifecycle]` tests: null-InputState safety, false/true flag, flag reset |
| Tests — no coverage weakened | ✅ `test_ndc_viewport.cpp` additions are net-new ScreenTransform tests; existing checks intact |
| Commit scope limited to #304 | ✅ All touched files are on-scope; `test_ndc_viewport.cpp` additions are minor scope expansion but harmless |

**Non-blocking observations:**
- `on_web_unload` returns `nullptr` with comment "empty string suppresses the browser confirmation dialog" — `nullptr` and `""` are functionally equivalent for Emscripten's beforeunload handler; comment is slightly inaccurate, behaviour is correct.
- `WindowShouldClose()` branch of `game_loop_should_quit` is untested in the native test binary (requires a raylib window context). This is acknowledged in the test file header comment and is acceptable — the flag branch is fully tested.
- `test_ndc_viewport.cpp` +51 lines (ScreenTransform coordinate math) are slightly out of the #304 lifecycle scope but add genuine coverage and do not introduce regressions.

---

### 2026-05-XX — Review: issue #253 (HighScoreState compact array — Hockney impl)

**Scope:** Replace `std::map<std::string,int32_t>` with compact fixed-size inline array in `HighScoreState`. Commits reviewed: `60bcd26` (functional impl), cross-checked with HEAD.

**Criteria verified:**

| Criterion | Result |
|-----------|--------|
| No `std::map` or node-based storage | ✅ `std::array<Entry, MAX_ENTRIES>` with `entry_count` counter — zero heap nodes |
| Inline key storage (no heap per entry) | ✅ `char key[KEY_CAP=32]` inside `Entry` — fully inline |
| Correct lookup for known keys | ✅ `get_score` linear scan via `strncmp`; returns stored value |
| Correct lookup for missing keys | ✅ Returns 0; tested via `get_current_high_score()` before any insert |
| Update existing key without duplicate | ✅ `set_score` finds and updates in-place; does not append |
| Capacity behavior explicit and safe | ✅ Comment: "Silently drops if table is full." Guard: `if (entry_count < MAX_ENTRIES)` — no corruption |
| Capacity sized to game data | ✅ `MAX_ENTRIES=9` = `LEVEL_COUNT(3) × DIFFICULTY_COUNT(3)` — table cannot overflow in normal operation |
| Negative scores clamped on load | ✅ `raw < 0 → 0`; tested with `"negative|easy": -100` → `get_score == 0` |
| Oversized scores clamped on load | ✅ `raw > INT32_MAX → INT32_MAX`; tested with `"huge|hard": 999999999999` → `get_score == INT32_MAX` |
| JSON format preserved (backward compat) | ✅ Same `{"scores":{key:value,...}}` structure; load preserves `current_key` |
| Missing file returns false, state unchanged | ✅ Tested |
| Malformed JSON returns false, state unchanged | ✅ Tested |
| Invalid schema (array instead of object) returns false | ✅ Tested |
| String-typed score value returns false | ✅ Tested |
| `current_key` preserved across load | ✅ `next.current_key = state.current_key` before JSON parse; tested |
| SIGABRT from old `dense_map::operator[]` resolved | ✅ `set_score` replaces the crashing `scores["key"] = value` pattern entirely |
| No new heap allocations in hot score paths | ✅ `get_score`/`set_score` touch only `char[]` and `int32_t` inside fixed array |
| Targeted `[high_score]` tests pass | ✅ 47 assertions / 14 test cases — all pass |
| Build: zero warnings | ✅ Clean compile, no warnings |

**Non-blocking note:**
- No explicit test for the capacity-overflow path (`set_score` when `entry_count == MAX_ENTRIES`). Under normal game operation this edge is unreachable (3×3=9=`MAX_ENTRIES`), and the drop behavior is safe and documented. Acceptable gap; could harden if song/difficulty counts ever expand.

**Verdict:** ✅ APPROVED — #253 can be closed.

### 2026-04-27 — Review: issue #243 (collision perf — per-kind structural views)

**Scope:** Keaton's `b65c301` — replace broad `ObstacleTag+Obstacle` view + `try_get` dispatch chain with six tight per-kind structural EnTT views.

**Verification performed:**
- Ran `[collision]` test tag: 47/49 test cases pass, 101/105 assertions pass.
- 2 failures (`low bar drains energy when sliding`, `high bar drains energy when jumping`) are pre-existing: both tests call `collision_system` but omit the required `scoring_system` call to complete the energy-drain chain. These pre-date this commit; Keaton explicitly documented them in the commit message.
- Structural views confirmed: ShapeGate, LaneBlock, LowBar/HighBar (RequiredVAction), ComboGate, SplitPath, LanePush — all six correct.
- No per-entity `try_get` for obstacle kind/data in any hot path. One `try_get<BeatInfo>` remains in `resolve()` for rhythm timing grade only — correct and out of scope.
- `resolve()` lambda receives `ObstacleKind` directly from caller; last `try_get<Obstacle>` removed.
- #280 boundary intact: `collision_system` emplace `MissTag`/`ScoredTag` only; energy drain stays in `scoring_system`.
- LanePush: timing-window guard (`continue` before `ScoredTag`), same-lane check, delta ±1, `dest >= 0 && dest < LANE_COUNT` edge clamp, `p_lane.target < 0` no-overwrite guard, always scores once in window. All correct.
- Four `[lane_push]` tests added: push-left, push-right, out-of-range clamp (stays -1), timing-window guard (not scored). `make_lane_push` helper added to `test_helpers.h`. Full coverage of critical paths.
- No try_get chain regressions. Exclude masks on structural views correctly differentiate overlapping component sets (ComboGate vs SplitPath vs ShapeGate via `RequiredLane` exclusion).
- Build: zero warnings (zero-warning policy enforced via `-Wall -Wextra -Werror`).

**Non-blocking notes:**
- No test covers LanePush when player is in timing window but NOT on the same lane (obstacle still scores, no push). Minor gap; not blocking.
- The 2 pre-existing test failures (`low bar drains energy when sliding`, `high bar drains energy when jumping`) are a test-authoring bug: missing `scoring_system` calls. These should be assigned to Verbal for a future cleanup pass.

**Verdict:** ✅ APPROVED — #243 can be closed.

### 2026-05-XX — Review: issue #265 (GPU RAII guards — Hockney impl)

**Scope:** Hockney `817b062` — RAII ownership semantics for `RenderTargets` (camera.h) and `camera::ShapeMeshes` (camera_system.h/.cpp); test coverage in `test_gpu_resource_lifecycle.cpp`.

**Criteria verified:**

| Criterion | Result |
|-----------|--------|
| Copy ctor/assign deleted (RenderTargets) | ✅ `= delete` on both |
| Copy ctor/assign deleted (ShapeMeshes) | ✅ `= delete` on both |
| Move ctor/assign declared noexcept (both) | ✅ `noexcept` on all four |
| Move transfers ownership — source `owned` cleared | ✅ `o.owned = false` in both move ctor bodies |
| Move assign calls `release()` on `this` before steal | ✅ `release()` called first in both move assign bodies; self-assign guarded |
| `release()` idempotent — `if (!owned) return` guard | ✅ Both structs |
| `release()` zeros members after unload | ✅ Shapes, slab, quad, material zeroed; world, ui zeroed |
| `owned` set only after successful GPU creation | ✅ `sm.owned = true` at end of `build_shape_meshes()`; `RenderTargets(w,u)` ctor sets `owned{true}` after both textures are passed in |
| `camera::shutdown` + ctx destruction safe (no double-unload) | ✅ `shutdown()` calls `.release()` on both → `owned=false`; registry-ctx destructor fires `release()` again → no-op |
| Tests: no GL context required | ✅ Static type-trait `static_assert` and manual `owned` manipulation only; no `InitWindow()` |
| Tests: static_assert non-copyable | ✅ 4 static_asserts (copy ctor + copy assign for each struct) |
| Tests: static_assert movable | ✅ 4 static_asserts (move ctor + move assign for each struct) |
| Tests: default-constructed `owned == false` | ✅ 2 test cases |
| Tests: move transfers ownership | ✅ 2 test cases; `owned = false` cleanup prevents spurious GL unload |
| Tests: idempotent release on unowned | ✅ 2 test cases (double `release()` on default-constructed objects) |
| CMake: new test picked up via `file(GLOB)` | ✅ `file(GLOB TEST_SOURCES tests/*.cpp)` — `test_gpu_resource_lifecycle` is NOT in the EXCLUDE REGEX list |
| CMake EXCLUDE REGEX unchanged (no scratch tests admitted) | ✅ Filter still excludes `test_safe_area_layout`, `test_ui_redfoot_pass`, `test_ui_element_schema`, `test_ui_overlay_schema`, `test_lifecycle` only |
| No conflict with #304 WASM GLSL changes | ✅ `#ifdef PLATFORM_WEB` shader blocks are independent of RAII wrappers; `camera::shutdown()` logic unchanged for WASM path |
| Build: zero warnings | ✅ Clean compile with `-Wall -Wextra -Werror` |
| `[gpu_resource_lifecycle]` tests pass | ✅ 8 assertions / 6 test cases — all pass |

**Non-blocking observations:**
- In `unload_shape_meshes()`, `UnloadShader(sm.material.shader)` is called before `UnloadMaterial(sm.material)`. raylib's `UnloadMaterial` also unloads the material's shader, which means the shader ID could be freed twice. This is pre-existing code (not introduced by #265); raylib's `rlUnloadShaderProgram` is a no-op for the default shader and on most drivers calling `glDeleteProgram` on an already-deleted program is also a no-op. Not blocking for this review, but worth a cleanup ticket.
- Moved-from structs leave stale GPU ID values in fields (only `owned` is cleared, not the ID fields). This is safe and standard RAII practice — `owned=false` prevents the destructor from acting on them.

**Verdict:** ✅ APPROVED — #265 can be closed.

---

### 2026-04-27 — Review: PR #43 mesh child cleanup CI fix (Keaton)

**Scope:** `tests/test_helpers.h` + `tests/test_pr43_regression.cpp` — priming `ObstacleChildren` pool before `wire_obstacle_counter` in `make_registry()`; removing redundant priming from `make_obs_registry()`.

**Root cause verified:** `wire_obstacle_counter(reg)` calls `reg.on_construct/on_destroy<ObstacleTag>()`, which allocates the `ObstacleTag` pool. Any subsequent `reg.storage<ObstacleChildren>()` therefore gets a higher pool index. EnTT `reg.destroy()` iterates pools in reverse insertion order, so `ObstacleChildren` (higher index) was torn down **before** `on_obstacle_destroy` fired for `ObstacleTag` — `try_get<ObstacleChildren>` returned null and silently skipped child deletion.

**Criteria verified:**

| Criterion | Result |
|-----------|--------|
| Fix mirrors production ordering in `game_loop.cpp:113-114` | ✅ `reg.storage<ObstacleChildren>()` is now the first pool operation in `make_registry()`, before `wire_obstacle_counter` |
| `make_registry()` priming is side-effect-free for all tests | ✅ Only ensures pool exists; no components emplaced, no signals connected; unrelated tests unaffected |
| `make_obs_registry()` redundant priming correctly removed | ✅ Old `reg.storage<ObstacleChildren>()` inside `make_obs_registry()` fired *after* `make_registry()` already created the `ObstacleTag` pool — it was the broken call; removing it is correct |
| Ordering invariant ownership is clear | ✅ Comment at `test_pr43_regression.cpp:271-274` and `test_helpers.h:30-32` both document the invariant and point to `make_registry()` as the owner |
| `on_obstacle_destroy` still connected correctly in `make_obs_registry()` | ✅ Signal connection retained; only the redundant priming removed |
| Regression tests correctly exercise the fix | ✅ Both `[obstacle][cleanup][pr43]` tests verify: children of destroyed parent removed, bystander parent's children untouched |
| Fix is strictly test-scoped — production code untouched | ✅ `game_loop.cpp` was already correct; no production files modified |
| No hidden production bug masked | ✅ Production invariant is the reference; tests now replicate it rather than deviate from it |

**Verdict:** ✅ APPROVED — PR #43 mesh child cleanup CI failure family is resolved.

**Non-blocking notes:** None.

### 2026-05-16 — EnTT ECS Audit: systems and components vs. Entity_Component_System.md

**Scope:** Full pass of `app/components/` and `app/systems/` against the EnTT wiki guide.

**Key findings:**
- `scoring_system.cpp` removes iterated view components (`Obstacle`, `ScoredTag`) inside `view.each()` — allowed by EnTT but invalidates dangling refs. Safe today because refs aren't used post-remove; latent UB trap. Collect-then-process pattern preferred.
- `COLLISION_MARGIN = 40.0f` defined in three separate `.cpp` files (collision, beat_scheduler, test_player). Promoted to `constants.h` is the fix. Drift = broken rhythm timing.
- `APPROACH_DIST` duplicated between `constants.h` and `song_state.h`. Known; still present.
- `ensure_active_tags_synced()` in `input_events.h` is a full registry-mutating system operation living in a component header — convention violation but no correctness risk.
- Render systems (`game_render_system`, `ui_render_system`) take non-const `entt::registry&` though they are read-only. Should be `const`.
- `HighScoreState` and `SettingsState` components contain non-trivial mutation methods — low risk (context singletons), but precedent concern.
- `static std::vector` in `cleanup_system.cpp` — intentional/documented optimization, NOISE.
- No groups used anywhere — views are correct, groups are optimization opportunity only, NOISE.

**Deliverables:** Decision written to `.squad/decisions/inbox/kujan-entt-ecs-audit-review.md`. Reusable checklist at `.squad/skills/entt-ecs-audit/SKILL.md`.

**Key file paths to remember:**
- ECS guide: `docs/entt/Entity_Component_System.md`
- `COLLISION_MARGIN` triplicated: `app/systems/collision_system.cpp:16`, `app/systems/beat_scheduler_system.cpp:25`, `app/systems/test_player_system.cpp:319`
- `APPROACH_DIST` duplicated: `app/constants.h:70`, `app/components/song_state.h:72`
- System logic in component headers: `app/components/input_events.h` (`ensure_active_tags_synced`), `app/components/obstacle_counter.h` (`wire_obstacle_counter`)

### 2026-05-17 — EnTT ECS Audit Review: Synthesis & Prioritization

**Role:** Review findings from Keyser (compliance) and Keaton (performance). Synthesize into actionable backlog with clear ownership.

**Material findings consolidated:**

| Priority | Category | Owner | Est. Effort |
|----------|----------|-------|------------|
| F1 | scoring_system removes iterated view components | McManus | Low |
| F2 | COLLISION_MARGIN triplicated across 3 files | McManus/Fenster | Low |
| F3 | APPROACH_DIST duplicated in .h files | McManus/Fenster | Trivial |
| F4 | System logic in component headers | McManus | Low-Medium |
| F5 | Render systems non-const registry | McManus/Keaton | Trivial |
| F6 | Component struct mutation methods | McManus/Saul | Low |
| Rule 1 | ScreenPosition emplace_or_replace → get_or_emplace | Keaton | Trivial |
| Rule 2 | scoring_system structural view branching | McManus | Low |
| Rule 3 | Hoist ctx() lookups above loops | McManus/Keaton | Low |

**Noise items (approved as-is):** cleanup_system static buffer (#242), no groups (correct choice), BurnoutState stale fields (no UB).

**Status:** Orchestration log written. Backlog ready for team sprint assignment. Decision inbox consolidated. Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`

### 2026-04-27 — Review: EnTT Dispatcher Input Model Rewrite (commit 2547830, Keaton)

**Scope:** Full input-model migration from hand-rolled EventQueue go/press arrays to entt::dispatcher.

**Key findings:**

- GoEvent and ButtonPressEvent are fully delivered via `entt::dispatcher` in `reg.ctx()`. Old `go_count`/`press_count` manual arrays gone. #213 replay bug eliminated naturally via `disp.update<T>()` drain semantics.
- Architecture diverged from decisions.md Tier-1 spec (InputEvent through dispatcher with listeners). Instead, gesture_routing and hit_test remain direct system calls; they read raw EventQueue and enqueue semantic events. This avoids pool-order latency without needing `trigger()`. Same-frame behavior preserved.
- EventQueue struct not fully removed (migration step 4 deferred). Still serves as raw gesture buffer (InputEvent[] only). Acceptable per "where intended" qualifier in acceptance criteria.
- R7 (start-of-frame dispatcher clear) not hardened for dispatcher queues — benign in practice since game_state_system always drains first tick.
- Contract test comments about "must use trigger()" are now stale/misleading (approach changed).
- All 2419 assertions / 768 test cases pass. Zero build warnings.

**Verdict:** APPROVED

**Guardrails for future dispatcher work:**
- Always unwire listeners before reg.clear() — registry pointer in sink is not ref-counted.
- The redundant disp.update() in player_input_system is load-bearing for isolated unit tests; do not remove it.
- If EventQueue raw-buffer is removed in future, test_input_pipeline_behavior.cpp and test_gesture_routing_split.cpp will need updating to inject events via dispatcher directly.

### 2026-04-27 — Input Dispatcher Review COMPLETE (APPROVED)

**Evidence:** 2419 assertions / 768 test cases pass; zero warnings; architecture documented; follow-ups identified.

**Review Decision:** APPROVED

**Approved architecture:**
- GoEvent/ButtonPressEvent delivery via entt::dispatcher ✓
- Same-frame semantics preserved ✓
- Manual zeroing anti-pattern (#213) eliminated ✓

**Architecture diff from decisions.md:**
- Tier-1 spec used InputEvent → dispatcher with gesture_routing/hit_test as listeners (to avoid pool order latency)
- Actual implementation: gesture_routing/hit_test stay as direct system calls, read raw EventQueue, enqueue semantic events. No latency hazard, same effect.
- This is architecturally equivalent and acceptable per acceptance criteria.

**Non-blocking follow-ups:**
1. Harden R7: explicit start-of-frame dispatcher clear for GoEvent/ButtonPressEvent
2. Update stale test comments in test_entt_dispatcher_contract.cpp
3. Audit EventQueue raw InputEvent buffer (defensive)

**Merged to decisions.md with full review details.**

### 2026-04-27 — Issue #315 Review Approval (EnTT-safe scoring_system iteration)

**McManus implementation reviewed and approved.**

**Evidence:** 
- Zero-warning build confirmed
- 2419 assertions / 768 test cases passed on main branch post-integration
- Collect-then-remove pattern properly implemented across two structural views
- No regressions introduced

**Verdict:** Issue #315 closed; pattern ready for team reference.

### 2026-04-27 — Review: issue #320 (consolidate duplicated rhythm timing constants)

**Scope:** McManus implementation — single canonical `COLLISION_MARGIN` in `constants.h`, removal of local copies from `collision_system.cpp`, `beat_scheduler_system.cpp`, and `test_player_system.cpp`; removal of duplicate `APPROACH_DIST` local from `song_state.h`.

**Findings:**
- `constants::COLLISION_MARGIN = 40.0f` added under "Rhythm Constants" section of `constants.h` alongside `APPROACH_DIST`. Correct location and value.
- All three local `constexpr float COLLISION_MARGIN = 40.0f` definitions removed from production systems; all call sites updated to `constants::COLLISION_MARGIN`. Clean.
- `song_state.h` local `APPROACH_DIST = 1040.0f` removed; `constants.h` included; `song_state_compute_derived()` now uses `constants::APPROACH_DIST`. Correct.
- `test_beat_scheduler_system.cpp` updated to `constants::COLLISION_MARGIN` (was the only test-side local copy).
- No residual local copies of either constant found anywhere in `app/` or `tests/`. Zero stray definitions.
- No unrelated production changes. 6 files, all narrowly scoped to the consolidation.
- Numeric values unchanged: 40.0f and 1040.0f — timing behavior preserved exactly.
- Build: clean, zero warnings (ld duplicate-lib linker note is pre-existing, not introduced here).
- Tests: **2419 assertions / 750 test cases — all pass**.

**Verdict:** APPROVED

---

### 2026-04-28 — Review: issue #323 (RNGState initialization moved to setup path)

**Scope:** Keaton implementation — `game_loop_init`, `setup_play_session`, `obstacle_spawn_system`.

**Verdict:** ✅ APPROVED

**Acceptance criteria check:**
- `RNGState` initialized at boot: `game_loop_init` adds `reg.ctx().emplace<RNGState>()` alongside other core singletons. ✓
- Session reset determinism: `setup_play_session` adds `reg.ctx().insert_or_assign(RNGState{})` in the "Reset singletons" block; default seed `1u` is always restored at session start. ✓
- Hot path guard removed: `obstacle_spawn_system` replaces the lazy `find/emplace` pattern with `reg.ctx().get<RNGState>()` which terminates on missing state — programmer error surfaces immediately rather than silently recovering. ✓
- Test helpers: `make_registry()` in `test_helpers.h` already emplaced `RNGState{}` — no test changes required; all existing tests remain compatible. ✓
- Behavior preserved: no logic changes to spawn system beyond the ctx access pattern. ✓
- Build/test evidence: **2419 assertions / 750 test cases — all pass** (verified locally). Zero-warning build (pre-existing duplicate-lib linker note from vcpkg is unrelated). ✓
- Scope: exactly 3 production files changed plus Keaton's history log. No unrelated changes. ✓

**Pattern established:** `insert_or_assign(T{})` in `setup_play_session` for deterministic-per-session singletons; `emplace<T>()` in `game_loop_init` for boot singletons; `ctx().get<T>()` in systems to surface missing setup as a hard failure.

### 2026-04-27 — Review: issue #325 (const render registry)

**Scope:** Keaton implementation — render systems refactored to accept const registry; material tinting uses local Material copies.

**Verdict:** ✅ APPROVED

**Acceptance criteria check:**
- Render system signatures updated: all `update_*_render()` and related functions take `const entt::registry&`. ✓
- Material tinting: `apply_material_tint` uses local `Material` copies instead of mutating registry state. ✓
- No structural mutations in render path: const registry contract enforced throughout render loop. ✓
- Build/test evidence: **zero-warning build; all existing tests pass**. ✓
- Scope: narrowly scoped to render systems; no unrelated changes. ✓

**Pattern reinforced:** Render systems are pure observers. Data mutations belong in non-render systems; render-phase tinting uses temporary local state.

### 2026-04-27 — Scribe Closure Summary: Issues #316, #317, #323, #325

Four ECS refactor issues documented as complete:
- **#316:** UIState loading boundary — closed, 2588 assertions / 808 test cases pass
- **#317:** active-tag extraction — closed, full tests pass
- **#323:** RNGState setup init — closed, pattern established
- **#325:** const render registry — closed, render path now pure observer

All approvals logged. Orchestration and session logs created. Agent histories synchronized.

---

### 2026-04-27 — Review: issue #327 (dispatcher drain-ownership comments and guards)

**Scope:** Keaton implementation — comments-only (plus one `(void)disp` guard) clarifying drain ownership, `clear()` hazard warnings, and isolated-test semantics across five files.

**Verdict:** ✅ APPROVED

**Acceptance criteria check:**

1. **Drain-ownership semantics and `game_state_system` authority**
   - `game_state_system.cpp` new block comment correctly identifies the system as the primary drain owner: it runs first in `tick_fixed_systems`, calls `disp.update<GoEvent/ButtonPressEvent>()` at its top, and fires all listeners in registration order (`game_state → level_select → player_input`).
   - `game_loop.cpp` annotation accurately mirrors this — names `game_state_system` as drain owner and notes that later systems find an empty queue.
   - `input_dispatcher.cpp` drain-ownership header explains single-drain-per-frame semantics and that registration order equals processing order (EnTT R1). All three listener pairs confirmed in the actual registration code. ✓

2. **`disp.clear<GoEvent/ButtonPressEvent>()` warning**
   - `game_state_system.cpp` carries an explicit `⚠ Do NOT call disp.clear<>()` warning placed correctly — before the `disp.update<>()` calls — with accurate reasoning: pre-tick systems (input_system, gesture_routing, hit_test) enqueue same-frame events that must reach listeners through this drain; a `clear()` here would silently drop them.
   - No `disp.clear()` calls were found anywhere in `app/` or `tests/` — the warning is preventive, not corrective. ✓

3. **Isolated-test `disp.update()` dual-role explanation**
   - `player_input_system.cpp` comment accurately distinguishes: in production the queue is already empty (no-op); in isolated tests with no preceding `game_state_system`, the `update()` calls are the sole drain.
   - The dual-role claim is semantically correct per EnTT: `update()` on an empty queue is a defined no-op.
   - `⚠ Do NOT replace with clear()` warning correctly references R3 (clear skips listeners) and the test-path masking risk. ✓

4. **No behaviour changes**
   - Verified via diff: five source files changed, all additions are comments or the one `(void)disp` suppress annotation. No logic, no new branches, no removed code.
   - `(void)disp` in `input_system.cpp` is a safe no-op expression — suppresses unused-variable warning on non-keyboard platforms; the comment accurately explains why. ✓

5. **Build and test evidence**
   - Full non-benchmark suite: **2588 assertions in 790 test cases — all pass** (verified locally in the worktree).
   - Focused `[input_pipeline],[player]` tags: **86 assertions in 44 test cases — all pass**.
   - Zero new compiler warnings. ✓

6. **Scope**
   - Exactly 5 production files changed plus Keaton's history log. No test file changes, no beatmap changes, no unrelated modifications. ✓

**Non-blocking notes:** None.

### 2026-05-17 — Review: issue #318 (move high-score and settings mutation logic out of components)

**Scope:** McManus implementation — `HighScoreState::update_current_high_score`, `set_current_key` and `SettingsState::clamp_audio_offset`, `clamp_ftue_run_count`, `mark_ftue_complete` moved to free functions in `high_score::` and `settings::` namespaces.

**Commit reviewed:** f34de773d419639d4c6e5cb5e6b9365b3b79276a

**Tests run:**
- Full suite: **2588 assertions / 808 test cases — all pass**
- Focused `[high_score],[settings],[ftue]`: **155 assertions / 36 test cases — all pass**
- Build: zero warnings (pre-existing duplicate-lib linker note, not introduced here)

**Findings:**

**Correct work (AC met):**
- `HighScoreState`: `update_current_high_score()` and `set_current_key()` removed from struct; `high_score::update_if_higher()` added to `high_score_persistence.h/.cpp`. Call sites updated in `game_state_system.cpp` (both game-over and song-complete paths). `play_session.cpp` now uses `hs->current_key = HighScoreState::make_key(...)` directly.
- `SettingsState`: `clamp_audio_offset()`, `clamp_ftue_run_count()`, `mark_ftue_complete()` removed from struct; added as free functions in `settings_persistence.h/.cpp`. `#include <algorithm>` removed from settings.h (no longer needed).
- All test files updated to use new free-function call sites. Behavior preserved.

**Blocking issue:**
- Branch `squad/318-high-score-settings-logic` was cut **before** commits `2e2b0c8` (Scribe ECS closures) and `b5e81c1` (docs #327 dispatcher safety comments) landed on `user/yashasg/ecs_refactor`. The branch is **2 commits behind** the merge target.
- `git diff user/yashasg/ecs_refactor..HEAD` shows those dispatcher safety docs as "removed" — they never existed on this branch. The combined post-merge state has not been tested. A rebase is required before this can safely merge.

**Non-blocking note:**
- `settings::mark_ftue_complete()` is defined and exported but has no active call sites in production code and no focused unit test for it. The function is correctly implemented; coverage is advisory.

**Verdict:** REJECTED — rebase onto `user/yashasg/ecs_refactor` required before merge.
**Lockout:** McManus is locked out. Keaton owns the rebase (authored #327, understands dispatcher context).

### 2026-05-17 — Review: issue #312 (pre-compute UI element map with entt::hashed_string keys)

**Scope:** Keaton implementation — `build_ui_element_map()` free function builds `std::unordered_map<entt::id_type, std::size_t>` in `UIState` at screen-load time; `find_el` in `ui_render_system.cpp` replaced with O(1) hashed lookup using `_hs` literals.

**Commit reviewed:** 62ace0d59a65463b09243d9bbbf40fa9fdeb46e1

**Tests run:**
- Focused `[ui][312]`: **19 assertions in 5 test cases — all pass**
- Full suite: **2607 assertions in 813 test cases — all pass**
- Build: zero warnings

**Findings:**

**Correct work (AC met):**
- `UIState::element_map` field added; built once per `screen_changed` boundary in `ui_navigation_system`, not per frame.
- `build_ui_element_map()` clears before rebuild, skips elements with empty `id`, consistent with `entt::hashed_string::value(id.c_str())` keying.
- `find_el` signature changed to `(const UIState&, entt::id_type)` with defensive stale-map guards (map miss, `elements` missing, bounds check).
- All call sites use `_hs` literals at compile time: `"shape_buttons"_hs`, `"lane_divider"_hs`, `"song_cards"_hs`, `"difficulty_buttons"_hs`.
- 5 focused tests cover: empty map, index-by-hash, id-less elements skipped, rebuild-on-re-call, gameplay.json integration smoke-test.
- 2 missing commits on `user/yashasg/ecs_refactor` (b5e81c1 dispatcher docs, 2e2b0c8 squad docs) touch **disjoint files** — no overlap with UI subsystem; merge-tree confirms zero conflicts.

**Non-blocking notes:**
- `entt/entt.hpp` now in `ui_state.h` (heavy include). Acceptable given project-wide EnTT usage; build confirms no warnings.
- `overlay_screen` intentionally not mapped — commit message notes no `find_el` calls for it; correct scope.

**Reusable quality note:** When a branch is behind merge target, check file overlap before requiring rebase. Disjoint file sets = no combined-state risk. File overlap = rebase required (see #318).

**Verdict:** ✅ APPROVED

### 2026-05-17 — Re-review: issue #318 (high-score and settings logic — post-rebase)

**Scope:** Keaton rebase — branch `squad/318-high-score-settings-logic` rebased/merged onto `origin/user/yashasg/ecs_refactor` at commit `d4d3f01`.

**Blocking issue from prior rejection:** Branch was 2 commits behind target (missing `2e2b0c8` and `b5e81c1`). ✅ Resolved — `d4d3f01` is a merge commit bringing both into branch history.

**Diff vs target (`user/yashasg/ecs_refactor`):** Only the expected #318 production and test files (11 files). No dispatcher safety comments dropped; merge was clean.

**Tests run:**
- Full suite: **2588 assertions / 808 test cases — all pass** ✅
- Focused `[high_score],[settings],[ftue]`: **155 assertions / 36 test cases — all pass** ✅
- Build: zero warnings (clean, no new issues) ✅

**Verdict:** ✅ APPROVED — stale-branch blocker resolved, combined post-merge state fully passes.

**Commits reviewed:** f34de773 (McManus original), d4d3f01 (Keaton rebase merge)

### 2026-05-17 — Review: issue #324 (remove dead burnout ECS surface)

**Scope:** McManus implementation (`d9be464`) + Baer test follow-up (`6ee912a`) on branch `squad/324-remove-dead-burnout-surface`.

**Commits reviewed:** d9be464, 6ee912a

**Diff summary (vs `user/yashasg/ecs_refactor`):**
- Deleted: `burnout.h`, `burnout_helpers.h`, `burnout_system.cpp`, all burnout-focused test files (5 test files)
- Removed: `BurnoutState` ctx registration, `burnout_window_scale` field, `best_burnout` in `SongResults`, burnout zone/multiplier constants, burnout NDC bar constants, dead `burnout_mult` variable in scoring
- Updated: comments cleaned of "burnout ladder" wording; comment clarified to "scoring ladder"
- Added: LanePushLeft + LanePushRight exclusion tests in `test_scoring_extended.cpp`

**Preserved behaviors verified:**
- Energy system (`EnergyState`, `energy_system.cpp`) ✅
- Timing-graded scoring (`timing_multiplier`) ✅
- Chain bonus ✅
- LanePush exclusion (code unchanged, now both Left+Right covered) ✅
- `SongResults.max_chain`, distance bonus ✅

**Tests run:**
- `[scoring][lane_push]`: 8 assertions / 2 test cases — PASS
- `[scoring]`: 43 assertions / 25 test cases — PASS
- Full suite: 2565 assertions / 792 test cases — PASS
- Build: zero warnings

**Non-blocking notes (pre-existing, out of scope):**
- `"burnout_meter"` string in `ui_loader.cpp` / `test_ui_element_schema.cpp` is a pre-existing UI schema name not wired to any ECS component or JSON screen content — candidate for a follow-up cleanup ticket.
- `scoring.h:20` comment `// burnout tier (legacy)` predates this branch; minor cleanup candidate.

**Verdict:** ✅ APPROVED

## Learnings
- **Reusable quality note:** When removing a dead mechanic, verify that (1) all live ECS ctx registrations are removed, (2) the dead multiplier variable is dropped (not left as hardcoded 1.0f), and (3) both symmetric variants of an exclusion rule (Left+Right, Up+Down) have independent regression tests.

### 2026-05-17 — Review: issues #311/#314 (GamePhaseBit enum with entt::enum_as_bitmask)

**Scope:** Keaton implementation — `GamePhaseBit` power-of-two enum class with `_entt_enum_as_bitmask` sentinel in `game_state.h`; `GamePhase` stays sequential state-machine discriminant; `to_phase_bit(GamePhase)` bridge function; `ActiveInPhase::phase_mask` typed as `GamePhaseBit`; all `phase_bit()`/raw `uint8_t` spawn sites migrated; 5 new `[phase_mask]` tests (28 assertions).

**Commit reviewed:** 6efd1b7b1d02cd19428c6e4f0286fa15c145d09e

**Merge compatibility:** Branch is 1 commit ahead of merge base `b5e81c1`. Target (`origin/user/yashasg/ecs_refactor`) has since integrated #312 (`e04d35b`) and #318 (`fdcd709`). `git merge-tree` reports zero conflicts — disjoint file sets (UI element map, high-score/settings vs. phase bitmask changes).

**Tests run:**
- Focused `[phase_mask]`: **28 assertions in 5 test cases — all pass**
- Full suite: **2616 assertions in 813 test cases — all pass**
- Build: zero warnings (arm64 macOS clang, -Wall -Wextra -Werror)

**Findings:**

**Correct work (all AC met):**
- AC1: `_entt_enum_as_bitmask` sentinel is last in `GamePhaseBit`, enabling EnTT typed `|/&/^` operators. Correct usage.
- AC2: `GamePhase` untouched (sequential 0–5), no switch/comparison sites at risk. `to_phase_bit` bridge is `constexpr`, `[[nodiscard]]`, maps each `GamePhase` value to the correct power-of-two bit. Round-trip test validates all 6 values.
- AC3: `ActiveInPhase::phase_mask` is `GamePhaseBit` (not `uint8_t`). Zero raw `phase_bit()` call sites remain in production code. `test_helpers.h` uses `to_phase_bit()` at the one remaining bridge site.
- AC4: Components (`ActiveInPhase`, `GameState`) are data-only. `phase_active()` is a free helper in the header, not a member method — no registry logic in component headers.
- AC5: Tests cover single-phase (6 assertions), multi-phase OR (6 assertions), distinct-bit uniqueness, round-trip all 6 phases, empty-mask semantics. Bit collision is explicitly tested via the combined-mask inequality assertions.

**Non-blocking notes:**
- `to_phase_bit` is not bounds-checked for hypothetical future `GamePhase` values ≥ 8 (would shift off `uint8_t`). Acceptable for current 6-phase design; a future addition should add a `static_assert` guard.
- `GamePhaseBit{}` default value is 0 (no named enumerator), which is valid C++ and semantically correct (empty mask).

**Reusable quality note:** When using `_entt_enum_as_bitmask`, keeping the state-machine enum (sequential) separate from the bitmask enum (power-of-two) avoids mixed-use bugs. Bridge via `constexpr` conversion function is the correct pattern.

**Verdict:** ✅ APPROVED — clean merge, zero warnings, all tests pass, all 5 acceptance criteria met.

### 2026-05-17 — Review: issue #313 (replace std::string current_key with entt::hashed_string)

**Scope:** Keaton implementation — `880b389 perf(#313): replace std::string current_key with entt::hashed_string hash`

**Commits reviewed:** 880b389

**Diff summary (vs `origin/user/yashasg/ecs_refactor`):**
- `high_score.h`: removed `current_key` (std::string) and `make_key()`; added `current_key_hash` (uint32_t), `make_key_str()`, `make_key_hash()`, `ensure_entry()`, `get_score_by_hash()`, `set_score_by_hash()`, `get_current_high_score()`
- `high_score_persistence.cpp`: `update_if_higher` uses `get_score_by_hash`/`set_score_by_hash`; preserves `current_key_hash` across load
- `play_session.cpp`: uses stack buffer + `ensure_entry` + `current_key_hash` instead of heap string
- Tests: 5 new test cases replacing `make_key()` string tests with hash stability, collision, key_str format, update-never-lowers, and persistence-preserves-session-key tests

**Acceptance Criteria:**
- AC1 ✅: No heap std::string current-key storage; `current_key_hash` is uint32_t
- AC2 ✅: `static_cast<const char*>` forces runtime overload explicitly; comment documents why
- AC3 ✅: Persistence uses `char key[KEY_CAP]` entries serialized as JSON strings — round-trip stable
- AC4 ✅: `update_if_higher`, `get_current_high_score` behavior unchanged except representation
- AC5 ✅: 16 test cases / 85 assertions in `[high_score]`; all pass
- AC6 ✅: `git merge-tree` confirms clean auto-merge; `play_session.cpp` "changed in both" resolves cleanly (burnout+GamePhaseBit edits vs hash wiring are disjoint sections)

**Tests run:**
- Focused `[high_score]`: **85 assertions / 16 test cases — all pass** ✅
- Full suite: **2645 assertions / 815 test cases — all pass** ✅
- Build: zero warnings ✅

**Merge compatibility:** Branch is 1 commit ahead, 3 commits behind `origin/user/yashasg/ecs_refactor`. `play_session.cpp` "changed in both" — auto-merge is clean (burnout removal + GamePhaseBit from ecs_refactor and hash wiring from #313 touch disjoint lines). 5 burnout files "removed in remote" — no conflict since #313 doesn't touch them.

**Non-blocking notes:**
- `<string>` include in `high_score.h` is still needed for `HighScorePersistence::path` — correct scope
- `get_score_by_hash` re-hashes stored keys on every lookup (O(N × key_len)); negligible for N ≤ 9

**Reusable quality note:** When replacing a heap type (std::string) with a hash for a session-scoped field, the key pattern is: (1) `ensure_entry()` at session start to pre-register the string entry, (2) `current_key_hash` for zero-allocation in-session updates, (3) preserve hash across persistence loads. This decouples allocation from the hot update path while keeping stable string persistence.

**Verdict:** ✅ APPROVED

## Learnings

### 2026-05-17 — Review: issue #322 (precompute UI layout data into POD cache structs)

**Scope:** Keyser implementation — `HudLayout` and `LevelSelectLayout` POD structs in new `ui_layout_cache.h`; builder functions `build_hud_layout()` / `build_level_select_layout()` added to `ui_loader.cpp`/`ui_loader.h`; cache stored via `reg.ctx().insert_or_assign()` in `ui_navigation_system` on screen change; `ui_render_system` draw paths consume POD cache with no JSON access for HUD/level-select stable constants; 11 tests in `test_ui_layout_cache.cpp`.

**Commit reviewed:** f8e049a

**Merge compatibility:** `git merge-tree origin/user/yashasg/ecs_refactor HEAD` returns a single clean tree hash — zero conflicts. Disjoint file set (new file `ui_layout_cache.h`, additions to `ui_loader`, `ui_navigation_system`, `ui_render_system`, new test file; no overlap with #312/#318/#311/#314 changes).

**AC verification:**
- AC1 ✅ Render path: `find_el`, `json_color`, `element_map` — all removed from `ui_render_system.cpp`. Confirmed with grep. Remaining `overlay_screen` JSON access is for transient per-load overlay color, correctly out of scope.
- AC2 ✅ POD structs are plain data. Built at screen-change boundary (`ui_navigation_system` after `build_ui_element_map`). Stored in `reg.ctx()` consistent with existing singleton pattern.
- AC3 ✅ Render output is pixel-equivalent (same multiplications, same field names, same fallback values `corner_radius=0.1f/0.2f`). No behavioral delta.
- AC4 ✅ 11 tests: 3 invalid-input cases for HudLayout (empty, missing element, missing nested field), 2 valid/construction cases, 1 integration against shipped `gameplay.json`; 3 invalid-input cases for LevelSelectLayout (empty, missing song_cards, missing difficulty_buttons), 1 valid/construction, 1 integration against shipped `level_select.json`. All 64 assertions pass.
- AC5 ✅ `merge-tree` clean. No conflicts with current `origin/user/yashasg/ecs_refactor`.

**Tests run:**
- Focused `[layout_cache]`: **64 assertions in 11 test cases — all pass**
- Full suite: **2671 assertions in 824 test cases — all pass**
- Build: zero compiler warnings (two pre-existing linker dup-lib warnings from vcpkg worktree setup, not introduced by this branch)

**Verdict:** ✅ APPROVED

## Learnings
- **Reusable quality note (#322):** When caching JSON layout data into POD structs, use `.at()` (throws on missing key) inside a `try/catch(nlohmann::json::exception)` block — this gives an explicit WARN log and returns `valid=false` without crashing. Optional sub-elements (like `lane_divider`) should be guarded by a separate `find_el` + optional `try/catch` so their absence doesn't invalidate the parent layout. This pattern matches the repo's "graceful degradation with flag" convention.
- **Reusable quality note (#322):** Integration tests against shipped JSON files (e.g. `gameplay.json`, `level_select.json`) are the correct second tier of layout cache tests — they catch field-name divergence between code and content authoring without needing a running game window.

### 2026-04-27 — Review: Archetype move to `app/archetypes/` (issue #344)

**Scope:** Reviewer gate on uncommitted archetype relocation: `app/systems/obstacle_archetypes.*` → `app/archetypes/obstacle_archetypes.*`.

**Verdict:** APPROVE WITH NON-BLOCKING NOTES

**Evidence:**
- Build clean, zero warnings (`-Wall -Wextra -Werror`). Only pre-existing ld duplicate-lib noise (unrelated).
- All 11 archetype tests pass (64 assertions, `[archetype]` tag).
- Switch statement exhaustive across all 8 `ObstacleKind` values — no missing case.
- Both callers (`beat_scheduler_system.cpp`, `obstacle_spawn_system.cpp`) updated to `#include "archetypes/obstacle_archetypes.h"`.
- Test file updated to `#include "archetypes/obstacle_archetypes.h"`.
- No stale includes to old `app/systems/obstacle_archetypes.*` path found anywhere.
- CMake: `ARCHETYPE_SOURCES` glob covers `app/archetypes/*.cpp`; added to `shapeshifter_lib` alongside `SYSTEM_SOURCES`. Include root `app/` is PUBLIC on `shapeshifter_lib`, so all consumers resolve the path correctly.
- Old files confirmed deleted from working tree.

**Non-blocking notes:**
1. `ObstacleArchetypeInput` field comment says "mask: LaneBlock, ComboGate" — LanePush kinds also use the struct (mask ignored). Doc-only gap.
2. Random spawner still selects `LaneBlock` as a kind index then immediately remaps to LanePush — vestigial mapping. Existing design, not introduced here.

**Relevant paths:** `app/archetypes/obstacle_archetypes.h`, `app/archetypes/obstacle_archetypes.cpp`, `app/systems/beat_scheduler_system.cpp`, `app/systems/obstacle_spawn_system.cpp`, `tests/test_obstacle_archetypes.cpp`, `CMakeLists.txt`

### 2026-04-27T23:30:53Z — P0 TestFlight: ecs_refactor Archetype Review (Issue #344)

**Status:** APPROVED WITH NON-BLOCKING NOTES

**Role in manifest:** Reviewer (background validation)

**Verdict:** Move is correct. Confirmed:
- Old path deleted
- Callers/tests updated
- No stale includes
- CMake `ARCHETYPE_SOURCES` wired into `shapeshifter_lib`
- Include roots resolve

**Non-blocking notes:**
- `ObstacleArchetypeInput.mask` comment omits LanePush behavior
- Random spawner has vestigial LaneBlock→LanePush mapping — deferred to issue #344


### 2026-05-XX — #SQUAD Comment Cleanup Review (shape_vertices.h, transform.h)

**Status:** APPROVED

**Files reviewed:** `app/components/shape_vertices.h`, `app/components/transform.h`, `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp`, `app/systems/game_render_system.cpp`

**Verdict:** APPROVE. All removals are safe and correct.

**Confirmed:**
- `CIRCLE` table retained and actively used in `game_render_system.cpp` (rlgl 3D ring geometry, lines 107–117).
- `HEXAGON`, `SQUARE`, `TRIANGLE` tables had zero production references — confirmed by rg across app/, tests/, benchmarks/.
- Test deletions (HEXAGON/SQUARE/TRIANGLE test cases) exactly match deleted data. No orphaned tests.
- Benchmark deletion (hexagon iteration bench) matches deleted data.
- Zero `#SQUAD` markers remain.
- `Position`/`Velocity` retained as separate named structs is the **correct** ECS decision: distinct component types = distinct EnTT pools, no aliasing in registry views. Merging into `Vector2` would collapse two pools and invalidate view semantics across 21+ system files.
- Explanatory comment added to `transform.h` accurately documents the rationale.

**Non-blocking note (pre-existing, not introduced by this change):** `transform.h` includes `<cstdint>` but contains no `uint*` types. Orphaned include. Not a blocker; unrelated to this cleanup.

**Relevant paths:** `app/components/shape_vertices.h`, `app/components/transform.h`, `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp`, `app/systems/game_render_system.cpp`

### 2026-04-27T23:58:23Z — #SQUAD Comment Cleanup Review (Final)

**Status:** APPROVED

**Wave:** Code cleanup task — review gate for #SQUAD comment fixes

**Review summary:** Confirmed all changes in `#SQUAD` comment cleanup are safe and architecturally correct.

**shape_vertices.h:** `HEXAGON`/`SQUARE`/`TRIANGLE` removal safe (zero production references). `CIRCLE` retention necessary for `game_render_system.cpp` rlgl 3D geometry path.

**transform.h:** `Position`/`Velocity` retention is correct ECS architecture — separate structs = separate EnTT component pools = no aliasing in views across 21+ system files. Merging to `Vector2` would collapse pools and break structural view semantics.

**Tests/benchmarks:** All deletions align exactly with deleted data. Circle and floor ring coverage preserved. No orphaned tests.

**Non-blocking note (pre-existing):** `transform.h` includes `<cstdint>` with no `uint*` types (orphaned). Not introduced by this cleanup. Low priority.

**Decision records:** Approved and merged to decisions.md.

**Orchestration log:** `.squad/orchestration-log/2026-04-27T23:58:23Z-kujan.md`

### 2026-04-28 — Review: ECS Cleanup Batch #273/#333/#286/#336/#342/#340

**Scope:** Keaton (#273/#333), Hockney (#286), Baer (#336/#342), McManus (#340)

**Test baseline confirmed:** 2808 assertions / 840 test cases — all pass. Zero warnings.

**Verdict per issue:**
- **#273 (ButtonPressEvent semantic payload):** ✅ APPROVED. Entity handle fully replaced with value payload. Encoding at hit-test time. Stale-entity safety test present. Non-blocking: `player_input_system.cpp` section heading "EventQueue consumption contract" still refers to deleted struct by name.
- **#333 (InputEvent dispatcher Tier-1 + EventQueue deletion):** ✅ APPROVED. EventQueue deleted. R7 guard in place. Two-tier architecture documented. Tier-1 sinks registered, unwired, and tested. Non-blocking: R2 (no per-frame cap) — no regression observed; low risk.
- **#286 (SettingsState/HighScoreState helpers):** ✅ APPROVED. Both structs plain data; helpers in persistence namespaces. All call sites updated. 170 assertions pass.
- **#336 (miss_detection exclude-view regression):** ✅ APPROVED. 28 assertions / 5 test cases, live-verified. No PLATFORM guard.
- **#342 (signal lifecycle ungated):** ✅ APPROVED. 16 assertions / 6 test cases, live-verified. No PLATFORM guard. Doc discrepancy: inbox claimed 23 assertions, file has 16 (non-blocking).
- **#340 (SongState/DifficultyConfig ownership comments):** ✅ APPROVED. Comments-only. All system attributions verified against actual `app/systems/` files.

**All six issues are closure-ready.**

**Reusable quality notes:**
- When deleting a hand-rolled queue in favour of `entt::dispatcher`, update ALL doc comments referencing the deleted struct name — including section headings in consumer systems and component-level descriptions.
- Signal lifecycle tests (on_construct/on_destroy) should always be non-platform-gated; they test ECS wiring, not platform capabilities.
- Doc assertion counts in inbox files should match the file's actual CHECK/REQUIRE count — check before filing.

---

### 2026-05-18 — Review: UI ECS Batch (#337, #338, #339, #343)

**Scope:** Keyser-authored UI ECS batch. Code changes landed in `fdefeb1`; decision documentation in `ec891a9`.

**Verification performed:**

**#337 — UIActiveCache startup initialization:**
- `app/game_loop.cpp:118`: `reg.ctx().emplace<UIActiveCache>();` confirmed present.
- `tests/test_helpers.h:53`: `reg.ctx().emplace<UIActiveCache>();` confirmed present.
- `active_tag_system.cpp`: uses `reg.ctx().get<UIActiveCache>()` (hard crash if absent) — lazy `find/emplace` pattern removed.
- Diff confirmed in `fdefeb1`: exactly the lazy-init lines removed and replaced with bare `.get<>()` calls.
- Existing `[hit_test][active_tag]` tests cover both `invalidate_active_tag_cache` and `ensure_active_tags_synced` via `make_registry()`. All pass.

**#338 — Safe JSON access in spawn_ui_elements:**
- Diff confirmed in `fdefeb1`: all `operator[]` accesses on required fields replaced with `.at()`.
- Three error classes handled correctly:
  - Animation block (optional): `try/catch` → `[WARN]` stderr, entity preserved. ✅
  - Button required colors (`bg_color`, `border_color`, `text_color`): outer `try/catch` → `[WARN]` + `reg.destroy(e)` + `continue`. ✅
  - Shape required color: `try/catch` → `[WARN]` + `reg.destroy(e)` + `continue`. ✅
- **Non-blocking gap:** Zero tests exercise the error paths. All three `catch` blocks are untested. Future regression could silently delete them. Recommend a hardening ticket with tests for malformed-JSON spawn (missing `bg_color`, missing animation sub-key, missing shape `color`).

**#339 — UIState.current hashing:**
- `UIState.current` is `std::string` in `app/components/ui_state.h`. No code change needed or made.
- Rationale is sound: comparison at screen-load boundaries only, SSO handles short names, element-level hot-path already uses `entt::hashed_string` (#312).
- Decision documented in `keyser-ui-ecs-batch.md`. ✅

**#343 — Dynamic text allocation:**
- `resolve_ui_dynamic_text` called per-frame per entity in render system. ~2–5 entities, strings < 20 chars.
- SSO threshold (libc++ ~15–22 chars) means most are stack-local. 300 ops/sec max.
- Cache invalidation complexity (compare-each-frame or event wiring) > zero measurable benefit.
- Decision documented in `keyser-ui-ecs-batch.md`. ✅

**Test run:** `./run.sh test` — **2808 assertions / 840 test cases** — all pass. Independently verified.

**Verdict: ✅ APPROVED**

All four issues are closure-ready. One non-blocking hardening gap noted for #338 (error-path test coverage).

**Hardening ticket recommendation:** Add tests for `spawn_ui_elements` malformed JSON paths (animation missing sub-key, button missing `bg_color`, shape missing `color`) to ensure the try/catch error paths are regression-protected.

**Reusable quality note:**
- When adding defensive `try/catch` error paths in a spawn function, add at minimum one test per error class to lock in the behavior. Absence of error-path tests means future refactors can silently delete the catch blocks.

### 2026-04-27 — Review: #335 and #341 (Keaton, commit a9ed3fc)

**Scope:** TestPlayerState planned entity lifecycle contract documentation + SKILL.md ctx guidance correction.

**Findings:**
- #335: `LIFETIME CONTRACT` comment placed correctly above `planned[]`. Two new test cases verify stale-pruning and live-entry retention. `test_player_clean_planned()` confirmed as first substantive call in system tick (after frame counter increment, before all entity logic). Invariants match implementation.
- #341: SKILL.md table correctly split into two rows distinguishing `ctx().emplace<T>()` (startup one-time, absent-type required) from `ctx().insert_or_assign()` (upsert boundary). Misleading `NOT emplace_or_replace` comment removed. No gameplay code touched.
- Full suite: 2818 assertions / 842 test cases — all pass.

**Verdict:** ✅ APPROVED. Both issues closure-ready. No regressions.

### 2026-04-27 — Review: #346 (Baer, commits 710ff34 + ff96be8)

**Scope:** Expose `spawn_ui_elements()` from `ui_navigation_system.cpp` into `ui_loader.cpp/h`; add 12 regression tests for malformed-JSON catch branches.

**Background:** In the #338 batch review I flagged that all three catch branches inside `spawn_ui_elements()` lacked test coverage and recommended a hardening ticket. #346 is the direct resolution.

**Findings:**

- **Function relocation:** `spawn_ui_elements()` and its four helpers (`skip_for_platform`, `json_font`, `json_align`, `json_shape_kind`) faithfully moved from `ui_navigation_system.cpp` to `ui_loader.cpp`. Zero behavior change confirmed by diffing the two copies — identical logic.
- **`json_color_rl` null-safety patch:** Added `if (!arr.is_array() || arr.size() < 3) return WHITE;` guard. Correct and safe; the old version would dereference an unvalidated index.
- **Public API declaration:** Declaration in `ui_loader.h` includes a comment explaining the test-exposure rationale. Controlled and correct.
- **ui_navigation_system.cpp:** Removed all now-dead statics; `spawn_ui_elements(reg, ui.screen)` call unchanged. Clean.
- **Tests (12 test cases, 27 assertions):** All four catch branches exercised:
  - Text animation catch (missing `speed` key, wrong `alpha_range` type)
  - Button outer catch (missing `bg_color`, missing `border_color`)
  - Button inner animation catch (bad animation survives with `UIButton`)
  - Shape outer catch (missing `color` field)
  - Positive paths: valid text+animation, valid shape, mixed valid+invalid elements, empty/missing elements array.
  - All tests use in-memory JSON — deterministic, no on-disk fixtures.
- **Orphan entity for composite types (non-blocking observation):** `spawn_ui_elements()` does not handle `stat_table`, `card_list`, `line`, `burnout_meter`, etc. For these types, entities are created with `UIElementTag` + `Position` but no visual component. Pre-existing behavior, not introduced by this PR. The orphans are benign: multi-component render queries skip them; `destroy_ui_elements()` cleans them on screen transition. Composite types are served by the layout cache system, not the entity spawn path. Warrants a separate follow-on issue for explicit `continue` + debug-log on unknown type.
- **Build:** Zero compiler warnings (`-Wall -Wextra -Werror`). Clean.
- **Full suite:** 854 test cases, 2845 assertions — all pass. (Author reported 837/2845 — minor discrepancy likely from platform/environment; assertions match exactly.)

**Verdict: ✅ APPROVED — closure-ready.**

Non-blocking note: open a follow-on issue for `spawn_ui_elements` to `continue` + warn on unrecognized element types, to prevent silent orphan accumulation if new composite types are added to `kSupportedTypes` without a spawn branch.
