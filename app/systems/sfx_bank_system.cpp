#include "sfx_bank_system.h"

#include "../components/audio.h"
#include "../systems/audio_runtime.h"

#include <SDL.h>
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
#include <SDL_mixer.h>
#endif
#include <glm/ext/scalar_constants.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
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

constexpr int SAMPLE_RATE = 44100;

constexpr std::array<SfxSpec, SFXBank::SFX_COUNT> SFX_SPECS{{
    {660.0f, 0.080f, 0.35f, ProceduralWave::Sine, 1u},
    {180.0f, 0.220f, 0.45f, ProceduralWave::Noise, 3u},
    {880.0f, 0.050f, 0.25f, ProceduralWave::Sine, 4u},
    {784.0f, 0.120f, 0.35f, ProceduralWave::Sine, 5u},
    {440.0f, 0.100f, 0.30f, ProceduralWave::Sine, 6u},
    {330.0f, 0.140f, 0.35f, ProceduralWave::Square, 7u},
    {220.0f, 0.180f, 0.40f, ProceduralWave::DownSweep, 8u},
    {988.0f, 0.060f, 0.25f, ProceduralWave::Sine, 9u},
    {587.0f, 0.180f, 0.35f, ProceduralWave::Sine, 10u},
}};

static_assert(SFX_SPECS.size() == static_cast<std::size_t>(SFXBank::SFX_COUNT),
              "SFX_SPECS entry count must match SFX enum count");

constexpr bool is_valid_sfx_index(const int idx) noexcept {
    return idx >= 0 && idx < SFXBank::SFX_COUNT;
}

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

    const float phase = 2.0f * glm::pi<float>() * frequency * t;
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

void append_u16le(std::vector<std::uint8_t>& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0x00ffu));
    data.push_back(static_cast<std::uint8_t>((value >> 8u) & 0x00ffu));
}

void append_u32le(std::vector<std::uint8_t>& data, std::uint32_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0x000000ffu));
    data.push_back(static_cast<std::uint8_t>((value >> 8u) & 0x000000ffu));
    data.push_back(static_cast<std::uint8_t>((value >> 16u) & 0x000000ffu));
    data.push_back(static_cast<std::uint8_t>((value >> 24u) & 0x000000ffu));
}

Mix_Chunk* make_procedural_chunk(const SfxSpec& spec) {
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    const int frame_count = static_cast<int>(spec.duration_sec * static_cast<float>(SAMPLE_RATE));
    if (frame_count <= 0) return nullptr;

    std::vector<std::int16_t> samples(static_cast<std::size_t>(frame_count));
    std::uint32_t noise_state = spec.seed;

    for (int frame = 0; frame < frame_count; ++frame) {
        const float shaped = sample_wave(spec, frame, noise_state) * envelope_at(frame, frame_count);
        const float scaled = shaped * spec.volume * 32767.0f;
        samples[static_cast<std::size_t>(frame)] = static_cast<std::int16_t>(scaled);
    }

    constexpr std::uint16_t channels = 1u;
    constexpr std::uint16_t bits_per_sample = 16u;
    constexpr std::uint16_t block_align = channels * (bits_per_sample / 8u);
    constexpr std::uint32_t byte_rate = static_cast<std::uint32_t>(SAMPLE_RATE) * block_align;
    const std::size_t data_size_raw = samples.size() * sizeof(std::int16_t);
    if (data_size_raw > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() - 36u)) return nullptr;
    const auto data_size = static_cast<std::uint32_t>(data_size_raw);
    const std::uint32_t riff_size = 36u + data_size;

    std::vector<std::uint8_t> wav_bytes;
    wav_bytes.reserve(44u + data_size_raw);
    wav_bytes.insert(wav_bytes.end(), {'R', 'I', 'F', 'F'});
    append_u32le(wav_bytes, riff_size);
    wav_bytes.insert(wav_bytes.end(), {'W', 'A', 'V', 'E'});
    wav_bytes.insert(wav_bytes.end(), {'f', 'm', 't', ' '});
    append_u32le(wav_bytes, 16u);
    append_u16le(wav_bytes, 1u);
    append_u16le(wav_bytes, channels);
    append_u32le(wav_bytes, static_cast<std::uint32_t>(SAMPLE_RATE));
    append_u32le(wav_bytes, byte_rate);
    append_u16le(wav_bytes, block_align);
    append_u16le(wav_bytes, bits_per_sample);
    wav_bytes.insert(wav_bytes.end(), {'d', 'a', 't', 'a'});
    append_u32le(wav_bytes, data_size);

    const auto* sample_bytes = reinterpret_cast<const std::uint8_t*>(samples.data());
    wav_bytes.insert(wav_bytes.end(), sample_bytes, sample_bytes + data_size_raw);

    SDL_RWops* rw = SDL_RWFromConstMem(wav_bytes.data(), static_cast<int>(wav_bytes.size()));
    if (rw == nullptr) return nullptr;

    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);
    if (chunk == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mix_LoadWAV_RW failed for procedural SFX: %s", Mix_GetError());
    }
    return chunk;
#else
    (void)spec;
    return nullptr;
#endif
}

void play_sfx_from_bank(entt::registry& reg, SFX sfx) {
    const auto* audio_device = reg.ctx().find<AudioDeviceRuntimeState>();
    if (!audio_device || !audio_runtime::is_audio_device_ready(*audio_device)) return;

    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank || !bank->loaded) return;

    const int idx = static_cast<int>(sfx);
    if (!is_valid_sfx_index(idx)) return;
    if (!bank->sound_loaded[idx] || bank->sounds[idx] == nullptr) return;

#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
    if (Mix_PlayChannel(-1, bank->sounds[idx], 0) == -1) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mix_PlayChannel failed for SFX idx %d: %s", idx, Mix_GetError());
    }
#endif
}

}  // namespace

void sfx_bank_init(entt::registry& reg) {
    if (!reg.ctx().find<SFXPlaybackBackend>()) {
        reg.ctx().emplace<SFXPlaybackBackend>(SFXPlaybackBackend{play_sfx_from_bank});
    }

    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank) {
        bank = &reg.ctx().emplace<SFXBank>();
    }

    const auto* audio_device = reg.ctx().find<AudioDeviceRuntimeState>();
    if (bank->loaded || !audio_device || !audio_runtime::is_audio_device_ready(*audio_device)) return;

    bool any_loaded = false;
    for (int idx = 0; idx < SFXBank::SFX_COUNT; ++idx) {
        bank->sounds[idx] = make_procedural_chunk(SFX_SPECS[static_cast<std::size_t>(idx)]);
        bank->sound_loaded[idx] = bank->sounds[idx] != nullptr;
        any_loaded = any_loaded || bank->sound_loaded[idx];
    }
    bank->loaded = any_loaded;
}

void sfx_bank_unload(entt::registry& reg) {
    auto* bank = reg.ctx().find<SFXBank>();
    if (!bank) return;

    for (int idx = 0; idx < SFXBank::SFX_COUNT; ++idx) {
        if (!bank->sound_loaded[idx] || bank->sounds[idx] == nullptr) continue;
#if defined(SHAPESHIFTER_BACKEND_SDL2) && !defined(__EMSCRIPTEN__)
        Mix_FreeChunk(bank->sounds[idx]);
#endif
        bank->sounds[idx] = nullptr;
        bank->sound_loaded[idx] = false;
    }
    bank->loaded = false;
}
