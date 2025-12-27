#include "AudioFormats.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

extern "C" {
#include "codec/vadpcm.h"
}

static uint16_t ReadU16LE(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

static uint32_t ReadU32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

static uint16_t ReadU16BE(const uint8_t* data) {
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

static uint32_t ReadU32BE(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

static void WriteU16BE(std::ostream& out, uint16_t value) {
    uint8_t bytes[2] = {
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static void WriteU32BE(std::ostream& out, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static void WriteExtended80(std::ostream& out, uint32_t sampleRate) {
    uint8_t bytes[10] = {};
    if (sampleRate == 0) {
        out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        return;
    }

    int exp = static_cast<int>(std::floor(std::log2(static_cast<double>(sampleRate))));
    double frac = static_cast<double>(sampleRate) / std::pow(2.0, exp);
    uint16_t exponent = static_cast<uint16_t>(exp + 16383);
    uint64_t mantissa = static_cast<uint64_t>(std::ldexp(frac, 63) + 0.5);

    bytes[0] = static_cast<uint8_t>((exponent >> 8) & 0x7F);
    bytes[1] = static_cast<uint8_t>(exponent & 0xFF);

    for (int i = 0; i < 8; i++) {
        bytes[2 + i] = static_cast<uint8_t>((mantissa >> (56 - i * 8)) & 0xFF);
    }

    out.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

static double ReadExtended80(const uint8_t* data) {
    uint16_t exponent = static_cast<uint16_t>(((data[0] & 0x7F) << 8) | data[1]);
    uint64_t mantissa = 0;
    for (int i = 0; i < 8; i++) {
        mantissa = (mantissa << 8) | data[2 + i];
    }

    if (exponent == 0 && mantissa == 0) {
        return 0.0;
    }

    int exp = static_cast<int>(exponent) - 16383;
    double frac = static_cast<double>(mantissa) / std::ldexp(1.0, 63);
    return std::ldexp(frac, exp);
}

static bool ReadFileBytes(const std::filesystem::path& path, std::vector<uint8_t>& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Failed to open file.";
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < 0) {
        error = "Invalid file size.";
        return false;
    }

    out.resize(static_cast<size_t>(size));
    if (!out.empty()) {
        file.read(reinterpret_cast<char*>(out.data()), out.size());
    }

    if (!file) {
        error = "Failed to read file.";
        return false;
    }
    return true;
}

bool ReadWavFile(const std::filesystem::path& path, WavData& out, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes, error)) {
        return false;
    }
    if (bytes.size() < 12) {
        error = "WAV header too small.";
        return false;
    }

    if (std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        error = "Not a RIFF/WAVE file.";
        return false;
    }

    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + offset;
        uint32_t chunkSize = ReadU32LE(chunk + 4);
        if (offset + 8 + chunkSize > bytes.size()) {
            error = "Invalid chunk size.";
            return false;
        }

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                error = "Invalid fmt chunk.";
                return false;
            }
            audioFormat = ReadU16LE(chunk + 8);
            numChannels = ReadU16LE(chunk + 10);
            sampleRate = ReadU32LE(chunk + 12);
            bitsPerSample = ReadU16LE(chunk + 22);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataOffset = static_cast<uint32_t>(offset + 8);
            dataSize = chunkSize;
        }

        offset += 8 + chunkSize;
        if (chunkSize & 1) {
            offset += 1;
        }
    }

    if (audioFormat != 1) {
        error = "WAV must be PCM format.";
        return false;
    }
    if (numChannels != 1) {
        error = "WAV must be mono.";
        return false;
    }
    if (bitsPerSample != 16) {
        error = "WAV must be 16-bit PCM.";
        return false;
    }
    if (dataOffset == 0 || dataSize == 0) {
        error = "Missing data chunk.";
        return false;
    }
    if (dataOffset + dataSize > bytes.size()) {
        error = "Invalid data range.";
        return false;
    }
    if (dataSize % 2 != 0) {
        error = "Data size is not 16-bit aligned.";
        return false;
    }

    out.sampleRate = sampleRate;
    out.samples.resize(dataSize / 2);
    for (size_t i = 0; i < out.samples.size(); i++) {
        const uint8_t* samplePtr = bytes.data() + dataOffset + i * 2;
        out.samples[i] = static_cast<int16_t>(ReadU16LE(samplePtr));
    }

    return true;
}

bool WriteAiffPcm(const std::filesystem::path& path, const WavData& wav, std::string& error) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "Failed to open output file.";
        return false;
    }

    uint32_t numFrames = static_cast<uint32_t>(wav.samples.size());
    uint32_t dataBytes = numFrames * 2;
    uint32_t commChunkSize = 18;
    uint32_t ssndChunkSize = 8 + dataBytes;

    uint32_t formSize = 4 + (8 + commChunkSize) + (8 + ssndChunkSize);

    out.write("FORM", 4);
    WriteU32BE(out, formSize);
    out.write("AIFF", 4);

    out.write("COMM", 4);
    WriteU32BE(out, commChunkSize);
    WriteU16BE(out, 1);
    WriteU32BE(out, numFrames);
    WriteU16BE(out, 16);
    WriteExtended80(out, wav.sampleRate);

    out.write("SSND", 4);
    WriteU32BE(out, ssndChunkSize);
    WriteU32BE(out, 0);
    WriteU32BE(out, 0);

    for (int16_t sample : wav.samples) {
        uint16_t be = static_cast<uint16_t>(sample);
        WriteU16BE(out, be);
    }

    if (!out) {
        error = "Failed to write AIFF data.";
        return false;
    }

    return true;
}

bool ReadAiffPcm(const std::filesystem::path& path, AiffPcm& out, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes, error)) {
        return false;
    }
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "FORM", 4) != 0) {
        error = "Not an AIFF file.";
        return false;
    }
    if (std::memcmp(bytes.data() + 8, "AIFF", 4) != 0) {
        error = "Unsupported AIFF type.";
        return false;
    }

    uint16_t numChannels = 0;
    uint16_t sampleSize = 0;
    uint32_t sampleRate = 0;
    std::vector<uint8_t> soundData;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + offset;
        uint32_t chunkSize = ReadU32BE(chunk + 4);
        if (offset + 8 + chunkSize > bytes.size()) {
            error = "Invalid chunk size.";
            return false;
        }

        const uint8_t* chunkData = chunk + 8;
        if (std::memcmp(chunk, "COMM", 4) == 0) {
            if (chunkSize < 18) {
                error = "Invalid COMM chunk.";
                return false;
            }
            numChannels = ReadU16BE(chunkData);
            sampleSize = ReadU16BE(chunkData + 6);
            double rate = ReadExtended80(chunkData + 8);
            sampleRate = static_cast<uint32_t>(rate + 0.5);
        } else if (std::memcmp(chunk, "SSND", 4) == 0) {
            if (chunkSize < 8) {
                error = "Invalid SSND chunk.";
                return false;
            }
            uint32_t dataOffset = ReadU32BE(chunkData);
            uint32_t dataSize = chunkSize - 8;
            if (dataOffset > dataSize) {
                error = "Invalid SSND offset.";
                return false;
            }
            soundData.assign(chunkData + 8 + dataOffset, chunkData + 8 + dataSize);
        }

        offset += 8 + chunkSize;
        if (chunkSize & 1) {
            offset += 1;
        }
    }

    if (numChannels != 1 || sampleSize != 16) {
        error = "AIFF must be mono 16-bit PCM.";
        return false;
    }
    if (soundData.size() % 2 != 0) {
        error = "AIFF data is not 16-bit aligned.";
        return false;
    }

    out.sampleRate = sampleRate;
    out.samples.resize(soundData.size() / 2);
    for (size_t i = 0; i < out.samples.size(); i++) {
        out.samples[i] = static_cast<int16_t>(ReadU16BE(soundData.data() + i * 2));
    }

    return true;
}

bool ReadAifcVadpcm(const std::filesystem::path& path, VadpcmAifc& out, std::string& error) {
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes, error)) {
        return false;
    }
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "FORM", 4) != 0) {
        error = "Not an AIFC file.";
        return false;
    }
    if (std::memcmp(bytes.data() + 8, "AIFC", 4) != 0) {
        error = "Unsupported AIFC type.";
        return false;
    }

    uint16_t numChannels = 0;
    uint16_t sampleSize = 0;
    uint32_t sampleRate = 0;
    std::vector<uint8_t> soundData;
    int order = 0;
    int predictors = 0;
    std::vector<int16_t> book;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + offset;
        uint32_t chunkSize = ReadU32BE(chunk + 4);
        if (offset + 8 + chunkSize > bytes.size()) {
            error = "Invalid chunk size.";
            return false;
        }

        const uint8_t* chunkData = chunk + 8;
        if (std::memcmp(chunk, "COMM", 4) == 0) {
            if (chunkSize < 23) {
                error = "Invalid COMM chunk.";
                return false;
            }
            numChannels = ReadU16BE(chunkData);
            sampleSize = ReadU16BE(chunkData + 6);
            double rate = ReadExtended80(chunkData + 8);
            sampleRate = static_cast<uint32_t>(rate + 0.5);
        } else if (std::memcmp(chunk, "SSND", 4) == 0) {
            if (chunkSize < 8) {
                error = "Invalid SSND chunk.";
                return false;
            }
            uint32_t dataOffset = ReadU32BE(chunkData);
            uint32_t dataSize = chunkSize - 8;
            if (dataOffset > dataSize) {
                error = "Invalid SSND offset.";
                return false;
            }
            soundData.assign(chunkData + 8 + dataOffset, chunkData + 8 + dataSize);
        } else if (std::memcmp(chunk, "APPL", 4) == 0) {
            if (chunkSize < 4) {
                offset += 8 + chunkSize + (chunkSize & 1);
                continue;
            }
            const uint8_t* appl = chunkData;
            if (std::memcmp(appl, "stoc", 4) == 0 && chunkSize >= 5) {
                uint8_t nameLen = appl[4];
                size_t headerLen = (5 + nameLen + 1) & ~1u;
                if (headerLen <= chunkSize) {
                    std::string name(reinterpret_cast<const char*>(appl + 5), nameLen);
                    const uint8_t* data = appl + headerLen;
                    size_t dataLen = chunkSize - headerLen;
                    if (name == "VADPCMCODES") {
                        if (dataLen >= 6) {
                            order = static_cast<int>(ReadU16BE(data + 2));
                            predictors = static_cast<int>(ReadU16BE(data + 4));
                            size_t tableCount = static_cast<size_t>(order) * predictors * 8;
                            if (dataLen >= 6 + tableCount * 2) {
                                book.resize(tableCount);
                                for (size_t i = 0; i < tableCount; i++) {
                                    book[i] = static_cast<int16_t>(ReadU16BE(data + 6 + i * 2));
                                }
                            }
                        }
                    }
                }
            }
        }

        offset += 8 + chunkSize;
        if (chunkSize & 1) {
            offset += 1;
        }
    }

    if (numChannels != 1 || sampleSize != 16) {
        error = "AIFC must be mono 16-bit.";
        return false;
    }
    if (soundData.empty()) {
        error = "Missing SSND chunk.";
        return false;
    }
    if (order <= 0 || predictors <= 0 || book.empty()) {
        error = "Missing VADPCM codebook.";
        return false;
    }

    out.sampleRate = sampleRate;
    out.adpcmData = std::move(soundData);
    out.order = order;
    out.predictors = predictors;
    out.book = std::move(book);
    return true;
}

static std::string VadpcmErrorString(vadpcm_error err) {
    const char* text = vadpcm_error_name(err);
    if (text) {
        return text;
    }
    std::ostringstream oss;
    oss << "vadpcm error " << static_cast<int>(err);
    return oss.str();
}

bool EncodeVadpcm(const WavData& wav, int predictorCount, VadpcmAifc& out, std::string& error) {
    if (predictorCount < 1 || predictorCount > kVADPCMMaxPredictorCount) {
        error = "Predictor count must be between 1 and 16.";
        return false;
    }

    size_t totalSamples = wav.samples.size();
    size_t frameCount = (totalSamples + kVADPCMFrameSampleCount - 1) / kVADPCMFrameSampleCount;
    size_t paddedSamples = frameCount * kVADPCMFrameSampleCount;
    size_t encodedBytes = frameCount * kVADPCMFrameByteSize;
    size_t codebookVecs = static_cast<size_t>(predictorCount) * kVADPCMEncodeOrder;

    std::vector<vadpcm_vector> codebook(codebookVecs);
    std::vector<uint8_t> encoded(encodedBytes);

    vadpcm_params params{};
    params.predictor_count = predictorCount;

    std::vector<int16_t> input;
    const int16_t* inputPtr = nullptr;
    if (paddedSamples > 0) {
        input = wav.samples;
        input.resize(paddedSamples, 0);
        inputPtr = input.data();
    }

    vadpcm_error err = vadpcm_encode(&params,
                                     codebook.data(),
                                     frameCount,
                                     encoded.data(),
                                     inputPtr,
                                     nullptr);
    if (err != kVADPCMErrNone) {
        error = "VADPCM encode failed: " + VadpcmErrorString(err);
        return false;
    }

    size_t bookCount = codebookVecs * kVADPCMVectorSampleCount;
    std::vector<int16_t> book(bookCount);
    for (size_t i = 0; i < codebookVecs; i++) {
        for (size_t j = 0; j < kVADPCMVectorSampleCount; j++) {
            book[i * kVADPCMVectorSampleCount + j] = codebook[i].v[j];
        }
    }

    out.sampleRate = wav.sampleRate;
    out.adpcmData = std::move(encoded);
    out.order = kVADPCMEncodeOrder;
    out.predictors = predictorCount;
    out.book = std::move(book);
    return true;
}

bool DecodeVadpcm(const VadpcmAifc& vadpcm, std::vector<int16_t>& outSamples, std::string& error) {
    if (vadpcm.order <= 0 || vadpcm.predictors <= 0) {
        error = "Invalid VADPCM codebook.";
        return false;
    }
    if (vadpcm.adpcmData.size() % kVADPCMFrameByteSize != 0) {
        error = "Invalid VADPCM data size.";
        return false;
    }
    size_t expectedBook = static_cast<size_t>(vadpcm.order) *
                          static_cast<size_t>(vadpcm.predictors) *
                          kVADPCMVectorSampleCount;
    if (vadpcm.book.size() < expectedBook) {
        error = "VADPCM codebook is incomplete.";
        return false;
    }

    size_t codebookVecs = static_cast<size_t>(vadpcm.order) *
                          static_cast<size_t>(vadpcm.predictors);
    std::vector<vadpcm_vector> codebook(codebookVecs);
    for (size_t i = 0; i < codebookVecs; i++) {
        for (size_t j = 0; j < kVADPCMVectorSampleCount; j++) {
            codebook[i].v[j] = vadpcm.book[i * kVADPCMVectorSampleCount + j];
        }
    }

    size_t frameCount = vadpcm.adpcmData.size() / kVADPCMFrameByteSize;
    outSamples.resize(frameCount * kVADPCMFrameSampleCount);
    if (frameCount == 0) {
        return true;
    }

    vadpcm_vector state{};
    vadpcm_error err = vadpcm_decode(vadpcm.predictors,
                                     vadpcm.order,
                                     codebook.data(),
                                     &state,
                                     frameCount,
                                     outSamples.data(),
                                     vadpcm.adpcmData.data());
    if (err != kVADPCMErrNone) {
        error = "VADPCM decode failed: " + VadpcmErrorString(err);
        return false;
    }
    return true;
}
