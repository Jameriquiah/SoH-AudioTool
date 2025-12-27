#include "Process.h"

#include <sstream>
#include <windows.h>

static std::wstring QuoteArg(const std::wstring& arg) {
    if (arg.find_first_of(L" \t\"\n\v") == std::wstring::npos) {
        return arg;
    }

    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            backslashes++;
        } else if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
        } else {
            if (backslashes) {
                quoted.append(backslashes, L'\\');
                backslashes = 0;
            }
            quoted.push_back(ch);
        }
    }
    if (backslashes) {
        quoted.append(backslashes * 2, L'\\');
    }
    quoted.push_back(L'"');
    return quoted;
}

bool RunProcess(const std::filesystem::path& exePath,
                const std::vector<std::wstring>& args,
                const std::filesystem::path& workingDir,
                std::wstring& error) {
    std::wstringstream cmd;
    cmd << QuoteArg(exePath.wstring());
    for (const auto& arg : args) {
        cmd << L" " << QuoteArg(arg);
    }

    std::wstring commandLine = cmd.str();
    std::vector<wchar_t> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(
        nullptr,
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDir.empty() ? nullptr : workingDir.wstring().c_str(),
        &si,
        &pi
    );

    if (!ok) {
        DWORD err = GetLastError();
        std::wstringstream msg;
        msg << L"CreateProcess failed (" << err << L")";
        error = msg.str();
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        exitCode = 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode != 0) {
        std::wstringstream msg;
        msg << L"Process exited with code " << exitCode;
        error = msg.str();
        return false;
    }

    return true;
}
