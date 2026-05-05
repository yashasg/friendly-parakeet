# Skill: Validator-Parity Regression Tests

## When to use
When C++ regression tests validate shipped content that is also governed by standalone validation scripts (e.g., Python beatmap validators).

## Pattern
1. Identify the current acceptance contracts in validator scripts.
2. Remove stale assertions tied to retired mechanics.
3. Re-express C++ tests in terms of active contracts (coverage, readability windows, forbidden legacy kinds).
4. Keep strict existing lower-difficulty guards intact (do not weaken easy-mode constraints).
5. Validate with both unit/integration tests and the external validator scripts.
6. Diff numeric thresholds and policy text across C++ and Python surfaces (same rule should have one source-of-truth constants file or generated bindings).
7. If CI ignores `tools/**`, treat Python validators as non-gating and explicitly label them advisory until wired into workflow jobs.
8. Ban source-text-grep assertions for behavior contracts; assert runtime outputs (components/events) instead.

## Why
This prevents test drift where C++ tests fail for intentionally removed mechanics, while preserving release-safety checks for shipped assets.
