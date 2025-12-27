#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct WavData {
    uint32_t sampleRate = 0;
    std::vector<int16_t> samples;
};

struct AiffPcm {
    uint32_t sampleRate = 0;
    std::vector<int16_t> samples;
};

struct VadpcmAifc {
    uint32_t sampleRate = 0;
    std::vector<uint8_t> adpcmData;
    int order = 0;
    int predictors = 0;
    std::vector<int16_t> book;
};

bool ReadWavFile(const std::filesystem::path& path, WavData& out, std::string& error);
bool WriteAiffPcm(const std::filesystem::path& path, const WavData& wav, std::string& error);
bool ReadAiffPcm(const std::filesystem::path& path, AiffPcm& out, std::string& error);
bool ReadAifcVadpcm(const std::filesystem::path& path, VadpcmAifc& out, std::string& error);
bool EncodeVadpcm(const WavData& wav, int predictorCount, VadpcmAifc& out, std::string& error);
bool DecodeVadpcm(const VadpcmAifc& vadpcm, std::vector<int16_t>& outSamples, std::string& error);
