#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Path to %LOCALAPPDATA%\\Bear2Wave\\translate_error.txt (UTF-8). */
const char* trace_translate_error_log_path(void);

/** Append one line (printf-style) to translate_error.txt. Always on. */
void trace_translate_error_log(const char* fmt, ...);

/** Write session header; call when user toggles Translate Filter Process. */
void trace_translate_error_session_begin(const char* tag);

#ifdef __cplusplus
}
#endif
