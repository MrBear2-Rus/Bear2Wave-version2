#include "core/trace_display.h"

#include "core/trace_var_types.h"
#include "fstapi.h"

#include <cmath>
#include <cstdlib>

bool bear2wave_parse_trace_double(const char* s, double* out)
{
    if (!s || !s[0] || !out)
        return false;
    char* endp = nullptr;
    const double d = strtod(s, &endp);
    if (endp == s || !std::isfinite(d))
        return false;
    *out = d;
    return true;
}

Bear2waveTraceKind bear2wave_classify_trace_kind(const signal_t* sig)
{
    if (!sig)
        return Bear2waveTraceKind::DigitalScalar;

    const int32_t t = sig->fst_var_type;
    if (t == BEAR2WAVE_VT_TRANSACTION)
        return Bear2waveTraceKind::TransactionEvent;
    if (t == BEAR2WAVE_VT_STRING || t == BEAR2WAVE_VT_TIME)
        return Bear2waveTraceKind::TextString;
    if (t == BEAR2WAVE_VT_REAL || t == BEAR2WAVE_VT_ANALOG)
        return Bear2waveTraceKind::RealAnalog;

    if (t >= 0) {
        if (t == (int32_t)FST_VT_GEN_STRING)
            return Bear2waveTraceKind::TextString;
        if (t == (int32_t)FST_VT_VCD_EVENT)
            return Bear2waveTraceKind::TransactionEvent;
        if (t == (int32_t)FST_VT_VCD_REAL || t == (int32_t)FST_VT_VCD_REAL_PARAMETER
            || t == (int32_t)FST_VT_VCD_REALTIME || t == (int32_t)FST_VT_SV_SHORTREAL)
            return Bear2waveTraceKind::RealAnalog;
        if (sig->size > 1u)
            return Bear2waveTraceKind::BusBits;
        return Bear2waveTraceKind::DigitalScalar;
    }

    if (sig->size > 1u)
        return Bear2waveTraceKind::BusBits;
    return Bear2waveTraceKind::DigitalScalar;
}
