#include "AudioFormats.h"
#include "SohSampleWriter.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

struct SampleItem {
    std::filesystem::path inputPath;
    std::string outputName;
    bool loopEnabled = false;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    int32_t loopCount = -1;
    uint32_t sampleRate = 0;
    double tuning = 0.0;
    std::string status;
};

static int ImGuiInputTextCallbackImpl(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

static bool InputTextString(const char* label, std::string& value, ImGuiInputTextFlags flags = 0) {
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputText(label, value.data(), value.capacity() + 1, flags, ImGuiInputTextCallbackImpl, &value);
}

static std::string ToUtf8(const std::wstring& input) {
#ifdef _WIN32
    if (input.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), result.data(), size, nullptr, nullptr);
    return result;
#else
    return std::string(input.begin(), input.end());
#endif
}

static std::wstring ToWide(const std::string& input) {
#ifdef _WIN32
    if (input.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), result.data(), size);
    return result;
#else
    return std::wstring(input.begin(), input.end());
#endif
}

static std::string PathToUtf8(const std::filesystem::path& path) {
    return ToUtf8(path.wstring());
}

static std::string DefaultOutputName(const std::filesystem::path& path) {
    return path.stem().string();
}

#ifdef _WIN32
static std::vector<std::filesystem::path> OpenWavDialog() {
    std::vector<std::filesystem::path> results;
    std::wstring buffer(65536, L'\0');

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"WAV Files\0*.wav\0All Files\0*.*\0";
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&ofn)) {
        return results;
    }

    std::wstring dir = buffer.c_str();
    const wchar_t* ptr = buffer.c_str() + dir.size() + 1;

    if (*ptr == L'\0') {
        results.emplace_back(dir);
        return results;
    }

    while (*ptr) {
        std::filesystem::path filePath = std::filesystem::path(dir) / ptr;
        results.emplace_back(filePath);
        ptr += wcslen(ptr) + 1;
    }

    return results;
}

static std::optional<std::filesystem::path> BrowseFolderDialog() {
    BROWSEINFOW bi{};
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        return std::nullopt;
    }

    wchar_t pathBuffer[MAX_PATH];
    if (!SHGetPathFromIDListW(pidl, pathBuffer)) {
        CoTaskMemFree(pidl);
        return std::nullopt;
    }
    CoTaskMemFree(pidl);

    return std::filesystem::path(pathBuffer);
}
#endif

static std::array<int16_t, 16> BuildLoopState(const std::vector<int16_t>& samples, uint32_t loopStart) {
    std::array<int16_t, 16> state{};
    if (samples.empty()) {
        return state;
    }

    if (loopStart >= 16) {
        for (size_t i = 0; i < 16; i++) {
            state[i] = samples[loopStart - 16 + i];
        }
    } else {
        size_t pad = 16 - loopStart;
        for (size_t i = 0; i < pad; i++) {
            state[i] = 0;
        }
        for (size_t i = 0; i < loopStart; i++) {
            state[pad + i] = samples[i];
        }
    }

    return state;
}

static bool ConvertSample(const SampleItem& item,
                          const std::filesystem::path& outputDir,
                          int predictorCount,
                          std::string& status) {
    std::string error;
    WavData wav;
    if (!ReadWavFile(item.inputPath, wav, error)) {
        status = "WAV error: " + error;
        return false;
    }

    if (item.outputName.empty()) {
        status = "Output name is empty.";
        return false;
    }

    if (outputDir.empty()) {
        status = "Output folder is empty.";
        return false;
    }

    VadpcmAifc aifc;
    if (!EncodeVadpcm(wav, predictorCount, aifc, error)) {
        status = "VADPCM encode failed: " + error;
        return false;
    }

    std::vector<int16_t> decodedSamples;
    if (!DecodeVadpcm(aifc, decodedSamples, error)) {
        status = "VADPCM decode failed: " + error;
        return false;
    }
    int maxAbs = 0;
    for (int16_t sample : decodedSamples) {
        int value = sample < 0 ? -static_cast<int>(sample) : static_cast<int>(sample);
        if (value > maxAbs) {
            maxAbs = value;
        }
    }
    if (maxAbs == 0) {
        status = "Encoded audio is silent.";
        return false;
    }

    SohSampleData outputSample;
    outputSample.adpcmData = std::move(aifc.adpcmData);
    outputSample.sampleCount = static_cast<uint32_t>(wav.samples.size());
    outputSample.order = aifc.order;
    outputSample.predictors = aifc.predictors;
    outputSample.book = std::move(aifc.book);

    if (item.loopEnabled) {
        if (decodedSamples.empty()) {
            status = "Decoded audio is empty.";
            return false;
        }
        uint32_t maxIndex = static_cast<uint32_t>(decodedSamples.size() - 1);
        uint32_t loopStart = item.loopStart;
        uint32_t loopEnd = item.loopEnd == 0 ? maxIndex : item.loopEnd;

        if (loopStart > loopEnd || loopEnd > maxIndex) {
            status = "Invalid loop range.";
            return false;
        }

        outputSample.loopEnabled = true;
        outputSample.loopStart = loopStart;
        outputSample.loopEnd = loopEnd;
        outputSample.loopCount = item.loopCount;
        outputSample.loopState = BuildLoopState(decodedSamples, loopStart);
    }

    std::filesystem::create_directories(outputDir);
    std::filesystem::path outPath = outputDir / item.outputName;
    if (!WriteSohSample(outPath, outputSample, error)) {
        status = "Write error: " + error;
        return false;
    }

    status = "OK";
    return true;
}

int main(int, char**) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    float mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("SoH Audio Tool", static_cast<int>(1200 * mainScale), static_cast<int>(720 * mainScale), windowFlags);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(mainScale);
    style.FontScaleDpi = mainScale;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    std::filesystem::path outputDir = std::filesystem::current_path();
    constexpr int predictorCount = 4;
    std::vector<SampleItem> items;
    std::string outputDirStr = PathToUtf8(outputDir);
    outputDirStr.reserve(512);

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                done = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            }
            if (event.type == SDL_EVENT_DROP_FILE) {
                std::filesystem::path path = event.drop.data;
                if (path.extension() == ".wav" || path.extension() == ".WAV") {
                    WavData wav;
                    std::string err;
                    SampleItem item;
                    item.inputPath = path;
                    item.outputName = DefaultOutputName(path);
                    item.outputName.reserve(128);
                    if (ReadWavFile(path, wav, err)) {
                        item.sampleRate = wav.sampleRate;
                        item.tuning = static_cast<double>(wav.sampleRate) / 32000.0;
                        item.status = "Ready";
                    } else {
                        item.status = "WAV error: " + err;
                    }
                    items.push_back(std::move(item));
                }
                SDL_free(const_cast<char*>(event.drop.data));
            }
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("SoH Audio Tool");

        ImGui::Text("vadpcm tool:");
        ImGui::SameLine();
        ImGui::TextDisabled("built-in encoder");

        ImGui::Text("Output folder:");
        ImGui::PushItemWidth(-120.0f);
        InputTextString("##output", outputDirStr);
        ImGui::PopItemWidth();
        outputDir = std::filesystem::path(ToWide(outputDirStr));
        ImGui::SameLine();
        if (ImGui::Button("Browse##output")) {
#ifdef _WIN32
            auto folder = BrowseFolderDialog();
            if (folder) {
                outputDir = *folder;
                outputDirStr = PathToUtf8(outputDir);
            }
#endif
        }

        ImGui::TextDisabled("Loop End = 0 uses last sample. Count = -1 means infinite.");

        if (ImGui::Button("Add WAVs")) {
#ifdef _WIN32
            auto files = OpenWavDialog();
            for (const auto& path : files) {
                WavData wav;
                std::string err;
                SampleItem item;
                item.inputPath = path;
                item.outputName = DefaultOutputName(path);
                item.outputName.reserve(128);
                if (ReadWavFile(path, wav, err)) {
                    item.sampleRate = wav.sampleRate;
                    item.tuning = static_cast<double>(wav.sampleRate) / 32000.0;
                    item.status = "Ready";
                } else {
                    item.status = "WAV error: " + err;
                }
                items.push_back(std::move(item));
            }
#endif
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear List")) {
            items.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Convert")) {
            for (auto& item : items) {
                std::string status;
                if (ConvertSample(item, outputDir, predictorCount, status)) {
                    item.status = status;
                } else {
                    item.status = status;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable("samples", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Input");
            ImGui::TableSetupColumn("Output Name");
            ImGui::TableSetupColumn("Loop");
            ImGui::TableSetupColumn("Start");
            ImGui::TableSetupColumn("End");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Rate");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < items.size(); i++) {
                auto& item = items[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(PathToUtf8(item.inputPath).c_str());

                ImGui::TableSetColumnIndex(1);
                InputTextString(("##out" + std::to_string(i)).c_str(), item.outputName);

                ImGui::TableSetColumnIndex(2);
                ImGui::Checkbox(("##loop" + std::to_string(i)).c_str(), &item.loopEnabled);

                ImGui::TableSetColumnIndex(3);
                ImGui::InputScalar(("##start" + std::to_string(i)).c_str(), ImGuiDataType_U32, &item.loopStart);

                ImGui::TableSetColumnIndex(4);
                ImGui::InputScalar(("##end" + std::to_string(i)).c_str(), ImGuiDataType_U32, &item.loopEnd);

                ImGui::TableSetColumnIndex(5);
                ImGui::InputScalar(("##count" + std::to_string(i)).c_str(), ImGuiDataType_S32, &item.loopCount);

                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%u (%.4f)", item.sampleRate, item.tuning);

                ImGui::TableSetColumnIndex(7);
                ImGui::TextUnformatted(item.status.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.12f, 0.14f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

#ifdef _WIN32
    CoUninitialize();
#endif

    return 0;
}
