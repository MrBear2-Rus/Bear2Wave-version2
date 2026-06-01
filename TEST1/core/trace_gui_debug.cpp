#include "core/trace_gui_debug.h"

#include "core/bear2wave_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#endif

namespace {

std::mutex g_fstDbgMutex;
FILE* g_fstDbgFile = nullptr;
char g_fstDbgPath[1024] = {};
bool g_fstDbgInitDone = false;

#ifdef _WIN32
static std::string exe_directory_utf8()
{
    wchar_t exe[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0)
        return {};
    wchar_t* slash = wcsrchr(exe, L'\\');
    if (slash)
        *(slash + 1) = L'\0';
    char buf[1024] = {};
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, buf, static_cast<int>(sizeof(buf)), nullptr, nullptr);
    buf[sizeof(buf) - 1] = '\0';
    return std::string(buf);
}
#else
static std::string exe_directory_utf8()
{
    char exe[PATH_MAX] = {};
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0)
        return {};
    exe[n] = '\0';
    char* slash = strrchr(exe, '/');
    if (slash)
        *(slash + 1) = '\0';
    return std::string(exe);
}
#endif

static void resolve_fst_debug_path(char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    out[0] = '\0';

    const char* env_path = std::getenv("BEAR2WAVE_FST_DEBUG_FILE");
    if (env_path && env_path[0]) {
        strncpy(out, env_path, out_len - 1);
        out[out_len - 1] = '\0';
        return;
    }

    const std::string dir = exe_directory_utf8();
    if (!dir.empty())
        snprintf(out, out_len, "%serr.txt", dir.c_str());
    else
        strncpy(out, "err.txt", out_len - 1);
    out[out_len - 1] = '\0';
}

static FILE* fst_debug_file()
{
    if (g_fstDbgInitDone)
        return g_fstDbgFile;
    g_fstDbgInitDone = true;

    if (!trace_fst_debug_enabled())
        return nullptr;

    resolve_fst_debug_path(g_fstDbgPath, sizeof(g_fstDbgPath));
    g_fstDbgFile = fopen(g_fstDbgPath, "w");
    if (!g_fstDbgFile)
        return nullptr;

    SYSTEMTIME st = {};
#ifdef _WIN32
    GetLocalTime(&st);
    fprintf(g_fstDbgFile,
        "=== Bear2Wave FST debug log ===\n"
        "file: %s\n"
        "started: %04u-%02u-%02u %02u:%02u:%02u\n\n",
        g_fstDbgPath,
        (unsigned)st.wYear,
        (unsigned)st.wMonth,
        (unsigned)st.wDay,
        (unsigned)st.wHour,
        (unsigned)st.wMinute,
        (unsigned)st.wSecond);
#else
    (void)st;
    fprintf(g_fstDbgFile, "=== Bear2Wave FST debug log ===\nfile: %s\n\n", g_fstDbgPath);
#endif
    fflush(g_fstDbgFile);
    return g_fstDbgFile;
}

} // namespace

int trace_fst_debug_enabled(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char* e = std::getenv("BEAR2WAVE_FST_DEBUG");
        cached = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return cached;
}

unsigned long trace_fst_thread_id(void)
{
#ifdef _WIN32
    return static_cast<unsigned long>(GetCurrentThreadId());
#else
    return static_cast<unsigned long>(pthread_self());
#endif
}

void trace_fst_debug_init(void)
{
    if (!trace_fst_debug_enabled())
        return;
    std::lock_guard<std::mutex> lock(g_fstDbgMutex);
    fst_debug_file();
}

const char* trace_fst_debug_log_path(void)
{
    if (!trace_fst_debug_enabled())
        return nullptr;
    std::lock_guard<std::mutex> lock(g_fstDbgMutex);
    if (!g_fstDbgInitDone)
        fst_debug_file();
    return g_fstDbgPath[0] ? g_fstDbgPath : nullptr;
}

void trace_fst_log(const char* stage, const char* fmt, ...)
{
    if (!trace_fst_debug_enabled() || !stage)
        return;

    char body[2048];
    body[0] = '\0';
    if (fmt && fmt[0]) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(body, sizeof(body), fmt, ap);
        va_end(ap);
        body[sizeof(body) - 1] = '\0';
    }

    const unsigned long tid = trace_fst_thread_id();
    char line[2304];
    if (body[0])
        snprintf(line, sizeof(line), "[tid=%lu][%s] %s", tid, stage, body);
    else
        snprintf(line, sizeof(line), "[tid=%lu][%s]", tid, stage);
    line[sizeof(line) - 1] = '\0';

    const char* full = line;
    char prefixed[2400];
    snprintf(prefixed, sizeof(prefixed), "[Bear2Wave][FST-DBG] %s", line);
    prefixed[sizeof(prefixed) - 1] = '\0';

    {
        std::lock_guard<std::mutex> lock(g_fstDbgMutex);
        FILE* fp = fst_debug_file();
        if (fp) {
            fprintf(fp, "%s\n", prefixed);
            fflush(fp);
        }
    }

    fprintf(stderr, "%s\n", prefixed);
    fflush(stderr);
    B2W_LOG_INFO("[FST-DBG] %s", line);
}
