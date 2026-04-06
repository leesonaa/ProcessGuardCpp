#pragma once
#include "common.h"
#include <fstream>

namespace Config {

inline std::wstring GetConfigPath() {
    wchar_t appdata[MAX_PATH] = {};
    // Use environment variable fallback (works with mingw)
    DWORD r = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    std::wstring dir;
    if (r > 0) {
        dir = std::wstring(appdata) + L"\\ProcessGuard";
    } else {
        dir = L".\\ProcessGuardData";
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\config.json";
}

inline std::string WToU8(const std::wstring& w) {
    if (w.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz <= 1) return {};
    std::string s(sz - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], sz, nullptr, nullptr);
    return s;
}

inline std::wstring U8ToW(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (sz <= 1) return {};
    std::wstring w(sz - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], sz);
    return w;
}

inline std::string EscJson(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"')       r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else                r += c;
    }
    return r;
}

inline void Save(const std::vector<GuardedProcess*>& procs) {
    std::string json = "{\n  \"processes\": [\n";
    for (size_t i = 0; i < procs.size(); ++i) {
        auto* p = procs[i];
        json += "    {\n";
        json += "      \"exePath\": \""       + EscJson(WToU8(p->exePath))        + "\",\n";
        json += "      \"name\": \""          + EscJson(WToU8(p->name))           + "\",\n";
        std::string en = p->enabled ? "true" : "false";
        json += "      \"enabled\": "         + en                                 + ",\n";
        json += "      \"restartDelayMs\": "  + std::to_string(p->restartDelayMs) + ",\n";
        json += "      \"maxRestarts\": "     + std::to_string(p->maxRestarts)    + ",\n";
        json += "      \"args\": \""          + EscJson(WToU8(p->args))           + "\"\n";
        json += "    }";
        if (i + 1 < procs.size()) json += ",";
        json += "\n";
    }
    json += "  ]\n}\n";

    std::wstring path = GetConfigPath();
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hf, json.c_str(), (DWORD)json.size(), &written, nullptr);
        CloseHandle(hf);
    }
}

inline std::string ReadFileStr(const std::wstring& path) {
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return {};
    DWORD sz = GetFileSize(hf, nullptr);
    if (sz == 0 || sz == INVALID_FILE_SIZE) { CloseHandle(hf); return {}; }
    std::string buf(sz, '\0');
    DWORD rd = 0;
    ReadFile(hf, &buf[0], sz, &rd, nullptr);
    CloseHandle(hf);
    buf.resize(rd);
    return buf;
}

inline std::string ExtractStr(const std::string& blk, const std::string& key) {
    std::string search = "\"" + key + "\": \"";
    auto pos = blk.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    std::string val;
    for (; pos < blk.size(); ++pos) {
        if (blk[pos] == '\\' && pos + 1 < blk.size()) { ++pos; val += blk[pos]; }
        else if (blk[pos] == '"') break;
        else val += blk[pos];
    }
    return val;
}

inline std::string ExtractVal(const std::string& blk, const std::string& key) {
    std::string search = "\"" + key + "\": ";
    auto pos = blk.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    std::string val;
    while (pos < blk.size() && blk[pos] != ',' && blk[pos] != '\n' && blk[pos] != '}')
        val += blk[pos++];
    while (!val.empty() && (val.back() == ' ' || val.back() == '\r')) val.pop_back();
    return val;
}

inline std::vector<GuardedProcess*> Load() {
    std::vector<GuardedProcess*> result;
    std::string json = ReadFileStr(GetConfigPath());
    if (json.empty()) return result;

    size_t pos = 0; int depth = 0;
    bool inArray = false;
    size_t blockStart = std::string::npos;

    for (pos = 0; pos < json.size(); ++pos) {
        if (json[pos] == '[') { inArray = true; continue; }
        if (!inArray) continue;
        if (json[pos] == '{') {
            if (depth == 0) blockStart = pos;
            ++depth;
        } else if (json[pos] == '}') {
            --depth;
            if (depth == 0 && blockStart != std::string::npos) {
                std::string block = json.substr(blockStart, pos - blockStart + 1);
                auto* gp = new GuardedProcess();
                gp->exePath        = U8ToW(ExtractStr(block, "exePath"));
                gp->name           = U8ToW(ExtractStr(block, "name"));
                gp->args           = U8ToW(ExtractStr(block, "args"));
                gp->enabled        = (ExtractVal(block, "enabled") == "true");
                std::string rd     = ExtractVal(block, "restartDelayMs");
                gp->restartDelayMs = rd.empty() ? 3000 : (DWORD)std::stoul(rd);
                std::string mr     = ExtractVal(block, "maxRestarts");
                gp->maxRestarts    = mr.empty() ? 0 : std::stoi(mr);
                if (!gp->exePath.empty()) result.push_back(gp);
                else delete gp;
                blockStart = std::string::npos;
            }
        }
    }
    return result;
}

} // namespace Config
