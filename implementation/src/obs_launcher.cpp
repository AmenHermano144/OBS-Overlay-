#include "obs_overlay/obs_launcher.h"

#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <filesystem>
#include <thread>
#include <chrono>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace obs_overlay::launcher {
namespace {

bool iequals(const wchar_t* a, const wchar_t* b) {
    return _wcsicmp(a, b) == 0;
}

std::wstring readRegString(HKEY root, const wchar_t* subkey, const wchar_t* value, DWORD flags) {
    HKEY h{};
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE | flags, &h) != ERROR_SUCCESS)
        return {};

    wchar_t buf[MAX_PATH * 2]{};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    std::wstring out;
    if (RegQueryValueExW(h, value, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size) == ERROR_SUCCESS
        && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        DWORD chars = size / sizeof(wchar_t);
        if (chars > 0 && buf[chars - 1] == L'\0') --chars;
        out.assign(buf, chars);
    }
    RegCloseKey(h);
    return out;
}

std::wstring resolveExe(std::wstring root) {
    if (root.empty()) return {};
    if (root.back() == L'\\' || root.back() == L'/') root.pop_back();

    std::wstring exe64 = root + L"\\bin\\64bit\\obs64.exe";
    if (std::filesystem::exists(exe64)) return exe64;

    std::wstring exe32 = root + L"\\bin\\32bit\\obs32.exe";
    if (std::filesystem::exists(exe32)) return exe32;

    return {};
}

std::wstring envDir(const wchar_t* var) {
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(var, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return std::wstring(buf, n);
}

}  // namespace

bool isRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;

    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (iequals(pe.szExeFile, L"obs64.exe") || iequals(pe.szExeFile, L"obs32.exe")) {
            found = true;
            break;
        }
    }

    CloseHandle(snap);
    return found;
}

std::wstring findInstallPath() {
    for (DWORD view : { KEY_WOW64_64KEY, KEY_WOW64_32KEY }) {
        auto root = readRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio", L"", view);
        if (auto exe = resolveExe(root); !exe.empty()) return exe;
    }

    for (DWORD view : { KEY_WOW64_64KEY, KEY_WOW64_32KEY }) {
        auto loc = readRegString(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBS Studio",
            L"InstallLocation", view);
        if (auto exe = resolveExe(loc); !exe.empty()) return exe;
    }

    for (const wchar_t* var : { L"ProgramW6432", L"ProgramFiles", L"ProgramFiles(x86)" }) {
        auto pf = envDir(var);
        if (pf.empty()) continue;
        if (auto exe = resolveExe(pf + L"\\obs-studio"); !exe.empty()) return exe;
    }

    return {};
}

bool launch() {
    auto exe = findInstallPath();
    if (exe.empty()) return false;

    std::filesystem::path exePath(exe);
    std::wstring workDir = exePath.parent_path().wstring();

    const wchar_t* args = L"--minimize-to-tray --disable-shutdown-check";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOASYNC;
    sei.lpVerb       = L"open";
    sei.lpFile       = exe.c_str();
    sei.lpParameters = args;
    sei.lpDirectory  = workDir.c_str();
    sei.nShow        = SW_SHOWMINNOACTIVE;

    return ShellExecuteExW(&sei) != FALSE;
}

bool ensureRunning() {
    if (isRunning()) return true;
    if (!launch())   return false;

    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (isRunning()) return true;
    }
    return false;
}

}  // namespace obs_overlay::launcher
