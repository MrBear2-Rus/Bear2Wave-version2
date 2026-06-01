#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Hierarchy-only open: parses $scope/$var/$enddefinitions only, skips value data. */
vcd_t* vcd_open_lazy(const char* utf8_path);

/** Load value changes for selected signals in [t0,t1] from the data section.
 *  Returns 0 on success, -1 on error, 1 if cancelled. */
int vcd_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);

/** Release VCD lazy session (FILE*, index, etc.). */
void vcd_lazy_session_destroy(void* session);

/** Request cancel of in-flight VCD data scan. */
void vcd_loader_request_cancel(vcd_t* vcd);

#ifdef __cplusplus
}
typedef void (*VcdLoaderProgressFn)(void* user, uint32_t block_cur, uint32_t block_total, uint32_t signals, uint64_t elapsed_ms);
void vcd_loader_set_progress_callback(vcd_t* vcd, VcdLoaderProgressFn fn, void* user);
#endif
