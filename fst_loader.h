#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Hierarchy only + open FST reader; value changes loaded on demand. */
vcd_t* fst_open_lazy(const char* utf8_path);

/** Load full FST into memory (tools / compatibility). */
vcd_t* fst_read_to_vcd(const char* utf8_path);

/** Load value changes for signals (FST lazy backend). Returns 0 ok, -1 error, 1 cancelled. */
int fst_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);

void fst_trace_session_destroy(void* session);

/** Request cancel of in-flight fstReaderIterBlocks2 (same vcd as fst_open_lazy). */
void fst_loader_request_cancel(vcd_t* vcd);

/** Enable stderr FST load trace (also active if env BEAR2WAVE_FST_DEBUG is set). */
void fst_loader_set_debug(int enabled);

/**
 * Optional: mirror each [FST] diagnostic line to the UI (e.g. wxLogMessage).
 * Called on the same thread as fst_read_to_vcd. Set enabled=0 to disable.
 */
void fst_loader_set_line_logger(void (*fn)(const char* line, void* user), void* user);
void fst_loader_set_line_logger_enabled(int enabled);

#ifdef __cplusplus
}
#endif
