#include "lxt_format.h"

#include <cstdio>
#include <cstring>

enum bear2wave_lxt_variant_t trace_lxt_probe_file(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return BEAR2WAVE_LXT_VARIANT_UNKNOWN;

    FILE* f = fopen(utf8_path, "rb");
    if (!f)
        return BEAR2WAVE_LXT_VARIANT_UNKNOWN;

    unsigned char hdr[2] = {};
    const size_t n = fread(hdr, 1, 2, f);
    fclose(f);
    if (n < 2)
        return BEAR2WAVE_LXT_VARIANT_UNKNOWN;

    const unsigned magic = (static_cast<unsigned>(hdr[0]) << 8) | hdr[1];
    if (magic == BEAR2WAVE_LXT2_HDRID)
        return BEAR2WAVE_LXT_VARIANT_LXT2;
    if (magic == BEAR2WAVE_LXT1_HDRID)
        return BEAR2WAVE_LXT_VARIANT_LXT1;
    return BEAR2WAVE_LXT_VARIANT_UNKNOWN;
}

const char* trace_lxt_variant_name(enum bear2wave_lxt_variant_t variant)
{
    switch (variant) {
    case BEAR2WAVE_LXT_VARIANT_LXT1: return "LXT1";
    case BEAR2WAVE_LXT_VARIANT_LXT2: return "LXT2";
    default: return "unknown";
    }
}
