#include "core/bear2wave_log.h"

#include "core/waveform_perf.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <string.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

std::mutex g_logMutex;
B2wLogLevel g_level = B2wLogLevel::Warn;
FILE* g_logFile = nullptr;
bool g_initialized = false;

const char* LevelTag(B2wLogLevel lv)
{
    switch (lv) {
    case B2wLogLevel::Error: return "ERROR";
    case B2wLogLevel::Warn: return "WARN";
    case B2wLogLevel::Info: return "INFO";
    case B2wLogLevel::Debug: return "DEBUG";
    case B2wLogLevel::Trace: return "TRACE";
    }
    return "?";
}

B2wLogLevel ParseLevelName(const char* name)
{
    if (!name || !name[0])
        return B2wLogLevel::Warn;
    if (!_stricmp(name, "error") || !_stricmp(name, "err"))
        return B2wLogLevel::Error;
    if (!_stricmp(name, "warn") || !_stricmp(name, "warning"))
        return B2wLogLevel::Warn;
    if (!_stricmp(name, "info"))
        return B2wLogLevel::Info;
    if (!_stricmp(name, "debug") || !_stricmp(name, "dbg"))
        return B2wLogLevel::Debug;
    if (!_stricmp(name, "trace") || !_stricmp(name, "verbose"))
        return B2wLogLevel::Trace;
    return B2wLogLevel::Warn;
}

void OpenLogFileIfRequested()
{
    if (g_logFile)
        return;
    if (WaveformPerf::EnvInt("BEAR2WAVE_LOG_FILE", 0) == 0)
        return;

#ifdef _WIN32
    char appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
        return;
    std::string dir = std::string(appData) + "\\Bear2Wave\\logs";
    CreateDirectoryA((std::string(appData) + "\\Bear2Wave").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string path = dir + "\\bear2wave.log";
    g_logFile = fopen(path.c_str(), "a");
#else
    g_logFile = fopen("bear2wave.log", "a");
#endif
}

} // namespace

void b2w_log_init()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_initialized)
        return;
    g_level = b2w_log_level_from_env();
    OpenLogFileIfRequested();
    g_initialized = true;
}

void b2w_log_shutdown()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    g_initialized = false;
}

B2wLogLevel b2w_log_level()
{
    return g_level;
}

void b2w_log_set_level(B2wLogLevel level)
{
    g_level = level;
}

B2wLogLevel b2w_log_level_from_env()
{
    const char* e = std::getenv("BEAR2WAVE_LOG_LEVEL");
    if (!e || !e[0])
        return B2wLogLevel::Warn;
    return ParseLevelName(e);
}

void b2w_log(B2wLogLevel level, const char* fmt, ...)
{
    if (!fmt || level > g_level)
        return;

    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    msg[sizeof(msg) - 1] = '\0';

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (!g_initialized)
        g_level = b2w_log_level_from_env();

    const char* tag = LevelTag(level);
    fprintf(stderr, "[Bear2Wave][%s] %s\n", tag, msg);
    if (g_logFile) {
        fprintf(g_logFile, "[Bear2Wave][%s] %s\n", tag, msg);
        fflush(g_logFile);
    }
}
