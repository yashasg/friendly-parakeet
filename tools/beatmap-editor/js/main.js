// main.js — Integration bootstrap for the SHAPESHIFTER beatmap editor.
// Imports all modules, initializes them, wires transport controls,
// and runs the requestAnimationFrame render loop.

import { state, getActiveBeats, on } from './state.js';
import * as audio from './audio.js';
import * as timeline from './timeline.js';
import * as editor from './editor.js';
import * as contextMenu from './context-menu.js';
import * as panels from './panels.js';
import { validate, exportBeatmap, downloadFile } from './io.js';

// ── DOM Elements ────────────────────────────────────────────────────

const canvas       = document.getElementById('timeline-canvas');
const ctxMenuEl    = document.getElementById('context-menu');
const btnPlay      = document.getElementById('btn-play');
const btnStop      = document.getElementById('btn-stop');
const timeDisplay  = document.getElementById('time-display');
const beatDisplay  = document.getElementById('beat-display');
const playbackRate = document.getElementById('playback-rate');
const chkMetronome = document.getElementById('chk-metronome');

// ── Module Initialization ───────────────────────────────────────────

try { timeline.init(canvas); }
catch (e) { console.error('[main] timeline.init failed:', e); }

try { contextMenu.init(ctxMenuEl); }
catch (e) { console.error('[main] contextMenu.init failed:', e); }

try { editor.init(canvas, contextMenu, audio); }
catch (e) { console.error('[main] editor.init failed:', e); }

try { panels.init(audio); }
catch (e) { console.error('[main] panels.init failed:', e); }

// ── Transport Controls ──────────────────────────────────────────────

function updatePlayButton() {
    btnPlay.textContent = audio.isPlaying() ? '⏸ Pause' : '▶ Play';
}

btnPlay.addEventListener('click', () => {
    if (audio.isPlaying()) {
        audio.pause();
    } else {
        audio.play();
    }
    updatePlayButton();
});

btnStop.addEventListener('click', () => {
    audio.pause();
    audio.seek(0);
    updatePlayButton();
});

playbackRate.addEventListener('change', () => {
    audio.setPlaybackRate(parseFloat(playbackRate.value));
});

chkMetronome.addEventListener('change', () => {
    audio.setMetronome(chkMetronome.checked, state.bpm, state.offset);
});

// Sync metronome when BPM or offset changes
on('metadata-changed', () => {
    if (chkMetronome.checked) {
        audio.setMetronome(true, state.bpm, state.offset);
    }
});

// ── Export Wiring ───────────────────────────────────────────────────
// editor.js emits 'export-requested' on Ctrl+S; panels.js handles the
// toolbar button click directly but doesn't listen on the event bus.

on('export-requested', () => {
    const json = exportBeatmap(state);
    const filename = (state.songId || 'untitled') + '_beatmap.json';
    downloadFile(filename, json);
});

// ── Validation Caching ──────────────────────────────────────────────

let cachedErrors = [];
let validationDirty = true;

on('beats-changed', () => { validationDirty = true; });
on('metadata-changed', () => { validationDirty = true; });
on('difficulty-changed', () => { validationDirty = true; });

// ── Time Formatting ─────────────────────────────────────────────────

function formatTime(seconds) {
    if (!Number.isFinite(seconds) || seconds < 0) seconds = 0;
    const m = Math.floor(seconds / 60);
    const s = seconds % 60;
    return `${m}:${s.toFixed(3).padStart(6, '0')}`;
}

function updateTimeDisplay(current, total) {
    timeDisplay.textContent = `${formatTime(current)} / ${formatTime(total)}`;
}

function updateBeatDisplay() {
    if (state.bpm <= 0) { beatDisplay.textContent = '0'; return; }
    const beat = Math.floor((state.playheadTime - state.offset) * state.bpm / 60);
    beatDisplay.textContent = Math.max(0, beat);
}

// ── Render Loop ─────────────────────────────────────────────────────

function frame() {
    // Sync playhead from audio engine
    if (audio.isPlaying()) {
        state.playheadTime = audio.getCurrentTime();
        state.playing = true;
    } else {
        state.playing = false;
    }

    // Keep play button label in sync (audio can stop on its own at end-of-track)
    updatePlayButton();

    // Display updates
    updateTimeDisplay(state.playheadTime, audio.getDuration() || state.duration);
    updateBeatDisplay();

    // Provide flattened beats reference that timeline.js expects
    state.beats = getActiveBeats();

    // Recompute validation only when state changed
    if (validationDirty) {
        cachedErrors = validate(state);
        validationDirty = false;
    }

    // Waveform data (sized to logical canvas width)
    const dpr = window.devicePixelRatio || 1;
    const logicalWidth = canvas.width / dpr;
    const waveformData = state.audioBuffer
        ? audio.getWaveformData(Math.round(logicalWidth))
        : null;

    // Render
    timeline.render(state, waveformData, cachedErrors);

    requestAnimationFrame(frame);
}

requestAnimationFrame(frame);
