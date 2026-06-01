#include "core/trace_translate_debug.h"

#include <ctime>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

static std::mutex g_log_mu;
static std::string g_log_path;
static int g_line_budget = 800;

static std::string default_bear2wave_dir()
{
#ifdef _WIN32
    PWSTR wide = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wide)) && wide) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        std::string out;
        if (n > 1) {
            out.resize(static_cast<size_t>(n - 1));
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
        }
        CoTaskMemFree(wide);
        if (!out.empty())
            return out + "\\Bear2Wave";
    }
#endif
    const char* home = std::getenv("HOME");
    if (home && home[0])
        return std::string(home) + "/.bear2wave";
    return ".bear2wave";
}

static const std::string& log_path()
{
    if (g_log_path.empty())
        g_log_path = (fs::path(default_bear2wave_dir()) / "translate_error.txt").string();
    return g_log_path;
}

const char* trace_translate_error_log_path(void)
{
    return log_path().c_str();
}

void trace_translate_error_log(const char* fmt, ...)
{
    if (!fmt || g_line_budget <= 0)
        return;

    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    char tbuf[64];
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));

    std::lock_guard<std::mutex> lock(g_log_mu);
    --g_line_budget;

    const fs::path path = fs::path(log_path());
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    FILE* f = fopen(path.string().c_str(), "a");
    if (!f)
        return;
    fprintf(f, "[%s] %s\n", tbuf, msg);
    fclose(f);
}

void trace_translate_error_session_begin(const char* tag)
{
    g_line_budget = 800;
    const fs::path path = fs::path(log_path());
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    FILE* f = fopen(path.string().c_str(), "w");
    if (f) {
        fprintf(f, "=== Bear2Wave Translate Filter Process debug ===\n");
        if (tag && tag[0])
            fprintf(f, "session: %s\n", tag);
        fprintf(f, "log_path: %s\n\n", log_path().c_str());
        fclose(f);
    }
    trace_translate_error_log("session_begin tag=%s", tag ? tag : "");
}
