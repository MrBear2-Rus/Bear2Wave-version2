#pragma once

#include <stddef.h>

#ifdef __cplusplus
#include <string>
#endif

#ifdef __cplusplus

/**
 * Run translate_proc with stdin line: full_name\\t<time>\\t<raw_value>\\n
 * Returns 0 on success and writes display text to out_display.
 */
int trace_translate_via_process(
    const char* full_name,
    long long sim_time,
    const char* raw_value,
    std::string* out_display,
    char* err_buf = nullptr,
    size_t err_buf_len = 0);

/** Clear LRU cache (tests / toggle). */
void trace_translate_cache_clear();

#endif
