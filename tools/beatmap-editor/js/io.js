// io.js — Beatmap file I/O and validation (pure functions, no DOM except downloadFile)

import { KINDS_WITH_SHAPE, VALIDATION } from './constants.js';

/**
 * Parse a beatmap JSON string and normalize to internal state shape.
 * Handles both modern (difficulties object) and legacy (flat beats array) formats.
 * @param {string} jsonString
 * @returns {{ data: object|null, errors: string[] }}
 */
export function importBeatmap(jsonString) {
    const errors = [];
    let j;
    try {
        j = JSON.parse(jsonString);
    } catch (e) {
        return { data: null, errors: [e.message] };
    }

    const data = {
        songId: j.song_id ?? '',
        title: j.title ?? '',
        bpm: j.bpm ?? 120,
        offset: j.offset ?? 0,
        leadBeats: j.lead_beats ?? 4,
        duration: j.duration_sec ?? 180,
        songPath: j.song_path ?? '',
        difficulties: {},
    };

    if (j.difficulties && typeof j.difficulties === 'object') {
        // Modern format: nested difficulties
        for (const [name, diff] of Object.entries(j.difficulties)) {
            const beats = Array.isArray(diff.beats) ? diff.beats.map(normalizeBeat) : [];
            data.difficulties[name] = { beats };
        }
    } else if (Array.isArray(j.beats)) {
        // Legacy format: flat top-level beats array → put into "easy"
        data.difficulties.easy = { beats: j.beats.map(normalizeBeat) };
    }

    return { data, errors };
}

function normalizeBeat(b) {
    return {
        beat: b.beat ?? 0,
        kind: b.kind ?? 'shape_gate',
        shape: b.shape ?? 'circle',
        lane: b.lane ?? 1,
        blocked: b.blocked,
    };
}

/**
 * Serialize editor state to JSON matching the C++ loader's expected format.
 * @param {object} state
 * @returns {string} JSON string with 2-space indent
 */
export function exportBeatmap(state) {
    const out = {
        song_id: state.songId,
        title: state.title,
        bpm: state.bpm,
        offset: state.offset,
        lead_beats: state.leadBeats,
        duration_sec: state.duration,
        song_path: state.songPath,
        difficulties: {},
    };

    for (const [name, diff] of Object.entries(state.difficulties)) {
        if (!diff.beats || diff.beats.length === 0) continue;

        const sorted = [...diff.beats].sort((a, b) => a.beat - b.beat);
        out.difficulties[name] = {
            beats: sorted.map(exportBeatEntry),
        };
    }

    return JSON.stringify(out, null, 2);
}

function exportBeatEntry(b) {
    const entry = { beat: b.beat, kind: b.kind };

    if (KINDS_WITH_SHAPE.includes(b.kind) && b.shape != null) {
        entry.shape = b.shape;
    }

    entry.lane = b.lane;

    if (b.kind === 'lane_block' && Array.isArray(b.blocked)) {
        entry.blocked = b.blocked;
    }

    return entry;
}

/**
 * Parse an analysis JSON string and return a normalized object.
 * @param {string} jsonString
 * @returns {object|null}
 */
export function importAnalysis(jsonString) {
    let j;
    try {
        j = JSON.parse(jsonString);
    } catch {
        return null;
    }

    return {
        title: j.title ?? '',
        source: j.source ?? '',
        bpm: j.bpm ?? 0,
        duration: j.duration ?? 0,
        beats: Array.isArray(j.beats) ? j.beats : [],
        structure: Array.isArray(j.structure) ? j.structure : [],
        onsets: Array.isArray(j.onsets) ? j.onsets : [],
        events: Array.isArray(j.events) ? j.events : [],
        quietRegions: Array.isArray(j.quiet_regions) ? j.quiet_regions : [],
    };
}

/**
 * Validate the active difficulty's beats against all 8 rules.
 * @param {object} state - editor state with bpm, offset, leadBeats, duration, activeDifficulty, difficulties
 * @returns {Array<{beatIndex: number, message: string, severity: string}>}
 */
export function validate(state) {
    const errors = [];

    // Rule 1: BPM in [60, 300]
    if (state.bpm < VALIDATION.BPM_MIN || state.bpm > VALIDATION.BPM_MAX) {
        errors.push({
            beatIndex: -1,
            message: `BPM must be in range [${VALIDATION.BPM_MIN}, ${VALIDATION.BPM_MAX}], got ${state.bpm}`,
            severity: 'error',
        });
    }

    // Rule 2: Offset in [0.0, 5.0]
    if (state.offset < VALIDATION.OFFSET_MIN || state.offset > VALIDATION.OFFSET_MAX) {
        errors.push({
            beatIndex: -1,
            message: `Offset must be in range [${VALIDATION.OFFSET_MIN}, ${VALIDATION.OFFSET_MAX}], got ${state.offset}`,
            severity: 'error',
        });
    }

    // Rule 3: Lead beats in [2, 8]
    if (state.leadBeats < VALIDATION.LEAD_BEATS_MIN || state.leadBeats > VALIDATION.LEAD_BEATS_MAX) {
        errors.push({
            beatIndex: -1,
            message: `Lead beats must be in range [${VALIDATION.LEAD_BEATS_MIN}, ${VALIDATION.LEAD_BEATS_MAX}], got ${state.leadBeats}`,
            severity: 'error',
        });
    }

    // Get active difficulty beats
    const activeDiff = state.difficulties?.[state.activeDifficulty];
    const beats = activeDiff?.beats ?? [];

    // Rule 4: At least 1 beat entry
    if (beats.length === 0) {
        errors.push({
            beatIndex: -1,
            message: 'Beat map must have at least 1 beat entry',
            severity: 'warning',
        });
    }

    const beatPeriod = 60.0 / state.bpm;
    const maxBeat = Math.floor(state.duration / beatPeriod);

    let prevBeat = -1;
    let prevShapeBeat = -1;
    let prevShape = null;
    let prevHadShape = false;

    for (let i = 0; i < beats.length; i++) {
        const entry = beats[i];

        // Rule 5: Beat indices monotonically increasing
        if (entry.beat <= prevBeat && prevBeat >= 0) {
            errors.push({
                beatIndex: entry.beat,
                message: 'Beat indices must be monotonically increasing',
                severity: 'error',
            });
        }
        prevBeat = entry.beat;

        // Rule 6: No beat index beyond song duration
        if (entry.beat > maxBeat) {
            errors.push({
                beatIndex: entry.beat,
                message: 'Beat index exceeds song duration',
                severity: 'error',
            });
        }

        // Rule 7: shape_gate / split_path lane must be 0–2
        if (entry.kind === 'shape_gate' || entry.kind === 'split_path') {
            if (entry.lane < 0 || entry.lane > 2) {
                errors.push({
                    beatIndex: entry.beat,
                    message: 'Lane must be 0-2',
                    severity: 'error',
                });
            }
        }

        // Rule 8: Different-shape gates must be ≥ 3 beats apart
        const hasShape = KINDS_WITH_SHAPE.includes(entry.kind);
        if (hasShape && prevHadShape) {
            if (entry.shape !== prevShape &&
                (entry.beat - prevShapeBeat) < VALIDATION.MIN_SHAPE_CHANGE_GAP) {
                errors.push({
                    beatIndex: entry.beat,
                    message: 'Different-shape gates must be >= 3 beats apart',
                    severity: 'warning',
                });
            }
        }
        if (hasShape) {
            prevShapeBeat = entry.beat;
            prevShape = entry.shape;
            prevHadShape = true;
        }
    }

    return errors;
}

/**
 * Trigger a file download in the browser.
 * @param {string} filename
 * @param {string} content
 */
export function downloadFile(filename, content) {
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}
