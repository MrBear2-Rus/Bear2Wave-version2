/*
 * Build (MinGW): from repo root
 *   gcc -O2 -o tools/write_sample_fst.exe tools/write_sample_fst.c ^
 *     third_party/libfst/src/fstapi.c third_party/libfst/src/fastlz.c third_party/libfst/src/lz4.c ^
 *     -I third_party/libfst/src -D_GNU_SOURCE -DHAVE_LIBPTHREAD=0 -DFST_DO_MISALIGNED_OPS -lz
 */
#include "fstapi.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *path = "test_sample.fst";
    fstWriterContext *w = fstWriterCreate(path, 1);
    if (!w) {
        fprintf(stderr, "fstWriterCreate failed\n");
        return 1;
    }

    fstWriterSetDate(w, "Tue May  5 12:00:00 2026");
    fstWriterSetVersion(w, "Bear2Wave sample generator");
    fstWriterSetTimescale(w, -9); /* 1 ns time unit */

    fstWriterSetScope(w, FST_ST_VCD_MODULE, "TOP", "");

    fstHandle clk = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "clk", 0);
    fstHandle din = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "din", 0);
    fstHandle dout = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "dout", 0);

    fstWriterSetUpscope(w);

    /* Simple clock-ish pattern over 0..40 ns */
    fstWriterEmitTimeChange(w, 0);
    fstWriterEmitValueChange(w, clk, "0");
    fstWriterEmitValueChange(w, din, "0");
    fstWriterEmitValueChange(w, dout, "0");

    fstWriterEmitTimeChange(w, 5);
    fstWriterEmitValueChange(w, clk, "1");
    fstWriterEmitValueChange(w, din, "1");

    fstWriterEmitTimeChange(w, 10);
    fstWriterEmitValueChange(w, clk, "0");

    fstWriterEmitTimeChange(w, 15);
    fstWriterEmitValueChange(w, clk, "1");
    fstWriterEmitValueChange(w, dout, "1");

    fstWriterEmitTimeChange(w, 20);
    fstWriterEmitValueChange(w, clk, "0");
    fstWriterEmitValueChange(w, din, "0");

    fstWriterEmitTimeChange(w, 40);

    fstWriterClose(w);
    printf("Wrote %s\n", path);
    return 0;
}
