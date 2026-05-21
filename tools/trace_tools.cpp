/*
 * Bear2Wave trace format utilities: generate sample files + smoke-test loaders.
 *
 * Build: msbuild tools\TraceTools.vcxproj /p:Configuration=Debug /p:Platform=x64
 *
 * Usage:
 *   trace_tools.exe gen-all [output_dir]   default: tests\traces
 *   trace_tools.exe gen-vcd|gen-fst|gen-vzt|gen-lxt2|gen-ghw <path>
 *   trace_tools.exe test <path>
 *   trace_tools.exe test-all [dir]         default: tests\traces
 */
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "fstapi.h"
#include "fst_loader.h"
#include "trace_loader.h"
#include "vcd.h"

#ifdef BEAR2WAVE_WITH_VZT
#include "vzt_write.h"
#endif

#ifdef BEAR2WAVE_WITH_LXT2
#include "lxt2_write.h"
#endif

static std::string join_path(const char* dir, const char* name)
{
    if (!dir || !dir[0])
        return name ? name : "";
    std::string p(dir);
    if (p.back() != '\\' && p.back() != '/')
        p += '\\';
    p += name;
    return p;
}

static int write_sample_fst(const char* path)
{
    fstWriterContext* w = fstWriterCreate(path, 1);
    if (!w)
        return 1;

    fstWriterSetDate(w, "Bear2Wave trace_tools sample");
    fstWriterSetVersion(w, "trace_tools gen-fst");
    fstWriterSetTimescale(w, -9);

    fstWriterSetScope(w, FST_ST_VCD_MODULE, "TOP", "");
    fstHandle clk = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 1, "clk", 0);
    fstHandle din = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 8, "data", 0);
    fstWriterSetUpscope(w);

    fstWriterEmitTimeChange(w, 0);
    fstWriterEmitValueChange(w, clk, "0");
    fstWriterEmitValueChange(w, din, "00000000");

    fstWriterEmitTimeChange(w, 10);
    fstWriterEmitValueChange(w, clk, "1");
    fstWriterEmitValueChange(w, din, "10101010");

    fstWriterEmitTimeChange(w, 20);
    fstWriterEmitValueChange(w, clk, "0");

    fstWriterEmitTimeChange(w, 40);
    fstWriterClose(w);
    fprintf(stderr, "[gen-fst] wrote %s\n", path);
    return 0;
}

#ifdef BEAR2WAVE_WITH_VZT
static int write_sample_vzt(const char* path)
{
    struct vzt_wr_trace* lt = vzt_wr_init(path);
    if (!lt)
        return 1;

    vzt_wr_set_compression_type(lt, VZT_WR_IS_GZ);
    vzt_wr_set_timescale(lt, 9);
    vzt_wr_symbol_bracket_stripping(lt, 1);

    struct vzt_wr_symbol* clk = vzt_wr_symbol_add(lt, "TOP.clk", 1, 0, 0, VZT_WR_SYM_F_WIRE);
    struct vzt_wr_symbol* data = vzt_wr_symbol_add(lt, "TOP.data", 1, 7, 0, VZT_WR_SYM_F_WIRE);

    vzt_wr_set_time(lt, 0);
    vzt_wr_emit_value_bit_string(lt, clk, 0, const_cast<char*>("0"));
    vzt_wr_emit_value_bit_string(lt, data, 0, const_cast<char*>("00000000"));

    vzt_wr_set_time(lt, 10);
    vzt_wr_emit_value_bit_string(lt, clk, 0, const_cast<char*>("1"));
    vzt_wr_emit_value_bit_string(lt, data, 0, const_cast<char*>("10101010"));

    vzt_wr_set_time(lt, 20);
    vzt_wr_emit_value_bit_string(lt, clk, 0, const_cast<char*>("0"));

    vzt_wr_flush(lt);
    vzt_wr_close(lt);
    fprintf(stderr, "[gen-vzt] wrote %s\n", path);
    return 0;
}
#else
static int write_sample_vzt(const char* path)
{
    fprintf(stderr, "[gen-vzt] skipped (BEAR2WAVE_WITH_VZT not defined): %s\n", path);
    return 1;
}
#endif

#ifdef BEAR2WAVE_WITH_LXT2
static int write_sample_lxt2(const char* path)
{
    struct lxt2_wr_trace* lt = lxt2_wr_init(path);
    if (!lt)
        return 1;

    lxt2_wr_set_timescale(lt, 9);
    lxt2_wr_symbol_bracket_stripping(lt, 1);

    struct lxt2_wr_symbol* clk = lxt2_wr_symbol_add(lt, "TOP.clk", 1, 0, 0, LXT2_WR_SYM_F_WIRE);
    struct lxt2_wr_symbol* data = lxt2_wr_symbol_add(lt, "TOP.data", 1, 7, 0, LXT2_WR_SYM_F_WIRE);

    lxt2_wr_set_time(lt, 0);
    lxt2_wr_emit_value_bit_string(lt, clk, 0, const_cast<char*>("0"));
    lxt2_wr_emit_value_bit_string(lt, data, 0, const_cast<char*>("00000000"));

    lxt2_wr_set_time(lt, 10);
    lxt2_wr_emit_value_bit_string(lt, clk, 0, const_cast<char*>("1"));
    lxt2_wr_emit_value_bit_string(lt, data, 0, const_cast<char*>("10101010"));

    lxt2_wr_flush(lt);
    lxt2_wr_close(lt);
    fprintf(stderr, "[gen-lxt2] wrote %s\n", path);
    return 0;
}
#else
static int write_sample_lxt2(const char* path)
{
    fprintf(stderr, "[gen-lxt2] skipped (BEAR2WAVE_WITH_LXT2 not defined): %s\n", path);
    return 1;
}
#endif

static int write_sample_vcd(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return 1;
    static const char kVcd[] =
        "$date\n"
        "    Bear2Wave trace_tools\n"
        "$end\n"
        "$version\n"
        "    trace_tools gen-vcd\n"
        "$end\n"
        "$timescale 1ns $end\n"
        "$scope module TOP $end\n"
        "$var wire 1 ! clk $end\n"
        "$var wire 8 \" data[7:0] $end\n"
        "$upscope $end\n"
        "$enddefinitions $end\n"
        "#0\n"
        "0!\n"
        "b00000000 \"\n"
        "#10\n"
        "1!\n"
        "b10101010 \"\n"
        "#20\n"
        "0!\n"
        "#40\n";
    fwrite(kVcd, 1, sizeof(kVcd) - 1, f);
    fclose(f);
    fprintf(stderr, "[gen-vcd] wrote %s\n", path);
    return 0;
}

static int test_file(const char* path)
{
    char err[512] = {};
    fprintf(stderr, "[test] loading \"%s\" ...\n", path);
    vcd_t* v = trace_load_from_path(path, err, sizeof(err));
    if (!v) {
        fprintf(stderr, "[test] FAIL \"%s\": %s\n", path, err);
        return 1;
    }

    const timestamp_t max_ts = vcd_get_max_timestamp(v);
    size_t total_changes = 0;
    for (signal_node_t* n = v->signals_head; n; n = n->next)
        total_changes += n->signal.changes_count;

    fprintf(stderr,
        "[test] PASS \"%s\" ext=%s signals=%zu max_ts=%llu changes=%zu\n",
        path,
        trace_format_extension(path),
        v->signals_count,
        (unsigned long long)max_ts,
        total_changes);

    vcd_free(v);
    return 0;
}

static long long bench_ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
}

static int bench_file(const char* path, int load_count)
{
    char err[512] = {};
    const auto t_open0 = std::chrono::steady_clock::now();
    vcd_t* v = trace_load_from_path(path, err, sizeof(err));
    const auto t_open1 = std::chrono::steady_clock::now();
    if (!v) {
        fprintf(stderr, "[bench] FAIL open \"%s\": %s\n", path, err);
        return 1;
    }

    std::vector<signal_t*> batch;
    batch.reserve(static_cast<size_t>(load_count > 0 ? load_count : 3));
    for (signal_node_t* n = v->signals_head; n; n = n->next) {
        if (load_count > 0 && static_cast<int>(batch.size()) >= load_count)
            break;
        batch.push_back(&n->signal);
        if (load_count <= 0 && batch.size() >= 3)
            break;
    }

    uint64_t file_max = v->trace_max_timestamp;
    if (file_max == 0)
        file_max = vcd_get_max_timestamp(v);
    if (file_max == 0)
        file_max = 100;

    uint64_t lt0 = 0, lt1 = file_max;
    trace_compute_padded_range(0, file_max, file_max, 0.2, &lt0, &lt1);

    const auto t_load0 = std::chrono::steady_clock::now();
    int lrc = 0;
    if (!batch.empty() && trace_uses_lazy_backend(v))
        lrc = trace_load_signals(v, batch.data(), batch.size(), lt0, lt1);
    const auto t_load1 = std::chrono::steady_clock::now();

    size_t total_changes = 0;
    for (signal_node_t* n = v->signals_head; n; n = n->next)
        total_changes += n->signal.changes_count;

    fprintf(stderr,
        "[bench] \"%s\" lazy=%d open_ms=%lld load_%zu_sig_ms=%lld signals=%zu loaded_changes=%zu max_ts=%llu lrc=%d\n",
        path,
        trace_uses_lazy_backend(v),
        (long long)bench_ms(t_open0, t_open1),
        batch.size(),
        (long long)bench_ms(t_load0, t_load1),
        v->signals_count,
        total_changes,
        (unsigned long long)file_max,
        lrc);

    vcd_free(v);
    return lrc < 0 ? 1 : 0;
}

static int test_all_dir(const char* dir)
{
    static const char* kNames[] = {
        "bear2wave_sample.vcd",
        "bear2wave_sample.fst",
        "bear2wave_sample.vzt",
        "bear2wave_sample.lxt2",
        "bear2wave_sample.ghw",
        "bear2wave_gtkwave_basic.vcd",
        "bear2wave_gtkwave_basic.fst",
    };

    int fails = 0;
    int ran = 0;
    for (const char* name : kNames) {
        const std::string path = join_path(dir, name);
        FILE* probe = fopen(path.c_str(), "rb");
        if (!probe)
            continue;
        fclose(probe);
        ++ran;
        fails += test_file(path.c_str());
    }

    if (ran == 0) {
        fprintf(stderr, "[test-all] no sample files in \"%s\" — run gen-all first.\n", dir);
        return 1;
    }
    fprintf(stderr, "[test-all] %d file(s), %d failure(s)\n", ran, fails);
    return fails ? 1 : 0;
}

static int copy_file(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in)
        return 1;
    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 1;
    }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(in);
    fclose(out);
    fprintf(stderr, "[copy] %s -> %s\n", src, dst);
    return 0;
}

static int gen_all(const char* out_dir)
{
    int rc = 0;
    rc |= write_sample_vcd(join_path(out_dir, "bear2wave_sample.vcd").c_str());
    rc |= write_sample_fst(join_path(out_dir, "bear2wave_sample.fst").c_str());
    rc |= write_sample_vzt(join_path(out_dir, "bear2wave_sample.vzt").c_str());
    rc |= write_sample_lxt2(join_path(out_dir, "bear2wave_sample.lxt2").c_str());

    const char* repo = "..";
    copy_file(join_path(repo, "third_party\\gtkwave-src\\lib\\libgtkwave\\test\\files\\basic.ghw").c_str(),
        join_path(out_dir, "bear2wave_sample.ghw").c_str());
    copy_file(join_path(repo, "third_party\\gtkwave-src\\lib\\libgtkwave\\test\\files\\basic.vcd").c_str(),
        join_path(out_dir, "bear2wave_gtkwave_basic.vcd").c_str());
    copy_file(join_path(repo, "third_party\\gtkwave-src\\lib\\libgtkwave\\test\\files\\basic.fst").c_str(),
        join_path(out_dir, "bear2wave_gtkwave_basic.fst").c_str());
    copy_file(join_path(repo, "test2.vcd").c_str(),
        join_path(out_dir, "bear2wave_test2.vcd").c_str());

    return rc;
}

static void usage(const char* argv0)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s gen-all [out_dir]\n"
        "  %s gen-vcd|gen-fst|gen-vzt|gen-lxt2 <path>\n"
        "  %s test <path>\n"
        "  %s test-all [dir]\n"
        "  %s bench <path> [num_signals]\n",
        argv0,
        argv0,
        argv0,
        argv0,
        argv0);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char* cmd = argv[1];
    if (!strcmp(cmd, "gen-all")) {
        const char* dir = (argc >= 3) ? argv[2] : "tests\\traces";
        return gen_all(dir);
    }
    if (!strcmp(cmd, "test-all")) {
        const char* dir = (argc >= 3) ? argv[2] : "tests\\traces";
        return test_all_dir(dir);
    }
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }
    const char* path = argv[2];

    if (!strcmp(cmd, "gen-vcd"))
        return write_sample_vcd(path);
    if (!strcmp(cmd, "gen-fst"))
        return write_sample_fst(path);
    if (!strcmp(cmd, "gen-vzt"))
        return write_sample_vzt(path);
    if (!strcmp(cmd, "gen-lxt2"))
        return write_sample_lxt2(path);
    if (!strcmp(cmd, "test"))
        return test_file(path);
    if (!strcmp(cmd, "bench")) {
        int n = (argc >= 4) ? atoi(argv[3]) : 3;
        return bench_file(path, n);
    }

    usage(argv[0]);
    return 1;
}
