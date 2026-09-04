#include "pch.h"
#include "core/Logger.h"

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>

namespace smpd::log {
namespace {

// Single log file stored next to LumaCore's own logs.
std::string g_file;
std::mutex g_mtx;

void WriteLine(const char* level, const char* fmt, va_list ap) {
    char msg[4096] = {};
    vsnprintf(msg, sizeof(msg) - 1, fmt, ap);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char line[4608] = {};
    _snprintf_s(line, sizeof(line), _TRUNCATE, "[%02d:%02d:%02d][SMPD][%s] %s",
                st.wHour, st.wMinute, st.wSecond, level, msg);

    std::lock_guard<std::mutex> lk(g_mtx);
    if (!g_file.empty()) {
        FILE* fh = nullptr;
        fopen_s(&fh, g_file.c_str(), "a");
        if (fh) {
            fprintf(fh, "%s\n", line);
            fclose(fh);
        }
    }
    OutputDebugStringA(line);
}

}  // namespace

void Init(const std::string& steamDir) {
    g_file = steamDir + "\\lumacore\\smpd_bridge.log";
    std::error_code ec;
    std::filesystem::create_directories(steamDir + "\\lumacore", ec);

    // Session header: when this is present, the DLL was loaded and the worker runs.
    FILE* fh = nullptr;
    fopen_s(&fh, g_file.c_str(), "a");
    if (fh) {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        fprintf(fh, "\n===== SMPD session %04d-%02d-%02d %02d:%02d:%02d pid=%lu =====\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                (unsigned long)GetCurrentProcessId());
        fclose(fh);
    }
}

void Info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    WriteLine("INFO", fmt, ap);
    va_end(ap);
}

void Warn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    WriteLine("WARN", fmt, ap);
    va_end(ap);
}

}  // namespace smpd::log
