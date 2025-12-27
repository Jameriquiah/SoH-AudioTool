#pragma once

#include <filesystem>
#include <string>
#include <vector>

bool RunProcess(const std::filesystem::path& exePath,
                const std::vector<std::wstring>& args,
                const std::filesystem::path& workingDir,
                std::wstring& error);
