/*
 * Command-line smoke test for fst_read_to_vcd (same path as the wx app).
 *
 * Build from repo root (MinGW):
 *   g++ -std=c++17 -O2 -o fst_load_smoke.exe tools/fst_load_smoke.cpp ^
 *     TEST1/fst_loader.cpp TEST1/vcd.cpp ^
 *     third_party/libfst/src/fstapi.c third_party/libfst/src/fastlz.c third_party/libfst/src/lz4.c ^
 *     -I TEST1 -I third_party/libfst/src ^
 *     -D_GNU_SOURCE -DHAVE_LIBPTHREAD=0 -DFST_DO_MISALIGNED_OPS -D__MINGW32__ ^
 *     -DFST_CONFIG_INCLUDE=\"fstapi.h\" -lz
 *
 * Usage: fst_load_smoke.exe path\to\file.fst
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fst_loader.h"
#include "vcd.h"

int main(int argc, char** argv)
{
    const char* path = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : "test2.fst";

    fst_loader_set_debug(1);
    fprintf(stderr, "[smoke] loading \"%s\" ...\n", path);

    vcd_t* v = fst_read_to_vcd(path);
    if (!v) {
        fprintf(stderr, "[smoke] FAIL: fst_read_to_vcd returned NULL\n");
        return 1;
    }

    timestamp_t max_ts = vcd_get_max_timestamp(v);
    fprintf(stderr, "[smoke] OK signals_count=%zu max_timestamp=%u\n",
        v->signals_count, (unsigned)max_ts);
    fprintf(stderr, "[smoke] date=%.64s version=%.64s\n", v->date, v->version);

    int n = 0;
    const int cap = 200;
    for (signal_node_t* node = v->signals_head; node && n < cap; node = node->next, ++n) {
        const signal_t* s = &node->signal;
        printf("%d\t%s\t%s\t%zu\t%d\t%zu\n",
            n,
            s->name,
            s->full_name,
            s->size,
            (int)s->fst_var_type,
            (size_t)s->changes_count);
    }
    if (v->signals_count > (size_t)cap)
        fprintf(stderr, "[smoke] (printed first %d signals, total %zu)\n", cap, v->signals_count);

    size_t total_ch = 0;
    for (signal_node_t* node = v->signals_head; node; node = node->next)
        total_ch += node->signal.changes_count;
    fprintf(stderr, "[smoke] sum(changes_count)=%zu\n", total_ch);

    vcd_free(v);
    fprintf(stderr, "[smoke] done.\n");
    return 0;
}
