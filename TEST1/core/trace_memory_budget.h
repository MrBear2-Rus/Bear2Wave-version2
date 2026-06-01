#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Total value-change rows currently held in memory for this trace. */
size_t trace_count_loaded_changes(const vcd_t* vcd);

/** Bump LRU tick when a signal's data is used or loaded. */
void trace_memory_touch(signal_t* sig);

/**
 * If BEAR2WAVE_MAX_LOADED_CHANGES > 0 and over budget, clear trace data for
 * least-recently-used signals not listed in protect[].
 */
void trace_memory_budget_enforce(vcd_t* vcd, signal_t** protect, size_t protect_count);

/** Internal: drop LRU entries when vcd is freed. */
void trace_memory_budget_on_vcd_free(vcd_t* vcd);

#ifdef __cplusplus
}
#endif
