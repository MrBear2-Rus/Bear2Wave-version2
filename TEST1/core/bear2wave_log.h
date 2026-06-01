#pragma once

/** E3-1: leveled logging (stderr + optional file). Controlled by BEAR2WAVE_LOG_LEVEL. */

enum class B2wLogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
};

void b2w_log_init();
void b2w_log_shutdown();

B2wLogLevel b2w_log_level();
void b2w_log_set_level(B2wLogLevel level);
B2wLogLevel b2w_log_level_from_env();

void b2w_log(B2wLogLevel level, const char* fmt, ...);

#define B2W_LOG_ERROR(...) b2w_log(B2wLogLevel::Error, __VA_ARGS__)
#define B2W_LOG_WARN(...)  b2w_log(B2wLogLevel::Warn, __VA_ARGS__)
#define B2W_LOG_INFO(...)  b2w_log(B2wLogLevel::Info, __VA_ARGS__)
#define B2W_LOG_DEBUG(...) b2w_log(B2wLogLevel::Debug, __VA_ARGS__)
#define B2W_LOG_TRACE(...) b2w_log(B2wLogLevel::Trace, __VA_ARGS__)
