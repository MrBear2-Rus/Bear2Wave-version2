#pragma once



#include "vcd.h"



#ifdef __cplusplus

extern "C" {

#endif



/** Load any supported trace; caller frees with vcd_free. NULL on failure. */

vcd_t* trace_load_from_path(const char* utf8_path, char* err_buf, size_t err_buf_len);



/** Returns lowercase extension without dot, or empty string. */

const char* trace_format_extension(const char* path);



int trace_format_supported(const char* path);



/** Non-zero if file uses on-demand value-change loading. */

int trace_uses_lazy_backend(const vcd_t* vcd);



/**

 * Padded [t0,t1] for viewport loading (margin_ratio e.g. 0.2 = 20% each side).

 * Clamps to [0, file_max].

 */

void trace_compute_padded_range(

    uint64_t view_t0,

    uint64_t view_t1,

    uint64_t file_max,

    double margin_ratio,

    uint64_t* out_t0,

    uint64_t* out_t1);



/**

 * Load value changes for signals in [t0,t1]. Lazy backends only; no-op otherwise.

 * Returns 0 ok, -1 error, 1 cancelled.

 */

int trace_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);



/** Cancel in-flight block iteration (FST/VZT/LXT2 lazy). */

void trace_loader_request_cancel(vcd_t* vcd);



#ifdef __cplusplus

}

#endif

