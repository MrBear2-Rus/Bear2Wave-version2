#include "lxt_loader.h"

#include "lxt2_loader.h"
#include "lxt_format.h"
#include "lxt1_loader.h"

#include <cstdio>

vcd_t* lxt_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

    const auto variant = trace_lxt_probe_file(utf8_path);
    fprintf(stderr, "[LXT] probe: variant=%s path=\"%s\"\n",
        trace_lxt_variant_name(variant), utf8_path ? utf8_path : "(null)");
    fflush(stderr);
    switch (variant) {
    case BEAR2WAVE_LXT_VARIANT_LXT2:
        fprintf(stderr, "[LXT] -> lxt2_open_lazy\n");
        fflush(stderr);
        return lxt2_open_lazy(utf8_path);
    case BEAR2WAVE_LXT_VARIANT_LXT1:
        fprintf(stderr, "[LXT] -> lxt1_read_to_vcd\n");
        fflush(stderr);
        return lxt1_read_to_vcd(utf8_path);
    default:
        fprintf(stderr,
            "[LXT] Unrecognized header in \"%s\" (expected LXT1 0x0138 or LXT2 0x1380)\n",
            utf8_path);
        return nullptr;
    }
}

int lxt_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
    if (!vcd)
        return -1;
    if (vcd->trace_backend == VCD_TRACE_BACKEND_LXT2_LAZY)
        return lxt2_load_signals(vcd, sigs, count, t0, t1);
    return 0;
}

void lxt_loader_request_cancel(vcd_t* vcd)
{
    if (!vcd)
        return;
    if (vcd->trace_backend == VCD_TRACE_BACKEND_LXT2_LAZY)
        lxt2_loader_request_cancel(vcd);
}
