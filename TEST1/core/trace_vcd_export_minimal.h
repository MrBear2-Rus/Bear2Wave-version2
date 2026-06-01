#pragma once

#include "vcd.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef __cplusplus

struct TraceVcdExportOptions {
    uint64_t t0 = 0;
    uint64_t t1 = UINT64_MAX;
};

/**
 * Export a minimal VCD (definitions + value changes) for selected signals.
 * Includes the last value at or before t0, then all changes in (t0, t1].
 */
int trace_vcd_export_minimal(
    vcd_t* vcd,
    const signal_t** signals,
    size_t signal_count,
    const TraceVcdExportOptions& opts,
    std::string* out_vcd,
    char* err_buf = nullptr,
    size_t err_buf_len = 0);

#endif
