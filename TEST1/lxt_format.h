#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** LXT family on-disk magic (big-endian uint16 at file start). */
#define BEAR2WAVE_LXT1_HDRID 0x0138u
#define BEAR2WAVE_LXT2_HDRID 0x1380u
#define BEAR2WAVE_LXT1_TRLID 0xB4u

enum bear2wave_lxt_variant_t {
    BEAR2WAVE_LXT_VARIANT_UNKNOWN = 0,
    BEAR2WAVE_LXT_VARIANT_LXT1 = 1,
    BEAR2WAVE_LXT_VARIANT_LXT2 = 2
};

/** Probe file magic; extension is ignored. */
enum bear2wave_lxt_variant_t trace_lxt_probe_file(const char* utf8_path);

/** Human-readable label for logs and errors. */
const char* trace_lxt_variant_name(enum bear2wave_lxt_variant_t variant);

#ifdef __cplusplus
}
#endif
