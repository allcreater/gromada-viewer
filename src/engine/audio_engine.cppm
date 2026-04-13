module;
#include <miniaudio.h>

export module engine.audio;

import std;
import Gromada.GameResources;

export class AudioEngine {
public:
    AudioEngine() {
        if (ma_engine_init(NULL, &m_engine) != MA_SUCCESS) {
            throw std::runtime_error("Failed to initialize miniaudio engine");
        }
    }

    ~AudioEngine() {
        for (auto& s : m_activeSounds) {
            ma_sound_uninit(&s.sound);
            ma_audio_buffer_uninit(&s.buffer);
        }
        ma_engine_uninit(&m_engine);
    }

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void playSound(const SoundData& sound) {
        cleanupFinishedSounds();

        auto [format, dataPtr, frameCount] = std::visit(
                [channels = sound.numChannels]( auto &&arg ) -> std::tuple<ma_format, const void *, ma_uint64> {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr ( std::is_same_v<T, std::vector<float> > ) {
                        return {ma_format_f32, arg.data(), arg.size() / channels};
                    } else if constexpr ( std::is_same_v<T, std::vector<std::int16_t> > ) {
                        return {ma_format_s16, arg.data(), arg.size() / channels};
                    } else if constexpr ( std::is_same_v<T, std::vector<std::uint8_t> > ) {
                        return {ma_format_u8, arg.data(), arg.size() / channels};
                    } else {
                        return {ma_format_unknown, nullptr, 0};
                    }
                }, sound.waveData);

        if (frameCount == 0 || !dataPtr) {
            return;
        }

        auto& active = m_activeSounds.emplace_back();

        ma_audio_buffer_config config = ma_audio_buffer_config_init(format, sound.numChannels, frameCount, dataPtr, NULL);
        config.sampleRate = sound.sampleRate;

        if (ma_audio_buffer_init(&config, &active.buffer) != MA_SUCCESS) {
            m_activeSounds.pop_back();
            return;
        }

        if (ma_sound_init_from_data_source(&m_engine, &active.buffer, 0, NULL, &active.sound) != MA_SUCCESS) {
            ma_audio_buffer_uninit(&active.buffer);
            m_activeSounds.pop_back();
            return;
        }

        ma_sound_start(&active.sound);
    }

private:
    void cleanupFinishedSounds() {
        m_activeSounds.remove_if([](ActiveSound& s) {
            if (ma_sound_at_end(&s.sound)) {
                ma_sound_uninit(&s.sound);
                ma_audio_buffer_uninit(&s.buffer);
                return true;
            }
            return false;
        });
    }

    struct ActiveSound {
        ma_audio_buffer buffer;
        ma_sound sound;
    };

    ma_engine m_engine;
    std::list<ActiveSound> m_activeSounds;
};
