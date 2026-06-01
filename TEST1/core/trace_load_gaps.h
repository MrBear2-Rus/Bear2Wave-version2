#pragma once

#include "vcd.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cstdint>

/** True if timestamp already has loaded VC data (incremental extend skips these). */
inline int trace_ts_in_loaded_span(const signal_t* sig, uint64_t ts)
{
    return sig && sig->trace_data_loaded && ts >= sig->trace_loaded_t0
        && ts <= sig->trace_loaded_t1;
}

inline void trace_gap_union(uint64_t g0, uint64_t g1, uint64_t& union_t0, uint64_t& union_t1)
{
    if (g0 > g1)
        return;
    if (union_t0 == UINT64_MAX) {
        union_t0 = g0;
        union_t1 = g1;
    } else {
        union_t0 = std::min(union_t0, g0);
        union_t1 = std::max(union_t1, g1);
    }
}

/** Union only time gaps needed to satisfy [t0,t1] vs sig's current loaded span. */
inline void trace_compute_sig_gaps(
    const signal_t* sig, uint64_t t0, uint64_t t1, uint64_t& union_t0, uint64_t& union_t1)
{
    if (!sig)
        return;
    if (!sig->trace_data_loaded) {
        trace_gap_union(t0, t1, union_t0, union_t1);
        return;
    }
    if (t0 >= sig->trace_loaded_t0 && t1 <= sig->trace_loaded_t1)
        return;
    if (t0 < sig->trace_loaded_t0) {
        const uint64_t g1 = sig->trace_loaded_t0 == 0 ? 0 : (sig->trace_loaded_t0 - 1);
        trace_gap_union(t0, g1, union_t0, union_t1);
    }
    if (t1 > sig->trace_loaded_t1) {
        const uint64_t g0 = (sig->trace_loaded_t1 == UINT64_MAX) ? UINT64_MAX : (sig->trace_loaded_t1 + 1);
        trace_gap_union(g0, t1, union_t0, union_t1);
    }
}

inline void trace_extend_loaded_span(signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig)
        return;
    if (!sig->trace_data_loaded) {
        sig->trace_loaded_t0 = t0;
        sig->trace_loaded_t1 = t1;
    } else {
        sig->trace_loaded_t0 = std::min(sig->trace_loaded_t0, t0);
        sig->trace_loaded_t1 = std::max(sig->trace_loaded_t1, t1);
    }
    sig->trace_data_loaded = 1;
}
