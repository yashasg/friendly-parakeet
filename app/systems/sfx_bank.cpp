#include "sfx_bank.h"
#include "../components/audio.h"
#include "../components/loaded_sfx.h"

#include <entt/entt.hpp>
#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

struct SfxSpec;

using WaveSampler = float (*)(const SfxSpec&, int, std::uint32_t&);

struct SfxSpec {
    float frequency;
    float duration_sec;
    float volume;
    WaveSampler sample;
    std::uint32_t seed;
};

constexpr float kPi = 3.14159265358979323846f;
constexpr int SAMPLE_RATE = 44100;
constexpr int SFX_COUNT = static_cast<int>(SFX::Count);

float sample_sine(const SfxSpec& spec, int frame, std::uint32_t& /*noise_state*/) {
    const float t = static_cast<float>(frame) / static_cast<float>(SAMPLE_RATE);
    const float phase = 2.0f * kPi * spec.frequency * t;
    return std::sin(phase);
}

float sample_noise(const SfxSpec& /*spec*/, int /*frame*/, std::uint32_t& noise_state) {
    noise_state = noise_state * 1664525u + 1013904223u;
    const auto sample = static_cast<int>((noise_state >> 16u) & 0xffffu) - 32768;
    return static_cast<float>(sample) / 32768.0f;
}

constexpr std::array<SfxSpec, SFX_COUNT> SFX_SPECS{{
    {660.0f, 0.080f, 0.35f, sample_sine,  1u},       // ShapeShift
    {180.0f, 0.220f, 0.45f, sample_noise, 3u},       // Crash
    {880.0f, 0.050f, 0.25f, sample_sine,  4u},       // UITap
    {784.0f, 0.120f, 0.35f, sample_sine,  5u},       // ChainBonus
    {988.0f, 0.060f, 0.25f, sample_sine,  9u},       // ScorePopup
    {587.0f, 0.180f, 0.35f, sample_sine,  10u},      // GameStart
}};

static_assert(SFX_SPECS.size() == static_cast<std::size_t>(SFX::Count),
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

Sound make_procedural_sound(const SfxSpec& spec) {
    const int frame_count = static_cast<int>(spec.duration_sec * static_cast<float>(SAMPLE_RATE));
    std::vector<std::int16_t> samples(static_cast<std::size_t>(frame_count));
    std::uint32_t noise_state = spec.seed;

    for (int frame = 0; frame < frame_count; ++frame) {
        const float shaped = spec.sample(spec, frame, noise_state) * envelope_at(frame, frame_count);
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

}  // namespace

// Load procedurally-generated `Sound` handles for every `SFX` enum value
// that successfully passes `IsSoundValid` after `LoadSoundFromWave`. Each
// successful load becomes one `LoadedSfx` row entity — sounds that fail
// validation simply never produce a row (presence ⇒ valid, the
// load-time gate replaces the former parallel `sound_loaded[]` NULL
// cursor — issue #1616 / Fabian Principle 3).
//
// Idempotent: re-entry with any existing `LoadedSfx` rows or a not-ready
// audio device is a no-op.
void sfx_bank_init(entt::registry& reg) {
    if (!IsAudioDeviceReady()) return;
    if (!reg.view<LoadedSfx>().empty()) return;

    for (int idx = 0; idx < SFX_COUNT; ++idx) {
        Sound sound = make_procedural_sound(SFX_SPECS[static_cast<std::size_t>(idx)]);
        if (!IsSoundValid(sound)) continue;
        const auto entity = reg.create();
        reg.emplace<LoadedSfx>(entity, static_cast<SFX>(idx), sound);
    }
}

// Tear down every `LoadedSfx` row. Collect-then-destroy (#1597) so we
// never mutate the storage we're walking; entity destruction fires the
// `LoadedSfx` destructor which calls `UnloadSound`.
void sfx_bank_unload(entt::registry& reg) {
    std::vector<entt::entity> doomed;
    auto view = reg.view<LoadedSfx>();
    doomed.reserve(view.size());
    for (auto entity : view) {
        doomed.push_back(entity);
    }
    for (auto entity : doomed) {
        reg.destroy(entity);
    }
}
