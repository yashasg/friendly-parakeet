// timeline.js — Pure canvas renderer for the beatmap timeline.
// Reads state and draws to canvas. Never mutates state.
// Also provides hit testing and coordinate conversion.

import {
    COLORS, GLYPHS, SHAPE_GLYPHS, KINDS_WITH_SHAPE,
    LANE_HEIGHT,
    HEADER_HEIGHT,
    WAVEFORM_HEIGHT,
    TIMELINE_PADDING_LEFT,
    LANE_LABELS,
} from './constants.js';

let canvas = null;
let ctx = null;
let dpr = 1;

// Cached state reference for hitTest (set each render call)
let lastState = null;

// ─── Coordinate Conversion ────────────────────────────────────────

export function beatToX(beat, state) {
    return TIMELINE_PADDING_LEFT + (beat * state.zoom) - state.scrollX;
}

export function xToBeat(x, state) {
    const raw = (x + state.scrollX - TIMELINE_PADDING_LEFT) / state.zoom;
    return Math.max(0, Math.round(raw));
}

export function timeToX(seconds, state) {
    const beat = (seconds - state.offset) * (state.bpm / 60);
    return beatToX(beat, state);
}

// ─── Init ─────────────────────────────────────────────────────────

export function init(canvasElement) {
    canvas = canvasElement;
    ctx = canvas.getContext('2d');
    dpr = window.devicePixelRatio || 1;

    function resize() {
        const container = canvas.parentElement;
        if (!container) return;
        const rect = container.getBoundingClientRect();
        canvas.width = rect.width * dpr;
        canvas.height = rect.height * dpr;
        canvas.style.width = rect.width + 'px';
        canvas.style.height = rect.height + 'px';
        ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    }

    resize();

    const ro = new ResizeObserver(resize);
    ro.observe(canvas.parentElement);
}

// ─── Hit Test ─────────────────────────────────────────────────────

export function hitTest(canvasX, canvasY) {
    const state = lastState;
    if (!state) return { beat: 0, lane: -1, obstacleIndex: null };

    const beat = xToBeat(canvasX, state);

    // Determine lane from Y position
    const laneAreaTop = HEADER_HEIGHT;
    const laneAreaBottom = HEADER_HEIGHT + 3 * LANE_HEIGHT;
    let lane = -1;
    if (canvasY >= laneAreaTop && canvasY < laneAreaBottom) {
        lane = Math.floor((canvasY - laneAreaTop) / LANE_HEIGHT);
        if (lane > 2) lane = 2;
    }

    // Check if an existing obstacle was clicked.
    // Beat values must be unique (validation enforces monotonic beat order with
    // no duplicates), so we search by beat only, not by beat+lane combination.
    let obstacleIndex = null;
    const beats = state.beats;
    if (beats) {
        for (let i = 0; i < beats.length; i++) {
            const entry = beats[i];
            if (entry.beat === beat) {
                // Verify click is within the glyph's horizontal bounding area
                const cx = beatToX(entry.beat, state);
                const halfCell = state.zoom / 2;
                if (Math.abs(canvasX - cx) <= halfCell) {
                    obstacleIndex = i;
                    break;
                }
            }
        }
    }

    return { beat, lane, obstacleIndex };
}

// ─── Render ───────────────────────────────────────────────────────

export function render(state, waveformData, validationErrors) {
    if (!canvas || !ctx) return;

    lastState = state;
    const w = canvas.width / dpr;
    const h = canvas.height / dpr;

    // Determine visible beat range for culling
    const firstBeat = Math.max(0, Math.floor((state.scrollX - TIMELINE_PADDING_LEFT) / state.zoom) - 1);
    const lastBeat = Math.ceil((state.scrollX + w) / state.zoom) + 1;

    // 1. Clear background
    ctx.fillStyle = COLORS.bg;
    ctx.fillRect(0, 0, w, h);

    // 2. Analysis overlay
    renderAnalysisOverlay(ctx, state, w);

    // 3. Lane bands
    renderLaneBands(ctx, w);

    // 4. Waveform
    if (waveformData) {
        renderWaveform(ctx, state, waveformData, w);
    }

    // 5. Beat grid lines
    renderBeatGrid(ctx, state, firstBeat, lastBeat, h);

    // 6. Beat number labels
    renderBeatLabels(ctx, state, firstBeat, lastBeat);

    // 7. Lane labels
    renderLaneLabels(ctx);

    // 8. Cursor highlight
    renderCursorHighlight(ctx, state);

    // 9. Obstacle glyphs
    renderObstacles(ctx, state, firstBeat, lastBeat);

    // 10. Selection highlights
    renderSelectionHighlights(ctx, state);

    // 11. Playhead
    renderPlayhead(ctx, state, h);

    // 12. Legend
    renderLegend(ctx, w, h);
}

// ─── Render Helpers ───────────────────────────────────────────────

function renderAnalysisOverlay(ctx, state, w) {
    if (!state.analysisData || !state.analysisData.structure) return;
    const sections = state.analysisData.structure;
    const laneAreaTop = HEADER_HEIGHT;
    const laneAreaBottom = HEADER_HEIGHT + 3 * LANE_HEIGHT;
    const bandHeight = laneAreaBottom - laneAreaTop;

    for (const section of sections) {
        const x0 = timeToX(section.start, state);
        const x1 = timeToX(section.end, state);
        if (x1 < 0 || x0 > w) continue;

        let color;
        const intensity = section.intensity || 'low';
        if (intensity === 'high') {
            color = 'rgba(255, 60, 60, 0.1)';
        } else if (intensity === 'medium') {
            color = 'rgba(255, 220, 50, 0.1)';
        } else {
            color = 'rgba(80, 200, 80, 0.1)';
        }

        ctx.fillStyle = color;
        ctx.fillRect(Math.max(x0, TIMELINE_PADDING_LEFT), laneAreaTop, x1 - Math.max(x0, TIMELINE_PADDING_LEFT), bandHeight);
    }
}

function renderLaneBands(ctx, w) {
    const laneAreaTop = HEADER_HEIGHT;
    for (let i = 0; i < 3; i++) {
        const y = laneAreaTop + i * LANE_HEIGHT;
        ctx.fillStyle = COLORS.laneBg[i];
        ctx.fillRect(0, y, w, LANE_HEIGHT);

        // Lane border (bottom edge)
        if (i < 2) {
            ctx.fillStyle = COLORS.laneBorder;
            ctx.fillRect(0, y + LANE_HEIGHT - 1, w, 1);
        }
    }
    // Bottom border of last lane
    ctx.fillStyle = COLORS.laneBorder;
    ctx.fillRect(0, laneAreaTop + 3 * LANE_HEIGHT - 1, w, 1);
}

function renderBeatGrid(ctx, state, firstBeat, lastBeat, h) {
    for (let b = firstBeat; b <= lastBeat; b++) {
        const x = beatToX(b, state);
        if (x < TIMELINE_PADDING_LEFT || x > ctx.canvas.width / dpr) continue;

        ctx.beginPath();
        ctx.moveTo(x, HEADER_HEIGHT);
        ctx.lineTo(x, h);
        ctx.strokeStyle = (b % 4 === 0) ? COLORS.gridLineStrong : COLORS.gridLine;
        ctx.lineWidth = 1;
        ctx.stroke();
    }
}

function renderBeatLabels(ctx, state, firstBeat, lastBeat) {
    ctx.fillStyle = COLORS.textMuted;
    ctx.font = '11px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    for (let b = firstBeat; b <= lastBeat; b++) {
        const x = beatToX(b, state);
        if (x < TIMELINE_PADDING_LEFT || x > ctx.canvas.width / dpr) continue;
        // Show label every beat if zoom is large enough, else every 4th
        if (state.zoom >= 20 || b % 4 === 0) {
            ctx.fillText(String(b), x, HEADER_HEIGHT / 2);
        }
    }
}

function renderLaneLabels(ctx) {
    ctx.fillStyle = COLORS.textMuted;
    ctx.font = '10px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    for (let i = 0; i < 3; i++) {
        const y = HEADER_HEIGHT + i * LANE_HEIGHT + LANE_HEIGHT / 2;
        ctx.fillText(LANE_LABELS[i], TIMELINE_PADDING_LEFT / 2, y);
    }
}

function renderCursorHighlight(ctx, state) {
    if (!state.cursor || state.cursor.beat < 0 || state.cursor.lane < 0) return;

    const x = beatToX(state.cursor.beat, state) - state.zoom / 2;
    const y = HEADER_HEIGHT + state.cursor.lane * LANE_HEIGHT;

    ctx.fillStyle = COLORS.cursor;
    ctx.fillRect(x, y, state.zoom, LANE_HEIGHT);
}

function renderObstacles(ctx, state, firstBeat, lastBeat) {
    const beats = state.beats;
    if (!beats) return;

    ctx.font = '20px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    for (let i = 0; i < beats.length; i++) {
        const entry = beats[i];
        if (entry.beat < firstBeat || entry.beat > lastBeat) continue;

        const x = beatToX(entry.beat, state);
        const y = HEADER_HEIGHT + entry.lane * LANE_HEIGHT + LANE_HEIGHT / 2;

        if (KINDS_WITH_SHAPE.includes(entry.kind)) {
            const glyph = SHAPE_GLYPHS[entry.shape] || '?';
            ctx.fillStyle = (COLORS.shape[entry.shape]) || COLORS.text;
            ctx.fillText(glyph, x, y);
        } else {
            const glyph = GLYPHS[entry.kind] || '?';
            ctx.fillStyle = (COLORS.kind[entry.kind]) || COLORS.text;
            ctx.fillText(glyph, x, y);
        }
    }
}

function renderSelectionHighlights(ctx, state) {
    const beats = state.beats;
    const sel = state.selectedIndices;
    if (!beats || !sel || sel.length === 0) return;

    for (const idx of sel) {
        if (idx < 0 || idx >= beats.length) continue;
        const entry = beats[idx];
        const x = beatToX(entry.beat, state) - state.zoom / 2;
        const y = HEADER_HEIGHT + entry.lane * LANE_HEIGHT;

        // Fill
        ctx.fillStyle = COLORS.selection;
        ctx.fillRect(x, y, state.zoom, LANE_HEIGHT);

        // Border
        ctx.strokeStyle = COLORS.selectionBorder;
        ctx.lineWidth = 1.5;
        ctx.strokeRect(x + 0.5, y + 0.5, state.zoom - 1, LANE_HEIGHT - 1);
    }
}

function renderPlayhead(ctx, state, h) {
    if (state.playheadTime == null) return;
    const x = timeToX(state.playheadTime, state);
    if (x < TIMELINE_PADDING_LEFT) return;

    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.strokeStyle = COLORS.playhead;
    ctx.lineWidth = 2;
    ctx.stroke();
}

function renderWaveform(ctx, state, waveformData, w) {
    const top = HEADER_HEIGHT + 3 * LANE_HEIGHT;
    const height = WAVEFORM_HEIGHT;
    const midY = top + height / 2;

    ctx.save();
    ctx.globalAlpha = 0.5;
    ctx.fillStyle = COLORS.accent;

    const sampleCount = waveformData.length;
    if (sampleCount === 0) { ctx.restore(); return; }

    // Map waveform samples to beat-time space so the waveform aligns with
    // the timeline and respects scroll/zoom.  waveformData[i] is the peak
    // amplitude for the audio segment at time (i/sampleCount)*duration.
    const duration = state.duration || 180;
    const bpm = state.bpm || 120;
    const totalBeats = duration * (bpm / 60);
    const barWidth = Math.max(1, (totalBeats / sampleCount) * state.zoom);
    for (let i = 0; i < sampleCount; i++) {
        const beatPos = (i / sampleCount) * totalBeats;
        const x = beatToX(beatPos, state);
        if (x + barWidth < TIMELINE_PADDING_LEFT || x > w) continue;

        const peak = waveformData[i];
        const barH = peak * (height / 2);
        ctx.fillRect(x, midY - barH, barWidth, barH * 2);
    }

    ctx.restore();
}

function renderLegend(ctx, w, h) {
    const items = [
        { glyph: SHAPE_GLYPHS.circle,      color: COLORS.shape.circle,          label: 'Circle' },
        { glyph: SHAPE_GLYPHS.square,      color: COLORS.shape.square,          label: 'Square' },
        { glyph: SHAPE_GLYPHS.triangle,    color: COLORS.shape.triangle,        label: 'Triangle' },
        { glyph: GLYPHS.low_bar,           color: COLORS.kind.low_bar,          label: 'LowBar' },
        { glyph: GLYPHS.high_bar,          color: COLORS.kind.high_bar,         label: 'HighBar' },
        { glyph: GLYPHS.combo_gate,        color: COLORS.kind.combo_gate,       label: 'ComboGate' },
        { glyph: GLYPHS.split_path,        color: COLORS.kind.split_path,       label: 'SplitPath' },
    ];

    const lineH = 16;
    const pad = 8;
    const legendH = items.length * lineH + pad * 2;
    const legendW = 132;
    const x = w - legendW - 10;
    const y = h - legendH - 10;

    // Background
    ctx.save();
    ctx.globalAlpha = 0.85;
    ctx.fillStyle = COLORS.panelBg;
    ctx.fillRect(x, y, legendW, legendH);
    ctx.globalAlpha = 1;
    ctx.strokeStyle = COLORS.panelBorder;
    ctx.lineWidth = 1;
    ctx.strokeRect(x, y, legendW, legendH);

    ctx.font = '12px sans-serif';
    ctx.textBaseline = 'middle';

    for (let i = 0; i < items.length; i++) {
        const item = items[i];
        const iy = y + pad + i * lineH + lineH / 2;

        // Glyph
        ctx.textAlign = 'center';
        ctx.fillStyle = item.color;
        ctx.fillText(item.glyph, x + 16, iy);

        // Label
        ctx.textAlign = 'left';
        ctx.fillStyle = COLORS.text;
        ctx.fillText(item.label, x + 30, iy);
    }

    ctx.restore();
}
