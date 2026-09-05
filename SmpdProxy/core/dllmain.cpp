#include "pch.h"
// ---------------------------------------------------------------------------
// winmm.dll proxy — FULL forward generated from the real export table
// (System32 winmm.dll: 180 names + ordinal 2).
// Generated files: winmm.def (LIBRARY winmm, name=Smpd_name),
// winmm_stubs.asm (indirect jmp, no signatures to maintain),
// winmm_forward.cpp (fills the pointers from System32, loop-safe like
// LcXInputProxy: never a "WINMM.X" pragma, which would resolve to ourselves).
//
// Why winmm instead of xinput1_4: xinput1_4.dll is already owned by LumaCore.
// winmm.dll is the third name proven with Steam: LuaTools/LuaLoader uses it
// as a proxy inside the Steam folder. version.dll crashes (0xc000007b, the
// 32-bit launcher imports it too), msimg32.dll is never loaded.
// This proxy does NOT load LumaCore (LC's own dwmapi/xinput already do):
// it only runs the pattern bridge + popup + log.
// ---------------------------------------------------------------------------
#include "pch.h"

#include <shlwapi.h>
#include <string>
#include <mutex>
#include <thread>
#include <vector>

#include "core/BridgeCache.h"
#include "core/BridgeConfig.h"
#include "core/Logger.h"

#pragma comment(lib, "shlwapi.lib")

void WinmmForward_Init();

// --- winmm forwarding: see winmm_forward.cpp + winmm_stubs.asm (generated).
// The pointers are filled in DllMain before the host calls any import
// (the loader completes our DllMain first).

namespace {

volatile LONG s_started = 0;
std::string g_steamDir;
smpd::bridge::BridgeConfig g_config;

// NOTE: LumaCore is intentionally NOT loaded here:
// LC's own dwmapi.dll/xinput1_4.dll already do that. Less interference.

// The pattern CI polls Steam on the "0 * * * *" UTC cron (minute 0 of every
// hour). Append a countdown to the next check computed from the user's own
// clock, so it is correct in every timezone (UTC math, local display).
void AppendNextCheckCountdown(std::string& body) {
    FILETIME ftNow{};
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER now{};
    now.LowPart = ftNow.dwLowDateTime;
    now.HighPart = ftNow.dwHighDateTime;

    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    const int nowMinOfDay = utc.wHour * 60 + utc.wMinute;
    const int remainMin = ((nowMinOfDay / 60) + 1) * 60 - nowMinOfDay;  // 1..60

    ULARGE_INTEGER next = now;
    next.QuadPart += (ULONGLONG)remainMin * 60ULL * 10000000ULL;
    const FILETIME ftNext{next.LowPart, next.HighPart};
    SYSTEMTIME utcNext{}, localNext{};
    FileTimeToSystemTime(&ftNext, &utcNext);
    SystemTimeToTzSpecificLocalTime(nullptr, &utcNext, &localNext);

    char line[256] = {};
    _snprintf_s(line, sizeof(line), _TRUNCATE,
                "\r\nNext pattern check in %dh %02dm (at %02d:%02d local time).",
                remainMin / 60, remainMin % 60, localNext.wHour, localNext.wMinute);
    body += line;
}

// Shown only when this run actually wrote pattern files. LumaCore reads the
// cache at startup, so files fetched now apply on the NEXT launch — hence
// the restart notice.
void ShowUpdatePopup(const std::vector<smpd::bridge::PatternChange>& changes) {
    bool fromOstFallback = false;
    for (const auto& c : changes) {
        if (c.source == "ost") {
            fromOstFallback = true;
            break;
        }
    }

    std::string body = "New Steam patterns installed:\r\n\r\n";
    for (const auto& c : changes) {
        body += "- " + c.kind + " (" + c.sha.substr(0, 12) + ") ";
        if (c.action == "downloaded") {
            body += "downloaded from " + c.source + "\r\n";
        } else if (!c.previousSource.empty() && c.previousSource != c.source) {
            body += "updated from " + c.previousSource + " to " + c.source + "\r\n";
        } else {
            body += "upgraded from " + c.source + "\r\n";
        }
    }
    if (fromOstFallback) {
        body += "\r\nNote: some patterns were served by the OST fallback, "
                "which does not publish the cloud patterns. Cloud error "
                "notifications are normal in this case. They will disappear "
                "once migo3 publishes the new patterns.";
    }
    body += "\r\n\r\nPlease restart Steam so the new patterns take effect.";
    AppendNextCheckCountdown(body);
    MessageBoxA(nullptr, body.c_str(), "SMPD -- Pattern Bridge",
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

DWORD WINAPI BridgeThread(LPVOID) {
    smpd::log::Init(g_steamDir);
    if (smpd::bridge::EnsureDefaultConfigExists(g_steamDir))
        smpd::log::Info("Created default smpd_config.txt (OST fallback disabled).");
    smpd::log::Info("SMPD bridge start. steamDir=%s mirror=%s ost=%s",
                    g_steamDir.c_str(), g_config.mirror.empty() ? "<default>" : g_config.mirror.c_str(),
                    g_config.useOst ? "enabled" : "disabled");
    smpd::log::Info("EXE=%s", []{ char e[MAX_PATH]={}; GetModuleFileNameA(nullptr,e,MAX_PATH); return std::string(e); }().c_str());

    const std::vector<smpd::bridge::PatternChange> changes =
        smpd::bridge::EnsureCache(g_steamDir, g_config.mirror, g_config.useOst);

    smpd::log::Info("SMPD bridge done (%zu changed).", changes.size());

    // Silent when nothing changed: no popup on plain launches.
    if (!changes.empty())
        ShowUpdatePopup(changes);
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        WinmmForward_Init();  // fill the stubs before any host import runs

        if (InterlockedCompareExchange(&s_started, 1, 0) == 0) {
            // Same gate as LC: steam.exe only. Additionally requires
            // steamclient64.dll nearby (the real Steam root).
            char exePath[MAX_PATH] = {};
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            const char* exeName = strrchr(exePath, '\\');
            exeName = exeName ? exeName + 1 : exePath;
            if (_stricmp(exeName, "steam.exe") != 0) return TRUE;

            char dllPath[MAX_PATH] = {};
            DWORD len = GetModuleFileNameA(instance, dllPath, MAX_PATH);
            if (len == 0 || len >= MAX_PATH) return TRUE;
            dllPath[MAX_PATH - 1] = '\0';
            PathRemoveFileSpecA(dllPath);
            g_steamDir = dllPath;

            std::string probe = g_steamDir + "\\steamclient64.dll";
            if (GetFileAttributesA(probe.c_str()) == INVALID_FILE_ATTRIBUTES) return TRUE;

            g_config = smpd::bridge::ReadBridgeConfig(g_steamDir);

            HANDLE h = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
            if (h) CloseHandle(h);
        }
    }
    return TRUE;
}
