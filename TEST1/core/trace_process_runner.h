#pragma once

#include <string>

/** Result of running an external tool with optional stdin payload. */
struct TraceProcessResult {
    int exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
    bool timed_out = false;
};

/**
 * Run executable with stdin_data written to child stdin.
 * timeout_ms <= 0 waits indefinitely.
 */
TraceProcessResult trace_process_run(
    const std::string& executable,
    const std::string& stdin_data,
    int timeout_ms);
