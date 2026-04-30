// panels.js — DOM panel bindings for the beatmap editor.
// Two-way data binding: user edits → state mutations, state changes → DOM updates.

import {
    state, getActiveBeats, setMetadata, setActiveDifficulty,
    addDifficulty, copyDifficulty, pushUndo, undo, redo,
    canUndo, canRedo, on, emit,
} from './state.js';

import {
    importBeatmap, exportBeatmap, importAnalysis,
    validate, downloadFile,
} from './io.js';

import {
    OBSTACLE_KINDS, SHAPES, KINDS_WITH_SHAPE,
    KIND_LABELS, SHAPE_LABELS, COLORS, GLYPHS, SHAPE_GLYPHS,
    MIN_ZOOM, MAX_ZOOM, DEFAULT_ZOOM, DIFFICULTY_KEYS,
    DEFAULT_LEVELS, canAutoLoadBundledContent, getContentUrl,
} from './constants.js';

// ── DOM Element Cache ───────────────────────────────
let els = {};

// ── Tap BPM State ───────────────────────────────────
let tapTimestamps = [];
let tapResetTimer = null;
const TAP_RESET_MS = 3000;
const TAP_MIN_TAPS = 4;

// ── Module Reference ────────────────────────────────
let _audioModule = null;

// ── Helpers ─────────────────────────────────────────

function $(id) {
    return document.getElementById(id);
}

function readFileAsText(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => resolve(reader.result);
        reader.onerror = () => reject(reader.error);
        reader.readAsText(file);
    });
}

// ── Init ────────────────────────────────────────────

export function init(audioModule) {
    _audioModule = audioModule;

    // Cache DOM elements
    els = {
        // Toolbar
        defaultLevelSelect: $('default-level-select'),
        btnImport:       $('btn-import'),
        btnExport:       $('btn-export'),
        btnLoadAudio:    $('btn-load-audio'),
        btnImportAnalysis: $('btn-import-analysis'),
        btnUndo:         $('btn-undo'),
        btnRedo:         $('btn-redo'),
        btnZoomIn:       $('btn-zoom-in'),
        btnZoomOut:      $('btn-zoom-out'),
        zoomDisplay:     $('zoom-display'),
        beatCountDisplay: $('beat-count-display'),

        // Metadata form
        metaSongId:   $('meta-song-id'),
        metaTitle:    $('meta-title'),
        metaBpm:      $('meta-bpm'),
        metaOffset:   $('meta-offset'),
        metaLeadBeats: $('meta-lead-beats'),
        metaDuration: $('meta-duration'),
        metaSongPath: $('meta-song-path'),
        btnTapBpm:    $('btn-tap-bpm'),
        btnSetOffset: $('btn-set-offset'),

        // Palette
        kindPalette:  $('kind-palette'),
        shapePalette: $('shape-palette'),

        // Difficulty tabs
        difficultyTabs:   $('difficulty-tabs'),
        btnAddDifficulty: $('btn-add-difficulty'),

        // Hidden file inputs
        fileInputAudio:    $('file-input-audio'),
        fileInputJson:     $('file-input-json'),
        fileInputAnalysis: $('file-input-analysis'),

        // Validation
        validationPanel: $('validation-panel'),
        validationList:  $('validation-list'),

        // Drag & Drop
        dropOverlay: $('drop-overlay'),
    };

    bindMetadataForm();
    bindPalette();
    bindDifficultyTabs();
    bindToolbar();
    bindDefaultLevelSelector();
    bindValidation();
    bindDragAndDrop();

    // Initial UI sync
    syncMetadataToDOM();
    syncPaletteToDOM();
    syncDifficultyTabsToDOM();
    syncUndoRedoButtons();
    syncZoomDisplay();
    syncBeatCount();
    runValidation();
    loadInitialDefaultLevel();
}

// ── Metadata Form ───────────────────────────────────

const META_FIELD_MAP = {
    'meta-song-id':   { key: 'songId',    type: 'string' },
    'meta-title':     { key: 'title',     type: 'string' },
    'meta-bpm':       { key: 'bpm',       type: 'number' },
    'meta-offset':    { key: 'offset',    type: 'number' },
    'meta-lead-beats': { key: 'leadBeats', type: 'number' },
    'meta-duration':  { key: 'duration',  type: 'number' },
    'meta-song-path': { key: 'songPath',  type: 'string' },
};

function bindMetadataForm() {
    // Snapshot on focus so undo captures the state before editing
    for (const id of Object.keys(META_FIELD_MAP)) {
        const el = $(id);
        if (!el) continue;

        el.addEventListener('focus', () => {
            pushUndo('Edit metadata');
        });

        el.addEventListener('input', () => {
            const { key, type } = META_FIELD_MAP[id];
            const raw = el.value;
            const value = type === 'number' ? parseFloat(raw) || 0 : raw;
            setMetadata({ [key]: value });
        });
    }

    // Tap BPM
    els.btnTapBpm.addEventListener('click', handleTapBpm);

    // Set Offset
    els.btnSetOffset.addEventListener('click', () => {
        const currentTime = state.playheadTime || 0;
        els.metaOffset.value = currentTime.toFixed(3);
        pushUndo('Set offset');
        setMetadata({ offset: currentTime });
    });

    // Reverse binding: state → DOM
    on('metadata-changed', syncMetadataToDOM);
}

function syncMetadataToDOM() {
    els.metaSongId.value   = state.songId;
    els.metaTitle.value    = state.title;
    els.metaBpm.value      = state.bpm;
    els.metaOffset.value   = state.offset;
    els.metaLeadBeats.value = state.leadBeats;
    els.metaDuration.value = state.duration;
    els.metaSongPath.value = state.songPath;
}

function applyImportedBeatmapData(data) {
    setMetadata({
        songId:    data.songId,
        title:     data.title,
        bpm:       data.bpm,
        offset:    data.offset,
        leadBeats: data.leadBeats,
        duration:  data.duration,
        songPath:  data.songPath,
    });

    state.difficulties = data.difficulties;

    // If the active difficulty doesn't exist in imported data, fix it
    const diffKeys = Object.keys(data.difficulties);
    if (!data.difficulties[state.activeDifficulty]) {
        if (diffKeys.length > 0) {
            setActiveDifficulty(diffKeys[0]);
        } else {
            state.difficulties = { easy: { beats: [] }, medium: { beats: [] }, hard: { beats: [] } };
            setActiveDifficulty('easy');
        }
    }

    state.selectedIndices = [];
    rebuildDifficultyTabs();
    emit('beats-changed');
    emit('difficulty-changed');
    emit('selection-changed');
}

function setDefaultLevelSelection(levelId) {
    if (els.defaultLevelSelect) {
        els.defaultLevelSelect.value = levelId || '';
    }
}

function bindDefaultLevelSelector() {
    if (!els.defaultLevelSelect) return;

    for (const level of DEFAULT_LEVELS) {
        const option = document.createElement('option');
        option.value = level.id;
        option.textContent = level.label;
        els.defaultLevelSelect.appendChild(option);
    }

    els.defaultLevelSelect.addEventListener('change', () => {
        const level = DEFAULT_LEVELS.find((candidate) => candidate.id === els.defaultLevelSelect.value);
        if (level) {
            loadBundledLevel(level, { recordUndo: true });
        }
    });
}

function loadInitialDefaultLevel() {
    if (!canAutoLoadBundledContent()) return;

    const firstLevel = DEFAULT_LEVELS[0];
    if (firstLevel) {
        loadBundledLevel(firstLevel, { recordUndo: false });
    }
}

async function loadBundledLevel(level, { recordUndo }) {
    if (!level) return;

    if (els.defaultLevelSelect) {
        els.defaultLevelSelect.disabled = true;
    }

    try {
        const beatmapUrl = getContentUrl(level.beatmapPath);
        const response = await fetch(beatmapUrl);
        if (!response.ok) {
            throw new Error(`Failed to load beatmap: ${response.status} ${response.statusText}`);
        }

        const { data, errors } = importBeatmap(await response.text());
        if (!data || errors.length > 0) {
            showValidationErrors(
                (errors || []).map(msg => ({ message: msg, severity: 'error', beatIndex: -1 }))
            );
            return;
        }

        if (recordUndo) {
            pushUndo('Load bundled level');
        }
        applyImportedBeatmapData(data);
        setDefaultLevelSelection(level.id);

        if (_audioModule && typeof _audioModule.loadAudioUrl === 'function') {
            const buffer = await _audioModule.loadAudioUrl(getContentUrl(level.audioPath));
            state.audioBuffer = buffer;
        }
    } catch (e) {
        showValidationErrors([
            { message: e.message || 'Failed to load bundled level', severity: 'error', beatIndex: -1 },
        ]);
    } finally {
        if (els.defaultLevelSelect) {
            els.defaultLevelSelect.disabled = false;
        }
    }
}

function handleTapBpm() {
    const now = performance.now();

    // Reset after inactivity
    if (tapResetTimer) clearTimeout(tapResetTimer);
    tapResetTimer = setTimeout(() => {
        tapTimestamps = [];
    }, TAP_RESET_MS);

    tapTimestamps.push(now);

    if (tapTimestamps.length >= TAP_MIN_TAPS) {
        // Compute average interval from all taps
        let totalInterval = 0;
        for (let i = 1; i < tapTimestamps.length; i++) {
            totalInterval += tapTimestamps[i] - tapTimestamps[i - 1];
        }
        const avgInterval = totalInterval / (tapTimestamps.length - 1);
        const bpm = Math.round(60000 / avgInterval * 100) / 100;

        els.metaBpm.value = bpm;
        pushUndo('Tap BPM');
        setMetadata({ bpm });
    }
}

// ── Palette Buttons ─────────────────────────────────

function bindPalette() {
    els.kindPalette.addEventListener('click', (e) => {
        const btn = e.target.closest('.palette-btn[data-kind]');
        if (!btn) return;
        state.tool.kind = btn.dataset.kind;
        emit('tool-changed');
    });

    els.shapePalette.addEventListener('click', (e) => {
        const btn = e.target.closest('.palette-btn[data-shape]');
        if (!btn) return;
        state.tool.shape = btn.dataset.shape;
        emit('tool-changed');
    });

    on('tool-changed', syncPaletteToDOM);
}

function syncPaletteToDOM() {
    // Kind buttons
    for (const btn of els.kindPalette.querySelectorAll('.palette-btn[data-kind]')) {
        btn.classList.toggle('active', btn.dataset.kind === state.tool.kind);
    }
    // Shape buttons
    for (const btn of els.shapePalette.querySelectorAll('.palette-btn[data-shape]')) {
        btn.classList.toggle('active', btn.dataset.shape === state.tool.shape);
    }
}

// ── Difficulty Tabs ─────────────────────────────────

function bindDifficultyTabs() {
    els.difficultyTabs.addEventListener('click', (e) => {
        const tab = e.target.closest('.diff-tab[data-diff]');
        if (!tab) return;
        setActiveDifficulty(tab.dataset.diff);
    });

    els.btnAddDifficulty.addEventListener('click', () => {
        const name = window.prompt('New difficulty name:');
        if (!name || !name.trim()) return;
        const key = name.trim().toLowerCase().replace(/\s+/g, '_');
        if (!DIFFICULTY_KEYS.includes(key)) {
            showValidationErrors([
                {
                    message: `Difficulty must be one of: ${DIFFICULTY_KEYS.join(', ')}`,
                    severity: 'error',
                    beatIndex: -1,
                },
            ]);
            return;
        }
        addDifficulty(key);
        rebuildDifficultyTabs();
        setActiveDifficulty(key);
    });

    on('difficulty-changed', syncDifficultyTabsToDOM);
}

function rebuildDifficultyTabs() {
    // Remove existing diff-tab buttons (keep the "+" button)
    for (const tab of [...els.difficultyTabs.querySelectorAll('.diff-tab')]) {
        tab.remove();
    }

    // Create new tabs for each difficulty
    const diffNames = Object.keys(state.difficulties);
    for (const name of diffNames) {
        const btn = document.createElement('button');
        btn.className = 'diff-tab';
        btn.dataset.diff = name;
        btn.textContent = name.charAt(0).toUpperCase() + name.slice(1);
        els.difficultyTabs.insertBefore(btn, els.btnAddDifficulty);
    }
}

function syncDifficultyTabsToDOM() {
    // Ensure tabs match state.difficulties
    const existingTabs = els.difficultyTabs.querySelectorAll('.diff-tab[data-diff]');
    const stateKeys = Object.keys(state.difficulties);

    // Rebuild if tab count doesn't match difficulty count
    const tabKeys = [...existingTabs].map(t => t.dataset.diff);
    if (tabKeys.length !== stateKeys.length ||
        !stateKeys.every((k, i) => k === tabKeys[i])) {
        rebuildDifficultyTabs();
    }

    // Update active state
    for (const tab of els.difficultyTabs.querySelectorAll('.diff-tab[data-diff]')) {
        tab.classList.toggle('active', tab.dataset.diff === state.activeDifficulty);
    }
}

// ── Toolbar Buttons ─────────────────────────────────

function bindToolbar() {
    // Import beatmap
    els.btnImport.addEventListener('click', () => els.fileInputJson.click());
    els.fileInputJson.addEventListener('change', handleImportBeatmap);

    // Export beatmap
    els.btnExport.addEventListener('click', handleExportBeatmap);
    on('export-requested', handleExportBeatmap);

    // Load audio
    els.btnLoadAudio.addEventListener('click', () => els.fileInputAudio.click());
    els.fileInputAudio.addEventListener('change', handleLoadAudio);

    // Import analysis
    els.btnImportAnalysis.addEventListener('click', () => els.fileInputAnalysis.click());
    els.fileInputAnalysis.addEventListener('change', handleImportAnalysis);

    // Undo / Redo
    els.btnUndo.addEventListener('click', () => undo());
    els.btnRedo.addEventListener('click', () => redo());
    on('beats-changed', syncUndoRedoButtons);
    on('metadata-changed', syncUndoRedoButtons);
    on('difficulty-changed', syncUndoRedoButtons);

    // Zoom
    els.btnZoomIn.addEventListener('click', () => {
        state.zoom = Math.min(state.zoom * 1.2, MAX_ZOOM);
        emit('zoom-changed');
    });
    els.btnZoomOut.addEventListener('click', () => {
        state.zoom = Math.max(state.zoom / 1.2, MIN_ZOOM);
        emit('zoom-changed');
    });
    on('zoom-changed', syncZoomDisplay);

    // Beat count
    on('beats-changed', syncBeatCount);
    on('difficulty-changed', syncBeatCount);
}

async function handleImportBeatmap() {
    const file = els.fileInputJson.files[0];
    if (!file) return;

    try {
        const text = await readFileAsText(file);
        const { data, errors } = importBeatmap(text);

        if (!data || errors.length > 0) {
            showValidationErrors(
                (errors || []).map(msg => ({ message: msg, severity: 'error', beatIndex: -1 }))
            );
            return;
        }

        pushUndo('Import beatmap');
        applyImportedBeatmapData(data);
        setDefaultLevelSelection('');
    } catch (e) {
        showValidationErrors([
            { message: 'Failed to read file: ' + e.message, severity: 'error', beatIndex: -1 },
        ]);
    }

    // Reset file input so the same file can be re-imported
    els.fileInputJson.value = '';
}

function handleExportBeatmap() {
    try {
        const json = exportBeatmap(state);
        const filename = (state.songId || 'untitled') + '_beatmap.json';
        downloadFile(filename, json);
    } catch (e) {
        const details = String(e?.message || 'Unknown export failure')
            .split('\n')
            .map((msg) => msg.trim())
            .filter(Boolean);
        showValidationErrors(details.map((message) => ({
            message,
            severity: 'error',
            beatIndex: -1,
        })));
    }
}

async function handleLoadAudio() {
    const file = els.fileInputAudio.files[0];
    if (!file) return;

    try {
        if (_audioModule && typeof _audioModule.loadAudioFile === 'function') {
            const buffer = await _audioModule.loadAudioFile(file);
            state.audioBuffer = buffer;
            // Update duration if the audio provides a different one
            if (buffer && buffer.duration && buffer.duration !== state.duration) {
                pushUndo('Load audio');
                setMetadata({ duration: Math.round(buffer.duration * 100) / 100 });
            }
        }
    } catch (e) {
        console.error('Failed to load audio:', e);
    }

    els.fileInputAudio.value = '';
}

async function handleImportAnalysis() {
    const file = els.fileInputAnalysis.files[0];
    if (!file) return;

    try {
        const text = await readFileAsText(file);
        const result = importAnalysis(text);
        if (result) {
            state.analysisData = result;
            emit('analysis-loaded');
        }
    } catch (e) {
        console.error('Failed to import analysis:', e);
    }

    els.fileInputAnalysis.value = '';
}

function syncUndoRedoButtons() {
    els.btnUndo.disabled = !canUndo();
    els.btnRedo.disabled = !canRedo();
}

function syncZoomDisplay() {
    const pct = Math.round(state.zoom / DEFAULT_ZOOM * 100);
    els.zoomDisplay.textContent = pct + '%';
}

function syncBeatCount() {
    const count = getActiveBeats().length;
    els.beatCountDisplay.textContent = count + ' beat' + (count !== 1 ? 's' : '');
}

// ── Validation Panel ────────────────────────────────

function bindValidation() {
    on('beats-changed', runValidation);
    on('metadata-changed', runValidation);
}

function runValidation() {
    const errors = validate(state);
    showValidationErrors(errors);
}

function showValidationErrors(errors) {
    const list = els.validationList;
    list.textContent = '';

    for (const err of errors) {
        const icon = err.severity === 'error' ? '✗' : '⚠';
        const div = document.createElement('div');
        div.className = 'validation-item ' + err.severity;
        const iconSpan = document.createElement('span');
        iconSpan.className = 'validation-icon';
        iconSpan.textContent = icon;
        div.appendChild(iconSpan);
        div.appendChild(document.createTextNode(' ' + err.message));
        list.appendChild(div);
    }

    els.validationPanel.classList.toggle('has-errors', errors.length > 0);
}

// ── Drag and Drop ───────────────────────────────────

function bindDragAndDrop() {
    let dragCounter = 0;

    document.addEventListener('dragenter', (e) => {
        e.preventDefault();
        dragCounter++;
        if (els.dropOverlay) els.dropOverlay.classList.add('visible');
    });

    document.addEventListener('dragleave', (e) => {
        e.preventDefault();
        dragCounter--;
        if (dragCounter <= 0) {
            dragCounter = 0;
            if (els.dropOverlay) els.dropOverlay.classList.remove('visible');
        }
    });

    document.addEventListener('dragover', (e) => {
        e.preventDefault();
    });

    document.addEventListener('drop', async (e) => {
        e.preventDefault();
        dragCounter = 0;
        if (els.dropOverlay) els.dropOverlay.classList.remove('visible');

        const file = e.dataTransfer?.files?.[0];
        if (!file) return;

        if (file.name.endsWith('.json')) {
            // Treat as beatmap import
            try {
                const text = await readFileAsText(file);
                const { data, errors } = importBeatmap(text);
                if (data && errors.length === 0) {
                    pushUndo('Import beatmap (drop)');
                    applyImportedBeatmapData(data);
                    setDefaultLevelSelection('');
                } else {
                    showValidationErrors(
                        (errors || []).map(msg => ({ message: msg, severity: 'error', beatIndex: -1 }))
                    );
                }
            } catch (err) {
                console.error('Drop import failed:', err);
            }
        } else if (/\.(flac|mp3|wav|ogg)$/i.test(file.name)) {
            // Treat as audio load
            if (_audioModule && typeof _audioModule.loadAudioFile === 'function') {
                try {
                    const buffer = await _audioModule.loadAudioFile(file);
                    state.audioBuffer = buffer;
                    if (buffer && buffer.duration && buffer.duration !== state.duration) {
                        pushUndo('Load audio (drop)');
                        setMetadata({ duration: Math.round(buffer.duration * 100) / 100 });
                    }
                } catch (err) {
                    console.error('Drop audio load failed:', err);
                }
            }
        }
    });
}
