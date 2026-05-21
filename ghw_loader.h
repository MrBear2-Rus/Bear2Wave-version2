#pragma once



#include "vcd.h"



#ifdef __cplusplus

extern "C" {

#endif



/** Hierarchy only; value changes loaded on demand by re-scanning file. */

vcd_t* ghw_open_lazy(const char* utf8_path);



vcd_t* ghw_read_to_vcd(const char* utf8_path);



int ghw_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);



void ghw_trace_session_destroy(void* session);



#ifdef __cplusplus

}

#endif

