// constants.js — Shared enums, colors, sizes for the beatmap editor
//
// Game-shared constants (obstacle_kinds, shapes, lanes, validation) are loaded
// from content/constants.json at init time. Editor-only constants (colors,
// glyphs, zoom, layout) stay here.

// ── Shared constants (populated by loadSharedConstants) ──────
export let OBSTACLE_KINDS = ["shape_gate", "lane_block", "low_bar", "high_bar", "combo_gate", "split_path"];
export let SHAPES = ["circle", "square", "triangle"];
export let LANES = [0, 1, 2];
export let KINDS_WITH_SHAPE = ["shape_gate", "combo_gate", "split_path"];
export let VALIDATION = {
    BPM_MIN: 60,
    BPM_MAX: 300,
    OFFSET_MIN: 0.0,
    OFFSET_MAX: 5.0,
    LEAD_BEATS_MIN: 2,
    LEAD_BEATS_MAX: 8,
    MIN_SHAPE_CHANGE_GAP: 3,
};

// Load shared constants from content/constants.json (call once at startup).
// Falls back to the defaults above if the fetch fails (e.g. file:// protocol).
export async function loadSharedConstants() {
    try {
        const resp = await fetch('../../content/constants.json');
        if (!resp.ok) return;
        const data = await resp.json();
        if (data.obstacle_kinds) OBSTACLE_KINDS = data.obstacle_kinds;
        if (data.shapes)         SHAPES = data.shapes;
        if (data.lanes)          LANES = data.lanes;
        if (data.kinds_with_shape) KINDS_WITH_SHAPE = data.kinds_with_shape;
        if (data.validation) {
            const v = data.validation;
            VALIDATION = {
                BPM_MIN:              v.bpm_min             ?? VALIDATION.BPM_MIN,
                BPM_MAX:              v.bpm_max             ?? VALIDATION.BPM_MAX,
                OFFSET_MIN:           v.offset_min          ?? VALIDATION.OFFSET_MIN,
                OFFSET_MAX:           v.offset_max          ?? VALIDATION.OFFSET_MAX,
                LEAD_BEATS_MIN:       v.lead_beats_min      ?? VALIDATION.LEAD_BEATS_MIN,
                LEAD_BEATS_MAX:       v.lead_beats_max      ?? VALIDATION.LEAD_BEATS_MAX,
                MIN_SHAPE_CHANGE_GAP: v.min_shape_change_gap ?? VALIDATION.MIN_SHAPE_CHANGE_GAP,
            };
        }
    } catch {
        // Fetch failed (file:// or missing file) — use hardcoded defaults
    }
}

// ── Editor-only constants ────────────────────────────────────
export const LANE_LABELS = ["Left", "Center", "Right"];

export const KIND_LABELS = {
    shape_gate: "ShapeGate",
    lane_block: "LaneBlock",
    low_bar: "LowBar",
    high_bar: "HighBar",
    combo_gate: "ComboGate",
    split_path: "SplitPath",
};

export const SHAPE_LABELS = {
    circle: "Circle",
    square: "Square",
    triangle: "Triangle",
};

export const COLORS = {
    bg: "#1a1a2e",
    panelBg: "#16213e",
    panelBorder: "#0f3460",
    text: "#e0e0e0",
    textMuted: "#888",
    accent: "#e94560",
    accentHover: "#ff6b81",
    gridLine: "#2a2a4a",
    gridLineStrong: "#3a3a6a",
    laneBg: ["#1e1e3a", "#1a1a32", "#1e1e3a"],
    laneBorder: "#2a2a4a",
    playhead: "#e94560",
    cursor: "rgba(233, 69, 96, 0.2)",
    selection: "rgba(100, 180, 255, 0.3)",
    selectionBorder: "#64b4ff",
    validationError: "#ff4444",
    validationWarning: "#ffaa00",

    kind: {
        shape_gate: "#4fc3f7",
        lane_block: "#ff8a65",
        low_bar: "#aed581",
        high_bar: "#ce93d8",
        combo_gate: "#fff176",
        split_path: "#4dd0e1",
    },

    shape: {
        circle: "#4fc3f7",
        square: "#ff8a65",
        triangle: "#aed581",
    },
};

export const GLYPHS = {
    shape_gate: "◆",
    lane_block: "▬",
    low_bar: "⌐",
    high_bar: "¬",
    combo_gate: "◈",
    split_path: "⑂",
};

export const SHAPE_GLYPHS = {
    circle: "●",
    square: "■",
    triangle: "▲",
};

export const DEFAULT_BPM = 120;
export const DEFAULT_OFFSET = 0;
export const DEFAULT_LEAD_BEATS = 4;
export const DEFAULT_DURATION = 180;
export const DEFAULT_ZOOM = 40; // pixels per beat
export const MIN_ZOOM = 10;
export const MAX_ZOOM = 200;
export const LANE_HEIGHT = 48;
export const HEADER_HEIGHT = 24;
export const WAVEFORM_HEIGHT = 60;
export const TIMELINE_PADDING_LEFT = 40;
