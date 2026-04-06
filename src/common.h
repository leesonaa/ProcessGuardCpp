#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

#define IDI_MAIN          101
#define IDI_TRAY          102
#define IDC_LIST          201
#define IDC_BTN_ADD       202
#define IDC_BTN_REMOVE    203
#define IDC_SEARCH        204
#define IDC_TIMER_REFRESH 1001
#define IDC_TIMER_ANIM    1002

#define COL_NAME    0
#define COL_STATUS  1
#define COL_CPU     2
#define COL_MEM     3
#define COL_PID     4
#define COL_RESTART 5

#define CLR_BG          RGB(240, 242, 250)
#define CLR_HEADER_BG   RGB(255, 255, 255)
#define CLR_CARD        RGB(255, 255, 255)
#define CLR_CARD_RUN    RGB(240, 253, 244)
#define CLR_ACCENT      RGB(48,  112, 240)
#define CLR_GREEN       RGB(22,  163,  74)
#define CLR_ORANGE      RGB(234,  88,  12)
#define CLR_RED         RGB(220,  38,  38)
#define CLR_GRAY        RGB(148, 158, 172)
#define CLR_TEXT        RGB(15,   20,  40)
#define CLR_TEXT_DIM    RGB(110, 118, 140)
#define CLR_BORDER      RGB(218, 222, 234)

enum class ProcStatus {
    Running, Stopped, Restarting, NotResponding, Disabled, FileMissing, StartFailed,
};

inline const wchar_t* StatusName(ProcStatus s) {
    switch(s) {
        case ProcStatus::Running:       return L"\u8FD0\u884C\u4E2D";
        case ProcStatus::Stopped:       return L"\u672A\u542F\u52A8";
        case ProcStatus::Restarting:    return L"\u91CD\u542F\u4E2D";
        case ProcStatus::NotResponding: return L"\u672A\u54CD\u5E94";
        case ProcStatus::Disabled:      return L"\u5DF2\u7981\u7528";
        case ProcStatus::FileMissing:   return L"\u6587\u4EF6\u4E22\u5931";
        case ProcStatus::StartFailed:   return L"\u542F\u52A8\u5931\u8D25";
        default:                        return L"\u672A\u77E5";
    }
}

inline COLORREF StatusColor(ProcStatus s) {
    switch(s) {
        case ProcStatus::Running:       return CLR_GREEN;
        case ProcStatus::Restarting:    return CLR_ORANGE;
        case ProcStatus::NotResponding: return CLR_RED;
        case ProcStatus::StartFailed:   return CLR_RED;
        case ProcStatus::FileMissing:   return CLR_RED;
        default:                        return CLR_GRAY;
    }
}

struct GuardedProcess {
    std::wstring exePath;
    std::wstring name;
    bool         enabled        = true;
    DWORD        restartDelayMs = 3000;
    int          maxRestarts    = 0;
    std::wstring args;

    DWORD        pid            = 0;
    HANDLE       hProcess       = INVALID_HANDLE_VALUE;
    ProcStatus   status         = ProcStatus::Stopped;
    int          restartCount   = 0;
    float        cpuPercent     = 0.f;
    float        memMB          = 0.f;
    ULONGLONG    lastCpuTime    = 0;
    ULONGLONG    lastWallTime   = 0;

    ~GuardedProcess() {
        if (hProcess != INVALID_HANDLE_VALUE)
            CloseHandle(hProcess);
    }
};
