/*
 * Legacy wrapper — prefer trace_tools.exe:
 *   trace_tools.exe test <path>
 */
#include <cstdio>
#include <cstring>

#include "trace_loader.h"
#include "vcd.h"

int main(int argc, char** argv)
{
    const char* path = (argc > 1 && argv[1][0]) ? argv[1] : "tests\\traces\\bear2wave_sample.fst";
    char err[512] = {};
    fprintf(stderr, "[smoke] trace_load \"%s\"\n", path);
    vcd_t* v = trace_load_from_path(path, err, sizeof(err));
    if (!v) {
        fprintf(stderr, "[smoke] FAIL: %s\n", err);
        return 1;
    }
    fprintf(stderr, "[smoke] OK signals=%zu max_ts=%llu\n",
        v->signals_count,
        (unsigned long long)vcd_get_max_timestamp(v));
    vcd_free(v);
    return 0;
}
