/*
 * Generate a large FST for Bear2Wave lazy-load / M2 testing.
 *
 * Build (MSBuild):
 *   msbuild tools/GenLargeFst.vcxproj /p:Configuration=Release /p:Platform=x64
 *
 * Usage:
 *   gen_large_fst.exe [-o path] [--size-mb N] [--modules M] [--signals-per-module S]
 *                     [--step-ns T] [--steps N]
 */
#include "fstapi.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_OUT "tests/traces/large_test.fst"
#define MAX_SIGNALS 8192
#define MAX_NAME 64

typedef struct {
    fstHandle handle;
    int is_bus;
    int bus_width;
    char value_buf[65];
} SigMeta;

static void usage(const char* argv0)
{
    fprintf(stderr,
        "Usage: %s [-o path] [--size-mb N] [--modules M] [--signals-per-module S]\n"
        "              [--step-ns T] [--steps N]\n",
        argv0);
}

static int parse_i(const char* s, int fallback)
{
    if (!s || !s[0])
        return fallback;
    return atoi(s);
}

static double parse_d(const char* s, double fallback)
{
    if (!s || !s[0])
        return fallback;
    return atof(s);
}

static void fill_bus_bits(char* out, int width, uint64_t step, int sig_index)
{
    const uint64_t mask = (width >= 64) ? ~0ULL : ((1ULL << width) - 1ULL);
    const uint64_t v = (step * 1103515245ULL + (uint64_t)sig_index * 12345ULL + 67890ULL) & mask;
    for (int i = width - 1; i >= 0; --i)
        out[width - 1 - i] = ((v >> (uint64_t)i) & 1ULL) ? '1' : '0';
    out[width] = '\0';
}

static char scalar_for_step(uint64_t step, int sig_index)
{
    const uint64_t v = step * 6364136223846793005ULL + (uint64_t)sig_index * 97ULL;
    return (char)('0' + (int)(v & 1ULL));
}

int main(int argc, char** argv)
{
    const char* out_path = DEFAULT_OUT;
    double size_mb = 100.0;
    int modules = 32;
    int signals_per_module = 32;
    int step_ns = 10;
    int steps_override = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            out_path = argv[++i];
        else if (!strcmp(argv[i], "--size-mb") && i + 1 < argc)
            size_mb = parse_d(argv[++i], size_mb);
        else if (!strcmp(argv[i], "--modules") && i + 1 < argc)
            modules = parse_i(argv[++i], modules);
        else if (!strcmp(argv[i], "--signals-per-module") && i + 1 < argc)
            signals_per_module = parse_i(argv[++i], signals_per_module);
        else if (!strcmp(argv[i], "--step-ns") && i + 1 < argc)
            step_ns = parse_i(argv[++i], step_ns);
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc)
            steps_override = parse_i(argv[++i], 0);
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (modules < 1 || signals_per_module < 1) {
        fprintf(stderr, "modules and signals-per-module must be >= 1\n");
        return 1;
    }

    const int num_signals = modules * signals_per_module;
    if (num_signals > MAX_SIGNALS) {
        fprintf(stderr, "Too many signals (%d > %d)\n", num_signals, MAX_SIGNALS);
        return 1;
    }

    const clock_t t0 = clock();
    fstWriterContext* w = fstWriterCreate(out_path, 1);
    if (!w) {
        fprintf(stderr, "fstWriterCreate failed for \"%s\"\n", out_path);
        return 1;
    }

    fstWriterSetDate(w, "Bear2Wave gen_large_fst");
    fstWriterSetVersion(w, "gen_large_fst 1.0");
    fstWriterSetTimescale(w, -9); /* 1 ns */

    SigMeta sigs[MAX_SIGNALS];
    int sig_idx = 0;
    for (int m = 0; m < modules; ++m) {
        char scope[MAX_NAME];
        snprintf(scope, sizeof(scope), "mod_%d", m);
        fstWriterSetScope(w, FST_ST_VCD_MODULE, scope, "");
        for (int s = 0; s < signals_per_module; ++s) {
            char name[MAX_NAME];
            const int is_bus = (s % 5 == 0);
            const int width = is_bus ? 32 : 1;
            if (is_bus)
                snprintf(name, sizeof(name), "bus_%d_%d", m, s);
            else
                snprintf(name, sizeof(name), "bit_%d_%d", m, s);

            sigs[sig_idx].handle = fstWriterCreateVar(
                w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, (uint32_t)width, name, 0);
            sigs[sig_idx].is_bus = is_bus;
            sigs[sig_idx].bus_width = width;
            sigs[sig_idx].value_buf[0] = '\0';
            ++sig_idx;
        }
        fstWriterSetUpscope(w);
    }

    /* FST is compressed; use a conservative step estimate then extend if needed. */
    int num_steps = steps_override;
    if (num_steps <= 0) {
        const double target_bytes = size_mb * 1024.0 * 1024.0;
        const double est_bytes_per_step = 220.0 * (double)num_signals;
        num_steps = (int)(target_bytes / est_bytes_per_step);
        if (num_steps < 1000)
            num_steps = 1000;
    }

    fprintf(stderr,
        "[gen_large_fst] path=\"%s\" signals=%d modules=%d step_ns=%d steps=%d\n",
        out_path,
        num_signals,
        modules,
        step_ns,
        num_steps);

    for (int step = 0; step <= num_steps; ++step) {
        const uint64_t ts = (uint64_t)step * (uint64_t)step_ns;
        fstWriterEmitTimeChange(w, ts);
        for (int i = 0; i < num_signals; ++i) {
            const char* val = NULL;
            char scalar_buf[2] = { scalar_for_step((uint64_t)step, i), '\0' };
            if (sigs[i].is_bus) {
                fill_bus_bits(sigs[i].value_buf, sigs[i].bus_width, (uint64_t)step, i);
                val = sigs[i].value_buf;
            } else {
                val = scalar_buf;
            }
            fstWriterEmitValueChange(w, sigs[i].handle, val);
        }
        if (step > 0 && step % 50000 == 0)
            fstWriterFlushContext(w);
    }

    fstWriterClose(w);

    FILE* check = fopen(out_path, "rb");
    long long fsize = -1;
    if (check) {
        fseek(check, 0, SEEK_END);
        fsize = ftell(check);
        fclose(check);
    }

    const double elapsed = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
    fprintf(stderr,
        "[gen_large_fst] wrote \"%s\"\n"
        "  size=%.2f MiB (%lld bytes)\n"
        "  max_time=%lld ns\n"
        "  elapsed=%.1fs\n",
        out_path,
        fsize > 0 ? (double)fsize / (1024.0 * 1024.0) : 0.0,
        fsize,
        (long long)num_steps * (long long)step_ns,
        elapsed);
    return 0;
}
