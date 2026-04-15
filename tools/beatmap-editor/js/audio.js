// audio.js — Web Audio API module for the beatmap editor
// Owns all audio playback, waveform extraction, and metronome.
// No DOM interaction, no state mutation — polled by main.js each frame.

let audioCtx = null;
let audioBuffer = null;
let sourceNode = null;
let gainNode = null;

let playing = false;
let seekOffset = 0;
let startTime = 0;
let playbackRate = 1.0;

// Waveform cache
let waveformCache = null;
let waveformCacheWidth = 0;

// Metronome state
let metronomeEnabled = false;
let metronomeBpm = 120;
let metronomeOffset = 0;
let metronomeGain = null;
let metronomeScheduledIds = [];
let metronomeTimerId = null;
let lastScheduledBeat = -1;

function ensureContext() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    return audioCtx;
}

export async function loadAudioFile(file) {
    const ctx = ensureContext();

    const arrayBuffer = await new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => resolve(reader.result);
        reader.onerror = () => reject(reader.error);
        reader.readAsArrayBuffer(file);
    });

    audioBuffer = await ctx.decodeAudioData(arrayBuffer);

    // Invalidate waveform cache on new file load
    waveformCache = null;
    waveformCacheWidth = 0;

    // Reset playback state
    if (playing) {
        stopSource();
    }
    seekOffset = 0;
    playing = false;

    return audioBuffer;
}

function stopSource() {
    if (sourceNode) {
        try {
            sourceNode.onended = null;
            sourceNode.stop();
        } catch (_) {
            // Already stopped
        }
        sourceNode.disconnect();
        sourceNode = null;
    }
}

export function play() {
    if (!audioBuffer) return;

    const ctx = ensureContext();

    // If already playing, stop and restart
    if (playing) {
        seekOffset = getCurrentTime();
        stopSource();
    }

    // Clamp seek offset
    seekOffset = Math.max(0, Math.min(seekOffset, audioBuffer.duration));
    if (seekOffset >= audioBuffer.duration) {
        seekOffset = 0;
    }

    sourceNode = ctx.createBufferSource();
    sourceNode.buffer = audioBuffer;
    sourceNode.playbackRate.value = playbackRate;

    if (!gainNode) {
        gainNode = ctx.createGain();
        gainNode.connect(ctx.destination);
    }
    sourceNode.connect(gainNode);

    sourceNode.onended = () => {
        // Only mark as not playing if we haven't already manually stopped
        if (playing && sourceNode) {
            playing = false;
            seekOffset = audioBuffer ? audioBuffer.duration : 0;
            sourceNode = null;
            stopMetronomeScheduler();
        }
    };

    startTime = ctx.currentTime;
    sourceNode.start(0, seekOffset);
    playing = true;

    if (metronomeEnabled) {
        startMetronomeScheduler();
    }
}

export function pause() {
    if (!playing) return;

    seekOffset = getCurrentTime();
    stopSource();
    playing = false;
    stopMetronomeScheduler();
}

export function seek(timeSeconds) {
    if (!audioBuffer) {
        seekOffset = 0;
        return;
    }

    const clamped = Math.max(0, Math.min(timeSeconds, audioBuffer.duration));

    if (playing) {
        stopSource();
        seekOffset = clamped;
        // Restart from new position
        const ctx = ensureContext();
        sourceNode = ctx.createBufferSource();
        sourceNode.buffer = audioBuffer;
        sourceNode.playbackRate.value = playbackRate;

        if (!gainNode) {
            gainNode = ctx.createGain();
            gainNode.connect(ctx.destination);
        }
        sourceNode.connect(gainNode);

        sourceNode.onended = () => {
            if (playing && sourceNode) {
                playing = false;
                seekOffset = audioBuffer ? audioBuffer.duration : 0;
                sourceNode = null;
                stopMetronomeScheduler();
            }
        };

        startTime = ctx.currentTime;
        sourceNode.start(0, clamped);

        // Restart metronome scheduling from new position
        if (metronomeEnabled) {
            stopMetronomeScheduler();
            startMetronomeScheduler();
        }
    } else {
        seekOffset = clamped;
    }
}

export function getCurrentTime() {
    if (!audioBuffer) return 0;
    if (!playing) return seekOffset;

    const ctx = ensureContext();
    const elapsed = (ctx.currentTime - startTime) * playbackRate;
    const t = seekOffset + elapsed;
    return Math.min(t, audioBuffer.duration);
}

export function isPlaying() {
    return playing;
}

export function setPlaybackRate(rate) {
    playbackRate = rate;
    if (sourceNode && playing) {
        sourceNode.playbackRate.value = rate;
    }
}

export function getWaveformData(width) {
    if (!audioBuffer || width <= 0) return new Float32Array(0);

    // Return cached result if width hasn't changed
    if (waveformCache && waveformCacheWidth === width) {
        return waveformCache;
    }

    const channelData = audioBuffer.getChannelData(0);
    const totalSamples = channelData.length;
    const samplesPerBucket = totalSamples / width;
    const peaks = new Float32Array(width);

    for (let i = 0; i < width; i++) {
        const bucketStart = Math.floor(i * samplesPerBucket);
        const bucketEnd = Math.floor((i + 1) * samplesPerBucket);
        let max = 0;
        for (let j = bucketStart; j < bucketEnd && j < totalSamples; j++) {
            const abs = Math.abs(channelData[j]);
            if (abs > max) max = abs;
        }
        peaks[i] = max;
    }

    waveformCache = peaks;
    waveformCacheWidth = width;
    return peaks;
}

export function getDuration() {
    return audioBuffer ? audioBuffer.duration : 0;
}

export function setMetronome(enabled, bpm, offset) {
    metronomeEnabled = enabled;
    metronomeBpm = bpm || 120;
    metronomeOffset = offset || 0;

    if (!enabled) {
        stopMetronomeScheduler();
        return;
    }

    if (playing) {
        stopMetronomeScheduler();
        startMetronomeScheduler();
    }
}

function stopMetronomeScheduler() {
    if (metronomeTimerId !== null) {
        clearInterval(metronomeTimerId);
        metronomeTimerId = null;
    }
    lastScheduledBeat = -1;
    metronomeScheduledIds = [];
}

function startMetronomeScheduler() {
    const ctx = ensureContext();

    if (!metronomeGain) {
        metronomeGain = ctx.createGain();
        metronomeGain.gain.value = 0.3;
        metronomeGain.connect(ctx.destination);
    }

    lastScheduledBeat = -1;

    // Schedule ahead by ~100ms, check every ~50ms
    const SCHEDULE_AHEAD = 0.1;
    const CHECK_INTERVAL = 50;

    const scheduleBeats = () => {
        if (!playing || !audioBuffer) return;

        const currentAudioTime = getCurrentTime();
        const lookAheadTime = currentAudioTime + SCHEDULE_AHEAD;
        const beatInterval = 60 / metronomeBpm;

        // Find the first beat index at or after the current time
        let firstBeat = Math.ceil((currentAudioTime - metronomeOffset) / beatInterval);
        if (firstBeat < 0) firstBeat = 0;

        // Schedule beats within the look-ahead window
        for (let n = firstBeat; ; n++) {
            const beatTime = metronomeOffset + n * beatInterval;
            if (beatTime > lookAheadTime) break;
            if (beatTime > audioBuffer.duration) break;
            if (beatTime < currentAudioTime - 0.01) continue;
            if (n <= lastScheduledBeat) continue;

            // Convert audio position to AudioContext time for precise scheduling
            const ctxNow = ctx.currentTime;
            const audioNow = getCurrentTime();
            const delta = beatTime - audioNow;
            const scheduleAt = ctxNow + delta / playbackRate;

            if (scheduleAt >= ctxNow) {
                scheduleClick(ctx, scheduleAt);
            }

            lastScheduledBeat = n;
        }
    };

    scheduleBeats();
    metronomeTimerId = setInterval(scheduleBeats, CHECK_INTERVAL);
}

function scheduleClick(ctx, when) {
    const osc = ctx.createOscillator();
    osc.type = "sine";
    osc.frequency.value = 880;

    const clickGain = ctx.createGain();
    clickGain.gain.setValueAtTime(0.3, when);
    clickGain.gain.exponentialRampToValueAtTime(0.001, when + 0.05);

    osc.connect(clickGain);
    clickGain.connect(metronomeGain || ctx.destination);

    osc.start(when);
    osc.stop(when + 0.05);
}
