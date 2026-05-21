#pragma once

#include <stdint.h>

/** Bear2Wave trace kind tags stored in signal_t::fst_var_type (when not raw FST enum). */
#define BEAR2WAVE_VT_VCD_ONLY   (-1)
#define BEAR2WAVE_VT_STRING     1001
#define BEAR2WAVE_VT_TIME        1002
#define BEAR2WAVE_VT_TRANSACTION 1003
#define BEAR2WAVE_VT_REAL        1004
#define BEAR2WAVE_VT_ANALOG      1005

/** Map VZT reader flags (libvzt) to Bear2Wave fst_var_type. */
inline int32_t Bear2waveVarTypeFromVztFlags(int32_t vzt_flags)
{
    if (vzt_flags & (1 << 2)) /* VZT_RD_SYM_F_STRING */
        return BEAR2WAVE_VT_STRING;
    if (vzt_flags & (1 << 3)) /* VZT_RD_SYM_F_TIME */
        return BEAR2WAVE_VT_TIME;
    if (vzt_flags & (1 << 1)) /* VZT_RD_SYM_F_DOUBLE */
        return BEAR2WAVE_VT_REAL;
    return BEAR2WAVE_VT_VCD_ONLY;
}
