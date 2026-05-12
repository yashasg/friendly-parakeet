# Rabin — History (Condensed)

**Last Updated:** 2026-05-11  
**Current Focus:** Level content fixes & validators — autonomous quality loop  

## Core Context

- **Owner:** yashasg
- **Project:** C++20 raylib/EnTT rhythm bullet hell game
- **Role:** Level Designer  
- **Joined:** 2026-04-26T02:12:00.632Z

## Expertise

**Beatmap Generation & Content QA:** Shipped generator fixes for #391/#392/#394/#396 (IOI ramps, lane-run caps, subdivision-aware snapping); fixed 4 validators (#443/#447/#449/#452) for onset-only beatmap semantics; filed 8 content issues across 3 songs.

**Key Learnings:**
- Canonical mapping: `ONSET_CLASS_TO_OBSTACLE` (C++), not legacy `SHAPE_TO_LANE` (Python inverted)
- Cross-layer simultaneity <50ms at same beat with different onset_class is intentional
- Onset-sequential `beat` ordinal invalidates beat-gap validators; must switch to time-based gates
- Fallback IOI pool enrichment via `classify_onset_class` prevents lane-1 explosions

## Recent Status (2026-05-11)

**Shipped in commit 24e8c95 (Round 3 fix-pass):**
- #449: Medium shape distribution rebalanced (was inverted vs spec; fixed via Python↔C++ alignment)
- #447: Onset-aware offset semantics (skip beat-formula drift, use onset_time_sec instead)
- #452: Time-based max-beat-gap validator for onset ordinals
- #443: Loop2 + gap-one gates demoted in onset mode (cross-layer exemptions added)

**Test Status:** 727/728 pass (one pre-existing UI failure, not from this work)

**Open Blockers:** Real silence gaps (8–27s per tier) routed to content/generator owner; #135 shape variety floor affirmed.
