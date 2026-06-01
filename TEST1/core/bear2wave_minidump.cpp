#include "core/bear2wave_minidump.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static int minidump_enabled()
{
    const char* e = std::getenv("BEAR2WAVE_MINIDUMP");
    if (!e || !e[0])
        return 1;
    return (e[0] != '0' && e[0] != 'n' && e[0] != 'N') ? 1 : 0;
}

static LONG WINAPI b2w_unhandled_exception_filter(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    wchar_t temp[MAX_PATH] = {};
    if (GetTempPathW(MAX_PATH, temp) == 0)
        return EXCEPTION_CONTINUE_SEARCH;

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t path[MAX_PATH] = {};
    swprintf_s(
        path,
        L"%sBear2Wave-%lu-%04u%02u%02u-%02u%02u%02u.dmp",
        temp,
        static_cast<unsigned long>(GetCurrentProcessId()),
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond);

    HANDLE file = CreateFileW(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    MINIDUMP_EXCEPTION_INFORMATION mei = {};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    const BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        MiniDumpWithDataSegs,
        &mei,
        nullptr,
        nullptr);
    CloseHandle(file);

    if (ok) {
        wchar_t msg[512] = {};
        swprintf_s(msg, L"Bear2Wave crashed. Minidump saved to:\n%s", path);
        MessageBoxW(nullptr, msg, L"Bear2Wave", MB_OK | MB_ICONERROR);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void b2w_minidump_install(void)
{
    if (!minidump_enabled())
        return;
    SetUnhandledExceptionFilter(b2w_unhandled_exception_filter);
}

#else

void b2w_minidump_install(void) {}

#endif
