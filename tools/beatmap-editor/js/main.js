// main.js — Integration bootstrap for the SHAPESHIFTER beatmap editor.
// Imports all modules, initializes them, wires transport controls,
// and runs the requestAnimationFrame render loop.

import { state, getActiveBeats, on } from './state.js';
import * as audio from './audio.js';
import * as timeline from './timeline.js';
import * as editor from './editor.js';
import * as contextMenu from './context-menu.js';
import * as panels from './panels.js';
import { validate } from './io.js';
import { loadSharedConstants, SHAPE_GLYPHS, KIND_LABELS, KINDS_WITH_SHAPE } from './constants.js';

// ── Load shared constants, then boot ────────────────────────────────
loadSharedConstants().then(boot).catch(() => boot());

function boot() {

// ── DOM Elements ────────────────────────────────────────────────────

const canvas       = document.getElementById('timeline-canvas');
const ctxMenuEl    = document.getElementById('context-menu');
const btnPlay      = document.getElementById('btn-play');
const btnStop      = document.getElementById('btn-stop');
const timeDisplay  = document.getElementById('time-display');
const beatDisplay  = document.getElementById('beat-display');
const playbackRate = document.getElementById('playback-rate');
const chkMetronome = document.getElementById('chk-metronome');
const hScrollWrap  = document.getElementById('h-scroll-wrapper');
const hScrollInner = document.getElementById('h-scroll-content');

// ── Module Initialization ───────────────────────────────────────────

try { timeline.init(canvas); }
catch (e) { console.error('[main] timeline.init failed:', e); }

try { contextMenu.init(ctxMenuEl); }
catch (e) { console.error('[main] contextMenu.init failed:', e); }

try { editor.init(canvas, contextMenu, audio); }
catch (e) { console.error('[main] editor.init failed:', e); }

try { panels.init(audio); }
catch (e) { console.error('[main] panels.init failed:', e); }

// ── Modals ──────────────────────────────────────────────────────────

const settingsModal = document.getElementById('settings-modal');
const helpModal = document.getElementById('help-modal');

bindModal({
    trigger: document.getElementById('btn-settings'),
    modal: settingsModal,
    close: document.getElementById('btn-settings-close'),
});

bindModal({
    trigger: document.getElementById('btn-help'),
    modal: helpModal,
    close: document.getElementById('btn-help-close'),
});

window.addEventListener('keydown', (e) => {
    if (e.key !== 'Escape') return;
    if (helpModal?.classList.contains('visible')) {
        hideModal(helpModal);
    } else if (settingsModal?.classList.contains('visible')) {
        hideModal(settingsModal);
    }
});

// Open help with '?' (Shift+/) when not typing in form fields.
window.addEventListener('keydown', (e) => {
    if (e.key !== '?') return;
    const tag = e.target?.tagName;
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
    if (helpModal?.classList.contains('visible')) return;
    e.preventDefault();
    showModal(helpModal);
    document.getElementById('btn-help-close')?.focus();
});

// ── Tool Display ────────────────────────────────────────────────────

const toolDisplay = document.getElementById('tool-display');

function updateToolDisplay() {
    const { kind, shape } = state.tool;
    const label = KIND_LABELS[kind] || kind;
    if (KINDS_WITH_SHAPE.includes(kind)) {
        const glyph = SHAPE_GLYPHS[shape] || '●';
        toolDisplay.textContent = `${glyph} ${label}`;
    } else {
        toolDisplay.textContent = label;
    }
}

on('tool-changed', updateToolDisplay);
updateToolDisplay();

// ── Horizontal Scrollbar ────────────────────────────────────────────

let scrollbarSuppressed = false;

function updateScrollbarSize() {
    const beatPeriod = 60 / (state.bpm || 120);
    const totalBeats = Math.ceil((state.duration || 180) / beatPeriod) + 4;
    const totalWidth = totalBeats * state.zoom + 100;
    hScrollInner.style.width = totalWidth + 'px';
}

hScrollWrap.addEventListener('scroll', () => {
    if (scrollbarSuppressed) return;
    state.scrollX = hScrollWrap.scrollLeft;
});

function syncScrollbar() {
    scrollbarSuppressed = true;
    hScrollWrap.scrollLeft = state.scrollX;
    scrollbarSuppressed = false;
}

on('metadata-changed', updateScrollbarSize);
on('beats-changed', updateScrollbarSize);
on('zoom-changed', updateScrollbarSize);
updateScrollbarSize();

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

    // Keep scrollbar in sync with state.scrollX (changed by zoom/pan/keyboard)
    syncScrollbar();

    // Waveform data (sized to logical canvas width)
    const dpr = window.devicePixelRatio || 1;
    const logicalWidth = canvas.width / dpr;
    const waveformData = state.audioBuffer
        ? audio.getWaveformData(Math.round(logicalWidth))
        : null;

    // Render
    timeline.render(state, waveformData, []);

    requestAnimationFrame(frame);
}

requestAnimationFrame(frame);

} // end boot()

function bindModal({ trigger, modal, close }) {
    if (!trigger || !modal || !close) return;

    trigger.addEventListener('click', () => showModal(modal));
    close.addEventListener('click', () => hideModal(modal));
    modal.addEventListener('click', (e) => {
        if (e.target === modal) hideModal(modal);
    });
}

function showModal(modal) {
    modal.classList.add('visible');
    modal.setAttribute('aria-hidden', 'false');
}

function hideModal(modal) {
    modal.classList.remove('visible');
    modal.setAttribute('aria-hidden', 'true');
}
