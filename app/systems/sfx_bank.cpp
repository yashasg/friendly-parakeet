#include "sfx_bank.h"
#include "sfx_bank_resources.h"
#include "../components/audio.h"

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

float next_noise(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    const auto sample = static_cast<int>((state >> 16u) & 0xffffu) - 32768;
    return static_cast<float>(sample) / 32768.0f;
}

float sample_sine(const SfxSpec& spec, int frame, std::uint32_t& /*noise_state*/) {
    const float t = static_cast<float>(frame) / static_cast<float>(SAMPLE_RATE);
    const float phase = 2.0f * kPi * spec.frequency * t;
    return std::sin(phase);
}

float sample_noise(const SfxSpec& /*spec*/, int /*frame*/, std::uint32_t& noise_state) {
    return next_noise(noise_state);
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

void sfx_bank_unload(entt::registry& reg) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank) return;

    bank->release();
}

void SFXBank::release() {
    for (int idx = 0; idx < SFX_COUNT; ++idx) {
        if (sound_loaded[idx] && IsSoundValid(sounds[idx])) {
            UnloadSound(sounds[idx]);
        }
        sounds[idx] = {};
        sound_loaded[idx] = false;
    }
    loaded = false;
}

SFXBank::~SFXBank() { release(); }

SFXBank::SFXBank(SFXBank&& other) noexcept
    : loaded{other.loaded}
{
    for (int idx = 0; idx < SFX_COUNT; ++idx) {
        sounds[idx] = other.sounds[idx];
        sound_loaded[idx] = other.sound_loaded[idx];
        other.sounds[idx] = {};
        other.sound_loaded[idx] = false;
    }
    other.loaded = false;
}

SFXBank& SFXBank::operator=(SFXBank&& other) noexcept {
    if (this != &other) {
        release();
        for (int idx = 0; idx < SFX_COUNT; ++idx) {
            sounds[idx] = other.sounds[idx];
            sound_loaded[idx] = other.sound_loaded[idx];
            other.sounds[idx] = {};
            other.sound_loaded[idx] = false;
        }
        loaded = other.loaded;
        other.loaded = false;
    }
    return *this;
}
