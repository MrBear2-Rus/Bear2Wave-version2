#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stream-convert a text VCD/EVCD to FST without loading all value changes into RAM.
 * Returns 0 on success, -1 on error.
 */
int vcd_stream_write_fst(const char* vcd_path, const char* fst_path, char* err_buf, size_t err_buf_len);

#ifdef __cplusplus
}
#endif
