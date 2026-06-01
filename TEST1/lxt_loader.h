#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Open LXT/LXT2: LXT2 lazy, LXT1 full-load into memory. */
vcd_t* lxt_open_lazy(const char* utf8_path);

int lxt_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);

void lxt_loader_request_cancel(vcd_t* vcd);

#ifdef __cplusplus
}
#endif
