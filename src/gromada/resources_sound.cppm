//
// Created by allcr on 25.03.2026.
//
export module Gromada.Resources.Sound;

import std;
import Gromada.ResourceReader;
import utils;

// Actually, it's a usual wav
export struct SoundData {
    std::uint16_t numChannels;
    std::uint32_t sampleRate;
    std::variant<std::vector<float>, std::vector<std::uint8_t>, std::vector<std::int16_t>> waveData;

    explicit SoundData (BinaryStreamReader& reader);
};

export std::vector<SoundData> getSounds( const Section& soundSection, BinaryStreamReader& soundReader);


// Implementation
std::vector<SoundData> getSounds( const Section& soundSection, BinaryStreamReader& soundReader) {
    if(soundSection.header().type != SectionType::Sound)
        throw std::logic_error("Trying to get sounds with invalid section");

    std::vector<SoundData> result;
    result.reserve(soundSection.header().elementCount);

    for (int i = 0; i < soundSection.header().elementCount; ++i) {
        const auto _ = soundReader.read<std::uint8_t>();
        const auto offset = soundReader.read<std::uint32_t>();

        const auto soundDataStartPos = soundReader.tellg();
        result.emplace_back(soundReader);
        const auto numBytesRead = soundReader.tellg() - soundDataStartPos;
        if (numBytesRead > offset) {
            throw std::logic_error("Sound data read exceeds expected offset");
        }

        soundReader.skip(offset - numBytesRead); // Ensure we move to the next sound if there are any remaining bytes for the current sound
    }
    return result;
}

SoundData::SoundData( BinaryStreamReader& reader ) {
    constexpr std::uint32_t riffMagic = 0x46464952; // "RIFF" in little-endian
    constexpr std::uint32_t waveMagic = 0x45564157; // "WAVE" in little-endian
    constexpr std::uint32_t fmtMagic  = 0x20746D66; // "fmt " in little-endian
    constexpr std::uint32_t dataMagic = 0x61746164; // "data" in little-endian

    // WAV header parsing
    std::uint32_t riff_val = reader.read<std::uint32_t>();
    if (riff_val != riffMagic) throw std::runtime_error("Not a RIFF file");
    reader.skip(4); // skip chunk size
    std::uint32_t wave_val = reader.read<std::uint32_t>();
    if (wave_val != waveMagic) throw std::runtime_error("Not a WAVE file");

    // --- Parse fmt chunk ---
    bool fmt_found = false;
    std::uint16_t audio_format = 0;
    std::uint16_t bits_per_sample = 0;
    while (!fmt_found) {
        std::uint32_t chunk_id = reader.read<std::uint32_t>();
        std::uint32_t chunk_size = reader.read<std::uint32_t>();
        if (chunk_id == fmtMagic) {
            audio_format = reader.read<std::uint16_t>();
            numChannels = reader.read<std::uint16_t>();
            sampleRate = reader.read<std::uint32_t>();
            [[maybe_unused]]auto byteRate = reader.read<std::uint32_t>();
            [[maybe_unused]] auto blockAlign = reader.read<std::uint16_t>();

            bits_per_sample = reader.read<std::uint16_t>();
            if (chunk_size > 16) reader.skip(chunk_size - 16);
            fmt_found = true;
        } else {
            reader.skip(chunk_size);
        }
    }

    // --- Find data chunk ---
    while (reader.bytesRemaining() > 0) {
        std::uint32_t chunk_id = reader.read<std::uint32_t>();
        std::uint32_t chunk_size = reader.read<std::uint32_t>();

        auto readData = [&]<typename T>(T _) {
            auto& buffer = waveData.emplace<std::vector<T>>( chunk_size / sizeof(T) );
            reader.read_to( std::as_writable_bytes(std::span{buffer}) );
        };

        if (chunk_id == dataMagic) {
            if (audio_format == 3 && bits_per_sample == 32) {
                readData(std::float_t{});
            } else if (audio_format == 1 && bits_per_sample == 8) {
                readData(std::uint8_t{});
            } else if (audio_format == 1 && bits_per_sample == 16) {
                readData(std::int16_t{});
            } else {
                throw std::runtime_error("Unsupported audio format in WAV file");
            }

            return;
        } else {
            reader.skip(chunk_size);
        }
    }
}
