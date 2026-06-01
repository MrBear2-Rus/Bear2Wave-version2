#pragma once

/**
 * TraceBackend (P4-2): unified lazy-load API over FST/VZT/LXT2/GHW.
 * Implementation delegates to trace_loader / *\_loader; this header is the
 * contract for future sidecar .idx and streaming VCD backends.
 */

#include "vcd.h"
#include "trace_loader.h"

#include <cstdint>
#include <cstddef>

namespace TraceBackend {

enum class Kind {
    None = 0,
    FstLazy = 1,
    VztLazy = 2,
    Lxt2Lazy = 3,
    GhwLazy = 4,
    VcdFull = 5,
    VcdLazy = 6
};

inline Kind FromVcd(const vcd_t* vcd)
{
    if (!vcd)
        return Kind::None;
    switch (vcd->trace_backend) {
    case VCD_TRACE_BACKEND_FST_LAZY: return Kind::FstLazy;
    case VCD_TRACE_BACKEND_VZT_LAZY: return Kind::VztLazy;
    case VCD_TRACE_BACKEND_LXT2_LAZY: return Kind::Lxt2Lazy;
    case VCD_TRACE_BACKEND_GHW_LAZY: return Kind::GhwLazy;
    case VCD_TRACE_BACKEND_VCD_LAZY: return Kind::VcdLazy;
    default:
        return trace_uses_lazy_backend(vcd) ? Kind::None : Kind::VcdFull;
    }
}

inline bool UsesLazyIO(Kind k)
{
    return k == Kind::FstLazy || k == Kind::VztLazy || k == Kind::Lxt2Lazy || k == Kind::GhwLazy || k == Kind::VcdLazy;
}

/** openHierarchy equivalent: already done via trace_load_from_path. */
inline bool IsHierarchyReady(const vcd_t* vcd) { return vcd != nullptr; }

/**
 * loadWindow: load [t0,t1] for signals. Returns 0 ok, -1 error, 1 cancelled.
 * Same semantics as trace_load_signals.
 */
inline int LoadWindow(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
    return trace_load_signals(vcd, sigs, count, t0, t1);
}

inline void Cancel(vcd_t* vcd) { trace_loader_request_cancel(vcd); }

} // namespace TraceBackend
