#pragma once

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus

enum class TraceConvertTarget {
    Unknown = 0,
    Vcd,
    Fst,
    Lxt,
    Lxt2,
    Vzt,
};

/** Map output extension (lowercase, no dot) to convert target; Unknown if unsupported. */
TraceConvertTarget trace_convert_target_from_extension(const char* ext);

/** Non-zero if this build can write the target format. */
int trace_convert_target_available(TraceConvertTarget target);

/** Human-readable target label for UI / errors. */
const char* trace_convert_target_label(TraceConvertTarget target);

/**
 * Convert trace file src_path -> dst_path (extensions determine formats).
 * Sources: all trace_loader formats (VPD/WLF/FSDB via E4 external tools).
 * Targets: VCD, FST, LXT, LXT2, VZT (GTKWave/libfst writers).
 * Returns 0 on success, -1 on error, -2 if target needs BEAR2WAVE_WITH_* at build time.
 */
int trace_convert_path(
    const char* src_path,
    const char* dst_path,
    char* err_buf,
    size_t err_buf_len);

void trace_convert_print_capabilities(FILE* out);

#endif
