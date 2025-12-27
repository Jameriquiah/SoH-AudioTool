#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct SohSampleData {
    std::vector<uint8_t> adpcmData;
    uint32_t sampleCount = 0;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    int32_t loopCount = 0;
    bool loopEnabled = false;
    std::array<int16_t, 16> loopState{};
    int order = 0;
    int predictors = 0;
    std::vector<int16_t> book;
};

bool WriteSohSample(const std::filesystem::path& path, const SohSampleData& sample, std::string& error);
