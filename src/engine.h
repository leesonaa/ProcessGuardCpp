#pragma once
#include "common.h"
#include "config.h"

class GuardEngine {
public:
    std::vector<GuardedProcess*> procs;
    std::mutex                   lock;

    GuardEngine() {
        // Load saved config
        auto loaded = Config::Load();
        for (auto* p : loaded) procs.push_back(p);
    }

    ~GuardEngine() {
        Stop();
        std::lock_guard<std::mutex> g(lock);
        for (auto* p : procs) delete p;
    }

    // ── Public API ────────────────────────────────────────────────

    void AddProcess(const std::wstring& exePath) {
        std::lock_guard<std::mutex> g(lock);
        // Check duplicate
        for (auto* p : procs)
            if (p->exePath == exePath) return;

        auto* gp = new GuardedProcess();
        gp->exePath = exePath;
        // Name = filename only
        gp->name = exePath.substr(exePath.find_last_of(L"\\/") + 1);
        gp->enabled = false;
        gp->status  = ProcStatus::Disabled;
        procs.push_back(gp);
        Config::Save(procs);
    }

    void RemoveProcess(size_t idx) {
        std::lock_guard<std::mutex> g(lock);
        if (idx >= procs.size()) return;
        GuardedProcess* gp = procs[idx];
        KillProcess(gp);
        procs.erase(procs.begin() + idx);
        delete gp;
        Config::Save(procs);
    }

    void SetEnabled(size_t idx, bool enabled) {
        std::lock_guard<std::mutex> g(lock);
        if (idx < procs.size()) {
            procs[idx]->enabled = enabled;
            if (!enabled) {
                procs[idx]->status = ProcStatus::Disabled;
            }
            Config::Save(procs);
        }
    }

    // Toggle enabled state.
    // Disabling: stop guarding (process keeps running).
    // Enabling:  start process if it's not already alive.
    void ToggleEnabled(size_t idx) {
        std::lock_guard<std::mutex> g(lock);
        if (idx >= procs.size()) return;
        bool en = !procs[idx]->enabled;
        procs[idx]->enabled = en;
        if (!en) {
            // Do NOT kill the process — just stop watching it.
            procs[idx]->status = ProcStatus::Disabled;
        } else {
            procs[idx]->restartCount = 0;
            if (!IsAlive(procs[idx]))
                StartProcess(procs[idx]);
            else
                procs[idx]->status = ProcStatus::Running;
        }
        Config::Save(procs);
    }

    void Start() {
        running = true;
        monitorThread = std::thread(&GuardEngine::MonitorLoop, this);
    }

    void Stop() {
        running = false;
        if (monitorThread.joinable()) monitorThread.join();
    }

private:
    std::atomic<bool> running{ false };
    std::thread       monitorThread;

    // ── Process control ───────────────────────────────────────────

    bool StartProcess(GuardedProcess* gp) {
        if (!PathFileExistsW(gp->exePath.c_str())) {
            gp->status = ProcStatus::FileMissing;
            return false;
        }

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWDEFAULT;

        PROCESS_INFORMATION pi{};
        std::wstring cmdLine = L"\"" + gp->exePath + L"\"";
        if (!gp->args.empty()) cmdLine += L" " + gp->args;

        // workDir = dir of exe
        std::wstring workDir = gp->exePath.substr(0, gp->exePath.find_last_of(L"\\/"));

        if (CreateProcessW(
                nullptr, &cmdLine[0],
                nullptr, nullptr, FALSE,
                0, nullptr,
                workDir.c_str(),
                &si, &pi)) {
            if (gp->hProcess != INVALID_HANDLE_VALUE) CloseHandle(gp->hProcess);
            gp->hProcess = pi.hProcess;
            gp->pid      = pi.dwProcessId;
            gp->status   = ProcStatus::Running;
            CloseHandle(pi.hThread);
            // Initialize CPU timing
            InitCpuTracking(gp);
            return true;
        }
        gp->status = ProcStatus::StartFailed;
        return false;
    }

    void KillProcess(GuardedProcess* gp) {
        if (gp->hProcess != INVALID_HANDLE_VALUE) {
            TerminateProcess(gp->hProcess, 0);
            WaitForSingleObject(gp->hProcess, 2000);
            CloseHandle(gp->hProcess);
            gp->hProcess = INVALID_HANDLE_VALUE;
        }
        gp->pid    = 0;
        gp->status = ProcStatus::Stopped;
    }

    bool IsAlive(GuardedProcess* gp) {
        if (gp->hProcess == INVALID_HANDLE_VALUE) return false;
        DWORD code = STILL_ACTIVE;
        GetExitCodeProcess(gp->hProcess, &code);
        return code == STILL_ACTIVE;
    }

    bool IsResponding(GuardedProcess* gp) {
        if (gp->pid == 0) return true;
        // Enumerate top-level windows owned by process
        struct Ctx { DWORD pid; bool found; bool responding; };
        Ctx ctx{ gp->pid, false, true };
        EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            DWORD wpid = 0;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid == c->pid && IsWindowVisible(hwnd)) {
                c->found = true;
                // SendMessageTimeout with 0 timeout just checks message queue
                DWORD_PTR res = 0;
                LRESULT r = SendMessageTimeoutW(hwnd, WM_NULL, 0, 0,
                                SMTO_ABORTIFHUNG|SMTO_BLOCK, 500, &res);
                if (r == 0) c->responding = false;
                return FALSE; // stop after first visible window
            }
            return TRUE;
        }, (LPARAM)&ctx);
        return !ctx.found || ctx.responding;
    }

    // ── CPU/MEM tracking ──────────────────────────────────────────

    void InitCpuTracking(GuardedProcess* gp) {
        FILETIME ct, et, kt, ut;
        if (GetProcessTimes(gp->hProcess, &ct, &et, &kt, &ut)) {
            ULARGE_INTEGER ui;
            ui.LowPart  = ut.dwLowDateTime;
            ui.HighPart = ut.dwHighDateTime;
            gp->lastCpuTime = ui.QuadPart;
        }
        FILETIME now; GetSystemTimeAsFileTime(&now);
        ULARGE_INTEGER nowui;
        nowui.LowPart  = now.dwLowDateTime;
        nowui.HighPart = now.dwHighDateTime;
        gp->lastWallTime = nowui.QuadPart;
    }

    void UpdateStats(GuardedProcess* gp) {
        if (gp->hProcess == INVALID_HANDLE_VALUE) {
            gp->cpuPercent = 0; gp->memMB = 0; return;
        }
        // CPU
        FILETIME ct, et, kt, ut;
        if (GetProcessTimes(gp->hProcess, &ct, &et, &kt, &ut)) {
            ULARGE_INTEGER cpu;
            cpu.LowPart  = ut.dwLowDateTime;
            cpu.HighPart = ut.dwHighDateTime;
            FILETIME now; GetSystemTimeAsFileTime(&now);
            ULARGE_INTEGER wall;
            wall.LowPart  = now.dwLowDateTime;
            wall.HighPart = now.dwHighDateTime;

            ULONGLONG cpuDelta  = cpu.QuadPart  - gp->lastCpuTime;
            ULONGLONG wallDelta = wall.QuadPart  - gp->lastWallTime;
            if (wallDelta > 0) {
                SYSTEM_INFO si; GetSystemInfo(&si);
                gp->cpuPercent = (float)(cpuDelta * 100.0 / wallDelta / si.dwNumberOfProcessors);
            }
            gp->lastCpuTime  = cpu.QuadPart;
            gp->lastWallTime = wall.QuadPart;
        }
        // Memory
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(gp->hProcess, &pmc, sizeof(pmc)))
            gp->memMB = (float)pmc.WorkingSetSize / 1024.f / 1024.f;
    }

    // ── Monitor loop (runs on background thread) ──────────────────

    void MonitorLoop() {
        // Start all enabled processes
        {
            std::lock_guard<std::mutex> g(lock);
            for (auto* gp : procs)
                if (gp->enabled) StartProcess(gp);
        }

        while (running) {
            {
                std::lock_guard<std::mutex> g(lock);
                for (auto* gp : procs) {
                    if (!gp->enabled) { gp->status = ProcStatus::Disabled; continue; }

                    if (IsAlive(gp)) {
                        UpdateStats(gp);
                        if (!IsResponding(gp))
                            gp->status = ProcStatus::NotResponding;
                        else
                            gp->status = ProcStatus::Running;
                    } else {
                        // Process died
                        if (gp->status == ProcStatus::Running ||
                            gp->status == ProcStatus::NotResponding) {
                            gp->restartCount++;
                        }
                        if (gp->hProcess != INVALID_HANDLE_VALUE) {
                            CloseHandle(gp->hProcess);
                            gp->hProcess = INVALID_HANDLE_VALUE;
                        }
                        gp->pid = 0;

                        if (gp->maxRestarts > 0 && gp->restartCount > gp->maxRestarts) {
                            gp->status = ProcStatus::Stopped;
                            continue;
                        }

                        gp->status = ProcStatus::Restarting;
                        // Delay before restart (release lock during sleep)
                        DWORD delay = gp->restartDelayMs;
                        lock.unlock();
                        Sleep(delay);
                        lock.lock();
                        if (!running) break;
                        if (gp->enabled) StartProcess(gp);
                    }
                }
            }
            Sleep(2000); // poll every 2 seconds
        }
    }
};
