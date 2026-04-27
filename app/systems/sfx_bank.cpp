#include "all_systems.h"
#include "../components/audio.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <vector>

namespace {

enum class ProceduralWave : std::uint8_t {
    Sine,
    Square,
    Noise,
    DownSweep
};

struct SfxSpec {
    float frequency;
    float duration_sec;
    float volume;
    ProceduralWave wave;
    std::uint32_t seed;
};

constexpr float kPi = 3.14159265358979323846f;
constexpr int SAMPLE_RATE = 44100;
constexpr int SFX_COUNT = static_cast<int>(magic_enum::enum_count<SFX>());

constexpr std::array<SfxSpec, SFX_COUNT> SFX_SPECS{{
    {660.0f, 0.080f, 0.35f, ProceduralWave::Sine, 1u},       // ShapeShift
    {180.0f, 0.220f, 0.45f, ProceduralWave::Noise, 3u},      // Crash
    {880.0f, 0.050f, 0.25f, ProceduralWave::Sine, 4u},       // UITap
    {784.0f, 0.120f, 0.35f, ProceduralWave::Sine, 5u},       // ChainBonus
    {440.0f, 0.100f, 0.30f, ProceduralWave::Sine, 6u},       // ZoneRisky
    {330.0f, 0.140f, 0.35f, ProceduralWave::Square, 7u},     // ZoneDanger
    {220.0f, 0.180f, 0.40f, ProceduralWave::DownSweep, 8u},  // ZoneDead
    {988.0f, 0.060f, 0.25f, ProceduralWave::Sine, 9u},       // ScorePopup
    {587.0f, 0.180f, 0.35f, ProceduralWave::Sine, 10u},      // GameStart
}};

static_assert(SFX_SPECS.size() == magic_enum::enum_count<SFX>(),
              "SFX_SPECS entry count must match SFX enum count");

float envelope_at(int frame, int frame_count) {
    const int attack_frames = SAMPLE_RATE / 200;
    const int release_frames = SAMPLE_RATE / 25;

    float envelope = 1.0f;
    if (frame < attack_frames) {
        envelope = static_cast<float>(frame) / static_cast<float>(attack_frames);
    }
    if (frame > frame_count - release_frames) {
        const int remaining = frame_count - frame;
        envelope = std::min(envelope,
                            static_cast<float>(remaining) / static_cast<float>(release_frames));
    }
    return envelope < 0.0f ? 0.0f : envelope;
}

float next_noise(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    const auto sample = static_cast<int>((state >> 16u) & 0xffffu) - 32768;
    return static_cast<float>(sample) / 32768.0f;
}

float sample_wave(const SfxSpec& spec, int frame, std::uint32_t& noise_state) {
    const float t = static_cast<float>(frame) / static_cast<float>(SAMPLE_RATE);
    float frequency = spec.frequency;
    if (spec.wave == ProceduralWave::DownSweep) {
        const float progress = t / spec.duration_sec;
        frequency *= 1.0f - 0.65f * progress;
    }

    const float phase = 2.0f * kPi * frequency * t;
    switch (spec.wave) {
        case ProceduralWave::Sine:
        case ProceduralWave::DownSweep:
            return std::sin(phase);
        case ProceduralWave::Square:
            return std::sin(phase) >= 0.0f ? 1.0f : -1.0f;
        case ProceduralWave::Noise:
            return next_noise(noise_state);
    }
    return 0.0f;
}

Sound make_procedural_sound(const SfxSpec& spec) {
    const int frame_count = static_cast<int>(spec.duration_sec * static_cast<float>(SAMPLE_RATE));
    std::vector<std::int16_t> samples(static_cast<std::size_t>(frame_count));
    std::uint32_t noise_state = spec.seed;

    for (int frame = 0; frame < frame_count; ++frame) {
        const float shaped = sample_wave(spec, frame, noise_state) * envelope_at(frame, frame_count);
        const float scaled = shaped * spec.volume * 32767.0f;
        samples[static_cast<std::size_t>(frame)] = static_cast<std::int16_t>(scaled);
    }

    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(frame_count);
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = samples.data();

    return LoadSoundFromWave(wave);
}

void play_sfx_from_bank(entt::registry& reg, SFX sfx) {
    if (!IsAudioDeviceReady()) return;

    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank || !bank->loaded) return;

    const int idx = static_cast<int>(sfx);
    if (idx >= SFX_COUNT) return;
    if (!bank->sound_loaded[idx] || !IsSoundValid(bank->sounds[idx])) return;

    PlaySound(bank->sounds[idx]);
}

}  // namespace

void sfx_bank_init(entt::registry& reg) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank) {
        bank = &reg.ctx().emplace<SFXBank>();
    }

    if (bank->loaded || !IsAudioDeviceReady()) return;

    bool any_loaded = false;
    for (int idx = 0; idx < SFX_COUNT; ++idx) {
        bank->sounds[idx] = make_procedural_sound(SFX_SPECS[static_cast<std::size_t>(idx)]);
        bank->sound_loaded[idx] = IsSoundValid(bank->sounds[idx]);
        any_loaded = any_loaded || bank->sound_loaded[idx];
    }
    bank->loaded = any_loaded;
}

void sfx_playback_backend_init(entt::registry& reg) {
    if (reg.ctx().find<SFXPlaybackBackend>()) return;

    reg.ctx().emplace<SFXPlaybackBackend>(SFXPlaybackBackend{play_sfx_from_bank});
}

void sfx_bank_unload(entt::registry& reg) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank) return;

    for (int idx = 0; idx < SFX_COUNT; ++idx) {
        if (bank->sound_loaded[idx] && IsSoundValid(bank->sounds[idx])) {
            UnloadSound(bank->sounds[idx]);
            bank->sound_loaded[idx] = false;
        }
    }
    bank->loaded = false;
}
