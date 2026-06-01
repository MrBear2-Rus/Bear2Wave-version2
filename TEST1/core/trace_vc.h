#pragma once

#include "vcd.h"
#include "core/waveform_perf.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define TRACE_VC_STORAGE_FULL 0u
#define TRACE_VC_STORAGE_COMPACT 1u

#ifdef __cplusplus
extern "C" {
#endif

int trace_vc_compact_enabled(void);
int trace_signal_compact_eligible(const signal_t* sig);
size_t trace_vc_count(const signal_t* sig);
uint64_t trace_vc_timestamp(const signal_t* sig, size_t index);
void trace_vc_format_value(const signal_t* sig, size_t index, char* buf, size_t buf_len);
const value_change_t* trace_vc_legacy_ptr(const signal_t* sig);
int trace_vc_append(signal_t* sig, timestamp_t ts, const char* value);
void trace_vc_free_storage(signal_t* sig);
void trace_vc_shrink_to_fit(signal_t* sig);
void trace_vc_sort(signal_t* sig);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

inline bool trace_vc_is_compact(const signal_t* sig)
{
    return sig && sig->vc_storage == TRACE_VC_STORAGE_COMPACT;
}

inline bool trace_vc_has_data(const signal_t* sig)
{
    return sig && trace_vc_count(sig) > 0;
}

#endif
