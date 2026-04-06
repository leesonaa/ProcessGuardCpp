#pragma once
#include "common.h"

// ── Watchdog child process ────────────────────────────────────────
// When started with args: --watchdog <parent_pid> <exe_path>
// it monitors the parent and relaunches it if it dies.

namespace SelfGuard {

inline bool IsWatchdogMode(int argc, wchar_t** argv) {
    for (int i = 1; i < argc; i++)
        if (wcscmp(argv[i], L"--watchdog") == 0) return true;
    return false;
}

// Entry point for watchdog mode: monitor parent and restart if it dies.
// Handles the race condition where parent may crash before OpenProcess succeeds.
inline int RunWatchdog(DWORD parentPid, const std::wstring& exePath) {
    // Retry opening the parent handle for up to 5 seconds to avoid the
    // race where the parent crashes before we can open its handle.
    HANDLE hParent = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 50; i++) {
        hParent = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                              FALSE, parentPid);
        if (hParent) break;
        Sleep(100);
    }

    if (hParent && hParent != INVALID_HANDLE_VALUE) {
        // Parent is alive — wait for it to exit (crash or normal exit)
        WaitForSingleObject(hParent, INFINITE);
        CloseHandle(hParent);

        // Check if this was an intentional exit: the main process writes a
        // sentinel file before calling TerminateProcess on us during clean exit.
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        std::wstring sentinel = std::wstring(tmp) + L"ProcessGuard.exit";
        bool intentional = (GetFileAttributesW(sentinel.c_str()) != INVALID_FILE_ATTRIBUTES);
        if (intentional) {
            DeleteFileW(sentinel.c_str());
            return 0; // Clean exit — do not restart
        }
    }
    // Parent not found (already crashed) or died unexpectedly — restart it
    Sleep(1500);

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exePath + L"\"";
    CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                   0, nullptr, nullptr, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread)  CloseHandle(pi.hThread);
    return 0;
}

// Launch watchdog child (called from main process)
inline HANDLE StartWatchdog(const std::wstring& exePath) {
    DWORD myPid = GetCurrentProcessId();
    std::wstring cmd = L"\"" + exePath + L"\" --watchdog "
                     + std::to_wstring(myPid)
                     + L" \"" + exePath + L"\"";

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags      = STARTF_USESHOWWINDOW;
    si.wShowWindow  = SW_HIDE;  // hidden

    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        return pi.hProcess; // caller should CloseHandle when done
    }
    return INVALID_HANDLE_VALUE;
}

} // namespace SelfGuard
