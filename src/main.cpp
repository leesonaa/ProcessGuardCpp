#include "common.h"
#include "engine.h"
#include "selfguard.h"
#include "window.h"

// Single instance mutex name
#define MUTEX_NAME L"ProcessGuard_SingleInstance_Mutex"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    // ── Parse command line ────────────────────────────────────────
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // ── Watchdog mode: monitor parent and restart if it dies ──────
    if (SelfGuard::IsWatchdogMode(argc, argv)) {
        DWORD parentPid = 0;
        std::wstring exePath;
        for (int i = 1; i < argc; i++) {
            if (wcscmp(argv[i], L"--watchdog") == 0) {
                if (i + 1 < argc) parentPid = (DWORD)_wtoi(argv[++i]);
                if (i + 1 < argc) exePath   = argv[++i];
            }
        }
        LocalFree(argv);
        return SelfGuard::RunWatchdog(parentPid, exePath);
    }
    LocalFree(argv);

    // ── Single instance check ─────────────────────────────────────
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Bring existing window to front
        HWND existing = FindWindowW(L"ProcessGuardWnd", nullptr);
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // ── Start self-guard watchdog ─────────────────────────────────
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    HANDLE hWatchdog = SelfGuard::StartWatchdog(exePath);

    // ── Create engine & window ────────────────────────────────────
    GuardEngine engine;
    g_engine = &engine;

    HWND hwnd = CreateMainWindow(hInst);
    if (!hwnd) {
        CloseHandle(hMutex);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ── Message loop ──────────────────────────────────────────────
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ── Cleanup ───────────────────────────────────────────────────
    engine.Stop();

    // Write sentinel file so watchdog knows this is a clean (intentional) exit
    if (hWatchdog != INVALID_HANDLE_VALUE) {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        std::wstring sentinel = std::wstring(tmp) + L"ProcessGuard.exit";
        HANDLE hf = CreateFileW(sentinel.c_str(), GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf);

        // Give watchdog a moment to see the sentinel before we terminate it
        Sleep(200);
        TerminateProcess(hWatchdog, 0);
        CloseHandle(hWatchdog);
    }
    CloseHandle(hMutex);
    return (int)msg.wParam;
}
