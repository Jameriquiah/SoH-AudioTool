#include "SohSampleWriter.h"

#include <fstream>

static void WriteU8(std::ostream& out, uint8_t value) {
    out.put(static_cast<char>(value));
}

static void WriteU16LE(std::ostream& out, uint16_t value) {
    uint8_t bytes[2] = {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static void WriteU32LE(std::ostream& out, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 24) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static void WriteU64LE(std::ostream& out, uint64_t value) {
    uint8_t bytes[8] = {
        static_cast<uint8_t>(value & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 32) & 0xFF),
        static_cast<uint8_t>((value >> 40) & 0xFF),
        static_cast<uint8_t>((value >> 48) & 0xFF),
        static_cast<uint8_t>((value >> 56) & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static void WriteHeader(std::ostream& out) {
    constexpr uint32_t kResTypeAudioSample = 0x4F534D50; // OSMP
    constexpr uint32_t kResVersion = 2;
    constexpr uint64_t kResId = 0xDEADBEEFDEADBEEFULL;

    WriteU8(out, 0);
    WriteU8(out, 0);
    WriteU8(out, 0);
    WriteU8(out, 0);

    WriteU32LE(out, kResTypeAudioSample);
    WriteU32LE(out, kResVersion);
    WriteU64LE(out, kResId);
    WriteU32LE(out, 0);
    WriteU64LE(out, 0);
    WriteU32LE(out, 0);

    while (out.tellp() < 0x40) {
        WriteU32LE(out, 0);
    }
}

bool WriteSohSample(const std::filesystem::path& path, const SohSampleData& sample, std::string& error) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "Failed to open output file.";
        return false;
    }

    WriteHeader(out);

    WriteU8(out, 0); // CODEC_ADPCM
    WriteU8(out, 0); // medium
    WriteU8(out, 0); // unk_bit26
    WriteU8(out, 0); // isRelocated

    WriteU32LE(out, static_cast<uint32_t>(sample.adpcmData.size()));
    if (!sample.adpcmData.empty()) {
        out.write(reinterpret_cast<const char*>(sample.adpcmData.data()), sample.adpcmData.size());
    }

    if (sample.loopEnabled) {
        WriteU32LE(out, sample.loopStart);
        WriteU32LE(out, sample.loopEnd);
        WriteU32LE(out, static_cast<uint32_t>(sample.loopCount));
        WriteU32LE(out, 16);
        for (int16_t value : sample.loopState) {
            WriteU16LE(out, static_cast<uint16_t>(value));
        }
    } else {
        WriteU32LE(out, 0);
        WriteU32LE(out, sample.sampleCount);
        WriteU32LE(out, 0);
        WriteU32LE(out, 0);
    }

    WriteU32LE(out, static_cast<uint32_t>(sample.order));
    WriteU32LE(out, static_cast<uint32_t>(sample.predictors));
    WriteU32LE(out, static_cast<uint32_t>(sample.book.size()));
    for (int16_t value : sample.book) {
        WriteU16LE(out, static_cast<uint16_t>(value));
    }

    if (!out) {
        error = "Failed to write output file.";
        return false;
    }

    return true;
}
