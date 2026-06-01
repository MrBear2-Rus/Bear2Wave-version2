#pragma once



#include "vcd.h"



#ifdef __cplusplus

extern "C" {

#endif



vcd_t* lxt2_open_lazy(const char* utf8_path);

vcd_t* lxt2_read_to_vcd(const char* utf8_path);

int lxt2_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);

void lxt2_trace_session_destroy(void* session);

void lxt2_loader_request_cancel(vcd_t* vcd);



#ifdef __cplusplus

}

#endif

