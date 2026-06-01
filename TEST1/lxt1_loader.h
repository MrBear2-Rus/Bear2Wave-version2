#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load legacy LXT (v1) into memory (VCD_TRACE_BACKEND_NONE).
 * Supports linear gzip change blocks (vcd2lxt -linear / bear2wave gen-lxt).
 */
vcd_t* lxt1_read_to_vcd(const char* utf8_path);

#ifdef __cplusplus
}
#endif
