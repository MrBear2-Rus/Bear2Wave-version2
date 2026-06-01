#pragma once

#include "vcd.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef __cplusplus

struct TransactionTraceChange {
    uint64_t time = 0;
    std::string value;
};

struct TransactionVirtualTrace {
    std::string name;
    std::vector<TransactionTraceChange> changes;
    /** GTKWave-style row tint (e.g. darkblue) from decoder output. */
    std::string row_color;
};

struct TransactionMarker {
    uint64_t time = 0;
    std::string label;
};

struct TransactionProcessResult {
    std::vector<TransactionVirtualTrace> traces;
    std::vector<TransactionMarker> markers;
    std::string stderr_text;
    int exit_code = -1;
    bool timed_out = false;
};

/**
 * Run transaction_proc: stdin = minimal VCD, stdout = $name / #time value lines.
 * Returns 0 on success, -2 if executable missing, -1 on failure.
 */
int trace_transaction_run(
    const std::string& input_vcd,
    TransactionProcessResult* out,
    char* err_buf = nullptr,
    size_t err_buf_len = 0);

/** Parse transaction_proc stdout (also used by tests). Returns 0 on success. */
int trace_transaction_parse_stdout(
    const std::string& stdout_text,
    TransactionProcessResult* out,
    char* err_buf = nullptr,
    size_t err_buf_len = 0);

/** Fill signal_t + backing storage from one decoded virtual trace. */
void trace_transaction_fill_synthetic(
    const TransactionVirtualTrace& trace,
    signal_t* out_sig,
    std::vector<value_change_t>* storage);

#endif
