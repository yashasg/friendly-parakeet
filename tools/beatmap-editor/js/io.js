// io.js — Beatmap file I/O and validation (pure functions, no DOM except downloadFile)

import {
    OBSTACLE_KINDS, SHAPES, KINDS_WITH_SHAPE, DIFFICULTY_KEYS, RHYTHM_LAYER_KEYS, VALIDATION,
} from './constants.js';

const LEGACY_ONSET_LAYER_MAP = Object.freeze({
    kick: 'percussive',
    snare: 'percussive',
    hihat: 'percussive',
    melody: 'harmonic',
    flux: 'full-spectrum',
});

function isPlainObject(value) {
    return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function getAllowedKinds() {
    return OBSTACLE_KINDS;
}

function isAllowedKind(kind) {
    return OBSTACLE_KINDS.includes(kind);
}

function isAllowedShape(shape) {
    return SHAPES.includes(shape);
}

function isAllowedDifficultyKey(key) {
    return DIFFICULTY_KEYS.includes(key);
}

function normalizeOnsetLayers(onsets) {
    const warnings = [];
    const normalized = {};
    if (!isPlainObject(onsets)) {
        return { onsets: normalized, warnings };
    }

    for (const [sourceKey, value] of Object.entries(onsets)) {
        const layerKey = RHYTHM_LAYER_KEYS.includes(sourceKey)
            ? sourceKey
            : LEGACY_ONSET_LAYER_MAP[sourceKey];
        if (!layerKey) {
            warnings.push(`Dropped unsupported onset layer '${sourceKey}'`);
            continue;
        }
        if (!isPlainObject(value)) {
            warnings.push(`Dropped onset layer '${sourceKey}' because it must be an object`);
            continue;
        }

        const timestamps = Array.isArray(value.timestamps) ? value.timestamps : [];
        const target = normalized[layerKey] ?? { timestamps: [] };
        target.timestamps.push(...timestamps);
        normalized[layerKey] = target;

        if (layerKey !== sourceKey) {
            warnings.push(`Mapped legacy onset layer '${sourceKey}' to '${layerKey}'`);
        }
    }

    return { onsets: normalized, warnings };
}

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

    if (!isPlainObject(j)) {
        return { data: null, errors: ['Beatmap root must be a JSON object'] };
    }

    const data = {
        songId: '',
        title: '',
        bpm: 120,
        offset: 0,
        leadBeats: 4,
        duration: 180,
        songPath: '',
        hasSongPath: false,
        difficulties: {},
        extraTopLevel: {},
    };
    const reservedTopLevel = new Set([
        'song_id',
        'title',
        'bpm',
        'offset',
        'lead_beats',
        'duration_sec',
        'song_path',
        'difficulties',
        'beats',
    ]);
    data.extraTopLevel = Object.fromEntries(
        Object.entries(j).filter(([key]) => !reservedTopLevel.has(key)),
    );

    if (j.song_id !== undefined) {
        if (typeof j.song_id !== 'string') errors.push('song_id must be a string');
        else data.songId = j.song_id;
    }
    if (j.title !== undefined) {
        if (typeof j.title !== 'string') errors.push('title must be a string');
        else data.title = j.title;
    }
    if (j.bpm !== undefined) {
        if (!Number.isFinite(j.bpm)) errors.push('bpm must be a finite number');
        else data.bpm = j.bpm;
    }
    if (j.offset !== undefined) {
        if (!Number.isFinite(j.offset)) errors.push('offset must be a finite number');
        else data.offset = j.offset;
    }
    if (j.lead_beats !== undefined) {
        if (!Number.isInteger(j.lead_beats)) errors.push('lead_beats must be an integer');
        else data.leadBeats = j.lead_beats;
    }
    if (j.duration_sec !== undefined) {
        if (!Number.isFinite(j.duration_sec)) errors.push('duration_sec must be a finite number');
        else data.duration = j.duration_sec;
    }
    if (j.song_path !== undefined) {
        if (typeof j.song_path !== 'string') errors.push('song_path must be a string');
        else {
            data.songPath = j.song_path;
            data.hasSongPath = true;
        }
    }

    if (j.difficulties !== undefined) {
        if (!isPlainObject(j.difficulties)) {
            errors.push('difficulties must be an object');
        } else {
        // Modern format: nested difficulties
            for (const [name, diff] of Object.entries(j.difficulties)) {
                if (!isAllowedDifficultyKey(name)) {
                    errors.push(`Unsupported difficulty key '${name}'`);
                    continue;
                }
                if (!isPlainObject(diff)) {
                    errors.push(`difficulties.${name} must be an object`);
                    continue;
                }
                if (!Array.isArray(diff.beats)) {
                    errors.push(`difficulties.${name}.beats must be an array`);
                    continue;
                }
                const beats = [];
                for (let i = 0; i < diff.beats.length; i++) {
                    const normalized = normalizeBeat(diff.beats[i], `difficulties.${name}.beats[${i}]`, errors);
                    if (normalized) beats.push(normalized);
                }
                const reservedDifficulty = new Set(['beats', 'count']);
                data.difficulties[name] = {
                    ...Object.fromEntries(
                        Object.entries(diff).filter(([key]) => !reservedDifficulty.has(key)),
                    ),
                    beats,
                    ...(diff.count !== undefined ? { count: diff.count } : {}),
                };
            }
        }
    } else if (Array.isArray(j.beats)) {
        // Legacy format: flat top-level beats array → put into "easy"
        const beats = [];
        for (let i = 0; i < j.beats.length; i++) {
            const normalized = normalizeBeat(j.beats[i], `beats[${i}]`, errors);
            if (normalized) beats.push(normalized);
        }
        data.difficulties.easy = { beats };
    } else if (j.beats !== undefined) {
        errors.push('beats must be an array');
    } else {
        errors.push('beatmap must include difficulties or beats');
    }

    return { data, errors };
}

function normalizeBeat(b, path, errors) {
    if (!isPlainObject(b)) {
        errors.push(`${path} must be an object`);
        return null;
    }

    let valid = true;
    const beat = b.beat ?? 0;
    const kind = b.kind ?? 'shape_gate';
    const shape = b.shape ?? 'circle';
    const lane = b.lane ?? 1;

    if (!Number.isInteger(beat)) {
        errors.push(`${path}.beat must be an integer`);
        valid = false;
    }

    if (typeof kind !== 'string' || !isAllowedKind(kind)) {
        errors.push(`${path}.kind must be one of: ${[...getAllowedKinds()].join(', ')}`);
        valid = false;
    }

    if (b.shape !== undefined && (typeof b.shape !== 'string' || !isAllowedShape(b.shape))) {
        errors.push(`${path}.shape must be one of: ${SHAPES.join(', ')}`);
        valid = false;
    }

    if (KINDS_WITH_SHAPE.includes(kind) && !isAllowedShape(shape)) {
        errors.push(`${path}.shape is required and must be one of: ${SHAPES.join(', ')}`);
        valid = false;
    }

    if (!Number.isInteger(lane)) {
        errors.push(`${path}.lane must be an integer`);
        valid = false;
    }

    if (b.blocked !== undefined) {
        errors.push(`${path}.blocked is not supported by active beatmap obstacle kinds`);
        valid = false;
    }

    if (!valid) return null;

    return {
        ...b,
        beat,
        kind,
        shape,
        lane,
    };
}

/**
 * Serialize editor state to JSON matching the C++ loader's expected format.
 * @param {object} state
 * @returns {string} JSON string with 2-space indent
 */
export function exportBeatmap(state) {
    const errors = [];
    const validationErrors = collectExportValidationErrors(state)
        .filter((err) => err.severity === 'error');
    for (const err of validationErrors) {
        errors.push(err.beatIndex >= 0 ? `beat ${err.beatIndex}: ${err.message}` : err.message);
    }
    const out = {
        ...(state.extraTopLevel || {}),
        song_id: state.songId,
        title: state.title,
        bpm: state.bpm,
        offset: state.offset,
        lead_beats: state.leadBeats,
        duration_sec: state.duration,
        difficulties: {},
    };
    if (state.songPath || state.hasSongPath) {
        out.song_path = state.songPath;
    }

    for (const [name, diff] of Object.entries(state.difficulties || {})) {
        if (!isAllowedDifficultyKey(name)) {
            errors.push(`Unsupported difficulty key '${name}' for export`);
            continue;
        }
        if (!isPlainObject(diff) || !Array.isArray(diff.beats)) {
            errors.push(`difficulties.${name}.beats must be an array`);
            continue;
        }
        if (!diff.beats || diff.beats.length === 0) continue;

        const sorted = [...diff.beats].sort((a, b) => a.beat - b.beat);
        const exportedBeats = [];
        for (let i = 0; i < sorted.length; i++) {
            const result = exportBeatEntry(sorted[i], `difficulties.${name}.beats[${i}]`);
            if (result.error) {
                errors.push(result.error);
            } else {
                exportedBeats.push(result.entry);
            }
        }
        out.difficulties[name] = {
            ...Object.fromEntries(
                Object.entries(diff).filter(([key]) => key !== 'beats'),
            ),
            beats: exportedBeats,
        };
    }

    if (errors.length > 0) {
        throw new Error(errors.join('\n'));
    }

    return JSON.stringify(out, null, 2);
}

function collectExportValidationErrors(state) {
    const difficulties = state.difficulties || {};
    const keys = Object.keys(difficulties).filter(isAllowedDifficultyKey);
    if (keys.length === 0) {
        return validate(state);
    }

    const unique = new Map();
    for (const key of keys) {
        for (const err of validate({ ...state, activeDifficulty: key })) {
            const id = `${err.severity}:${err.beatIndex}:${err.message}`;
            if (!unique.has(id)) {
                unique.set(id, err);
            }
        }
    }
    return [...unique.values()];
}

function exportBeatEntry(b, path) {
    if (!isPlainObject(b)) {
        return { error: `${path} must be an object` };
    }

    if (!Number.isInteger(b.beat)) {
        return { error: `${path}.beat must be an integer` };
    }

    if (typeof b.kind !== 'string' || !isAllowedKind(b.kind)) {
        return { error: `${path}.kind must be one of: ${[...getAllowedKinds()].join(', ')}` };
    }

    if (!Number.isInteger(b.lane)) {
        return { error: `${path}.lane must be an integer` };
    }
    if ((b.kind === 'shape_gate' || b.kind === 'split_path') &&
        (b.lane < 0 || b.lane > 2)) {
        return { error: `${path}.lane must be in range 0-2` };
    }

    if (b.blocked !== undefined) {
        return { error: `${path}.blocked is not supported by active beatmap obstacle kinds` };
    }

    const entry = { ...b, beat: b.beat, kind: b.kind, lane: b.lane };

    if (KINDS_WITH_SHAPE.includes(b.kind)) {
        if (typeof b.shape !== 'string' || !isAllowedShape(b.shape)) {
            return { error: `${path}.shape must be one of: ${SHAPES.join(', ')}` };
        }
        entry.shape = b.shape;
    }

    if (b.shape !== undefined && !KINDS_WITH_SHAPE.includes(b.kind)) {
        if (typeof b.shape !== 'string' || !isAllowedShape(b.shape)) {
            return { error: `${path}.shape must be one of: ${SHAPES.join(', ')}` };
        }
    }

    return { entry };
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

    const normalizedOnsets = normalizeOnsetLayers(j.onsets);

    return {
        title: j.title ?? '',
        source: j.source ?? '',
        bpm: j.bpm ?? 0,
        duration: j.duration ?? 0,
        beats: Array.isArray(j.beats) ? j.beats : [],
        structure: Array.isArray(j.structure) ? j.structure : [],
        onsets: normalizedOnsets.onsets,
        events: Array.isArray(j.events) ? j.events : [],
        quietRegions: Array.isArray(j.quiet_regions) ? j.quiet_regions : [],
        warnings: normalizedOnsets.warnings,
    };
}

/**
 * Validate the active difficulty's beats against all 8 rules.
 * @param {object} state - editor state with bpm, offset, leadBeats, duration, activeDifficulty, difficulties
 * @returns {Array<{beatIndex: number, message: string, severity: string}>}
 */
export function validate(state) {
    const errors = [];

    for (const key of Object.keys(state.difficulties || {})) {
        if (!isAllowedDifficultyKey(key)) {
            errors.push({
                beatIndex: -1,
                message: `Unsupported difficulty key '${key}'`,
                severity: 'error',
            });
        }
    }

    // Rule 1: BPM in [60, 300]
    if (state.bpm < VALIDATION.BPM_MIN || state.bpm > VALIDATION.BPM_MAX) {
        errors.push({
            beatIndex: -1,
            message: `BPM must be in range [${VALIDATION.BPM_MIN}, ${VALIDATION.BPM_MAX}], got ${state.bpm}`,
            severity: 'error',
        });
    }

    // Rule 2: Offset in shared content/constants.json range.
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
    if (!isAllowedDifficultyKey(state.activeDifficulty)) {
        errors.push({
            beatIndex: -1,
            message: `Unsupported active difficulty '${state.activeDifficulty}'`,
            severity: 'error',
        });
    }
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
    const beatTimes = state.extraTopLevel?.beat_times;
    const hasBeatTimes = beatTimes !== undefined;
    let beatTimesValid = true;
    if (hasBeatTimes) {
        if (!Array.isArray(beatTimes)) {
            beatTimesValid = false;
            errors.push({
                beatIndex: -1,
                message: 'beat_times must be an array when provided',
                severity: 'error',
            });
        } else {
            for (let i = 0; i < beatTimes.length; i++) {
                const value = beatTimes[i];
                if (typeof value !== 'number' || !Number.isFinite(value)) {
                    beatTimesValid = false;
                    errors.push({
                        beatIndex: i,
                        message: 'beat_times entries must be finite numbers',
                        severity: 'error',
                    });
                    break;
                }
                if (i > 0 && value < beatTimes[i - 1]) {
                    beatTimesValid = false;
                    errors.push({
                        beatIndex: i,
                        message: 'beat_times entries must be non-decreasing',
                        severity: 'error',
                    });
                    break;
                }
            }
        }
    }

    let prevBeat = -1;
    let prevAuthoredTime = null;
    let prevShapeBeat = -1;
    let prevShape = null;
    let prevHadShape = false;

    for (let i = 0; i < beats.length; i++) {
        const entry = beats[i];
        const beatIndex = Number.isInteger(entry.beat) ? entry.beat : -1;

        if (!Number.isInteger(entry.beat)) {
            errors.push({
                beatIndex: -1,
                message: 'Beat index must be an integer',
                severity: 'error',
            });
            continue;
        }

        if (typeof entry.kind !== 'string' || !isAllowedKind(entry.kind)) {
            errors.push({
                beatIndex,
                message: `Unknown obstacle kind '${entry.kind}'`,
                severity: 'error',
            });
            continue;
        }

        if (KINDS_WITH_SHAPE.includes(entry.kind) &&
            (typeof entry.shape !== 'string' || !isAllowedShape(entry.shape))) {
            errors.push({
                beatIndex,
                message: `Shape must be one of: ${SHAPES.join(', ')}`,
                severity: 'error',
            });
        }

        if (entry.shape !== undefined &&
            (typeof entry.shape !== 'string' || !isAllowedShape(entry.shape))) {
            errors.push({
                beatIndex,
                message: `Shape must be one of: ${SHAPES.join(', ')}`,
                severity: 'error',
            });
        }

        // Rule 5: Beat indices monotonically non-decreasing. The C++ loader
        // allows multiple authored entries on the same beat when their
        // resolved time_sec values are non-decreasing.
        if (entry.beat < prevBeat && prevBeat >= 0) {
            errors.push({
                beatIndex: entry.beat,
                message: 'Beat indices must be monotonically non-decreasing',
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
        if (hasBeatTimes && beatTimesValid && (entry.beat < 0 || entry.beat >= beatTimes.length)) {
            errors.push({
                beatIndex: entry.beat,
                message: 'Beat index is out of range for beat_times array',
                severity: 'error',
            });
        }

        if (entry.time_sec !== undefined) {
            if (!Number.isFinite(entry.time_sec)) {
                errors.push({
                    beatIndex: entry.beat,
                    message: 'time_sec must be finite when provided',
                    severity: 'error',
                });
            } else {
                if (entry.time_sec < 0) {
                    errors.push({
                        beatIndex: entry.beat,
                        message: 'time_sec must be >= 0 when provided',
                        severity: 'error',
                    });
                }
                if (entry.time_sec > state.duration) {
                    errors.push({
                        beatIndex: entry.beat,
                        message: 'time_sec must be <= duration_sec when provided',
                        severity: 'error',
                    });
                }
                if (prevAuthoredTime !== null && entry.time_sec < prevAuthoredTime) {
                    errors.push({
                        beatIndex: entry.beat,
                        message: 'time_sec must be non-decreasing across authored beat entries',
                        severity: 'error',
                    });
                }
                prevAuthoredTime = entry.time_sec;
            }
        }

        // Rule 7: shape_gate / split_path lane must be 0–2
        if (entry.kind === 'shape_gate' || entry.kind === 'split_path') {
            if (!Number.isInteger(entry.lane) || entry.lane < 0 || entry.lane > 2) {
                errors.push({
                    beatIndex: entry.beat,
                    message: 'Lane must be 0-2',
                    severity: 'error',
                });
            }
        }

        if (entry.blocked !== undefined) {
            errors.push({
                beatIndex: entry.beat,
                message: 'blocked is not supported by active beatmap obstacle kinds',
                severity: 'error',
            });
        }

        // Rule 8: Different-shape gates must be ≥ 3 beats apart
        const hasShape = KINDS_WITH_SHAPE.includes(entry.kind);
        if (hasShape && prevHadShape) {
            if (entry.shape !== prevShape &&
                (entry.beat - prevShapeBeat) < VALIDATION.MIN_SHAPE_CHANGE_GAP) {
                errors.push({
                    beatIndex: entry.beat,
                    message: 'Different-shape gates must be >= 3 beats apart',
                    severity: 'error',
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
