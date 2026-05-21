#pragma once



#include "vcd.h"



#ifdef __cplusplus

extern "C" {

#endif



/** Hierarchy only; keeps VZT reader open for trace_load_signals. */

vcd_t* vzt_open_lazy(const char* utf8_path);



/** Full load (TraceTools / compatibility). */

vcd_t* vzt_read_to_vcd(const char* utf8_path);



int vzt_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1);



void vzt_trace_session_destroy(void* session);



void vzt_loader_request_cancel(vcd_t* vcd);



#ifdef __cplusplus

}

#endif

