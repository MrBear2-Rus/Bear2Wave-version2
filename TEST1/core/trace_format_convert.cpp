#include "core/trace_format_convert.h"

#include "core/trace_external_convert.h"
#include "core/trace_vc.h"
#include "core/vcd_fst_writer.h"
#include "trace_loader.h"
#include "vcd.h"

#include "fstapi.h"

#ifdef BEAR2WAVE_WITH_LXT2
#include "lxt2_write.h"
#include "lxt_write.h"
#endif

#ifdef BEAR2WAVE_WITH_VZT
#include "vzt_write.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ConvertEvent {
    uint64_t t = 0;
    signal_t* sig = nullptr;
    char value[VCD_SIGNAL_SIZE] {};
};

static void set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
}

static std::string ext_lower(const fs::path& p)
{
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.')
        ext.erase(ext.begin());
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

static const char* signal_full_name(const signal_t* sig)
{
    if (!sig)
        return "sig";
    if (sig->full_name[0])
        return sig->full_name;
    if (sig->module_path[0] && sig->name[0]) {
        static thread_local char buf[VCD_SIGNAL_SIZE * 2];
        snprintf(buf, sizeof(buf), "%s.%s", sig->module_path, sig->name);
        return buf;
    }
    return sig->name[0] ? sig->name : "sig";
}

static int fst_timescale_exp(const vcd_t* vcd)
{
    if (!vcd)
        return -9;
    const char* u = vcd->timescale.unit;
    if (!u || !u[0])
        return -9;
    if (!strcmp(u, "s"))
        return 0;
    if (!strcmp(u, "ms"))
        return -3;
    if (!strcmp(u, "us"))
        return -6;
    if (!strcmp(u, "ns"))
        return -9;
    if (!strcmp(u, "ps"))
        return -12;
    if (!strcmp(u, "fs"))
        return -15;
    return -9;
}

static int lxt_timescale_exp(const vcd_t* vcd)
{
    const int e = fst_timescale_exp(vcd);
    return -e;
}

static std::vector<signal_t*> collect_export_signals(vcd_t* vcd)
{
    std::vector<signal_t*> out;
    if (!vcd)
        return out;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
        signal_t* sig = &n->signal;
        if (sig->trace_alias_source)
            continue;
        out.push_back(sig);
    }
    return out;
}

static int ensure_all_loaded(vcd_t* vcd, char* err_buf, size_t err_buf_len)
{
    if (!vcd)
        return -1;
    std::vector<signal_t*> sigs = collect_export_signals(vcd);
    if (sigs.empty()) {
        set_err(err_buf, err_buf_len, "no signals in source trace");
        return -1;
    }
    if (trace_uses_lazy_backend(vcd)) {
        const int rc = trace_load_signals(
            vcd, sigs.data(), sigs.size(), 0, TRACE_LOAD_T1_FULL);
        if (rc != 0) {
            set_err(err_buf, err_buf_len,
                rc == 1 ? "conversion cancelled while loading source"
                        : "failed to load all signal data from source");
            return -1;
        }
    }
    for (signal_t* sig : sigs)
        vcd_ensure_signal_sorted(sig);
    return 0;
}

static void collect_events(vcd_t* vcd, std::vector<ConvertEvent>& events)
{
    events.clear();
    for (signal_t* sig : collect_export_signals(vcd)) {
        const size_t n = trace_vc_count(sig);
        for (size_t i = 0; i < n; ++i) {
            ConvertEvent ev;
            ev.t = trace_vc_timestamp(sig, i);
            ev.sig = sig;
            trace_vc_format_value(sig, i, ev.value, sizeof(ev.value));
            events.push_back(ev);
        }
    }
    std::sort(events.begin(), events.end(), [](const ConvertEvent& a, const ConvertEvent& b) {
        if (a.t != b.t)
            return a.t < b.t;
        return a.sig < b.sig;
    });
}

static void split_module_path(const std::string& path, std::vector<std::string>& parts)
{
    parts.clear();
    size_t b = 0;
    while (b <= path.size()) {
        const size_t e = path.find('.', b);
        const std::string part = path.substr(b, e == std::string::npos ? std::string::npos : e - b);
        if (!part.empty())
            parts.push_back(part);
        if (e == std::string::npos)
            break;
        b = e + 1;
    }
    if (parts.empty())
        parts.push_back("TOP");
}

static void fst_goto_scope(fstWriterContext* w, const std::string& target, std::vector<std::string>* cur_parts)
{
    if (!w || !cur_parts)
        return;
    std::vector<std::string> target_parts;
    split_module_path(target.empty() ? "TOP" : target, target_parts);

    size_t common = 0;
    while (common < cur_parts->size() && common < target_parts.size()
           && (*cur_parts)[common] == target_parts[common])
        ++common;

    while (cur_parts->size() > common) {
        fstWriterSetUpscope(w);
        cur_parts->pop_back();
    }
    for (size_t i = common; i < target_parts.size(); ++i) {
        fstWriterSetScope(w, FST_ST_VCD_MODULE, target_parts[i].c_str(), "");
        cur_parts->push_back(target_parts[i]);
    }
}

static int write_fst(const char* dst_path, vcd_t* vcd, const std::vector<ConvertEvent>& events, char* err_buf, size_t err_buf_len)
{
    fstWriterContext* w = fstWriterCreate(dst_path, 1);
    if (!w) {
        set_err(err_buf, err_buf_len, "fstWriterCreate failed");
        return -1;
    }

    if (vcd->date[0])
        fstWriterSetDate(w, vcd->date);
    if (vcd->version[0])
        fstWriterSetVersion(w, vcd->version);
    fstWriterSetTimescale(w, fst_timescale_exp(vcd));

    std::vector<signal_t*> sigs = collect_export_signals(vcd);
    std::sort(sigs.begin(), sigs.end(), [](signal_t* a, signal_t* b) {
        const int cmp = strcmp(a->module_path, b->module_path);
        if (cmp != 0)
            return cmp < 0;
        return strcmp(a->name, b->name) < 0;
    });

    std::map<signal_t*, fstHandle> handles;
    std::vector<std::string> cur_scope_parts;

    for (signal_t* sig : sigs) {
        fst_goto_scope(w, sig->module_path, &cur_scope_parts);
        const unsigned width = static_cast<unsigned>(std::max<size_t>(1, sig->size));
        fstHandle h = fstWriterCreateVar(
            w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, width, sig->name, 0);
        if (h == 0) {
            fstWriterClose(w);
            set_err(err_buf, err_buf_len, "fstWriterCreateVar failed");
            return -1;
        }
        handles[sig] = h;
    }

    uint64_t cur_t = UINT64_MAX;
    for (const ConvertEvent& ev : events) {
        if (ev.t != cur_t) {
            fstWriterEmitTimeChange(w, ev.t);
            cur_t = ev.t;
        }
        const auto it = handles.find(ev.sig);
        if (it != handles.end())
            fstWriterEmitValueChange(w, it->second, ev.value);
    }

    fstWriterClose(w);
    return 0;
}

#ifdef BEAR2WAVE_WITH_LXT2
static int write_lxt2_family(
    const char* dst_path,
    vcd_t* vcd,
    const std::vector<ConvertEvent>& events,
    bool lxt_v1,
    char* err_buf,
    size_t err_buf_len)
{
    std::map<signal_t*, struct lt_symbol*> lxt1_symbols;

    if (lxt_v1) {
        struct lt_trace* lt = lt_init(dst_path);
        if (!lt) {
            set_err(err_buf, err_buf_len, "lt_init failed");
            return -1;
        }
        lt_set_timescale(lt, lxt_timescale_exp(vcd));
        lt_symbol_bracket_stripping(lt, 1);

        for (signal_t* sig : collect_export_signals(vcd)) {
            const int msb = static_cast<int>(std::max<size_t>(1, sig->size)) - 1;
            struct lt_symbol* sym = lt_symbol_add(
                lt, signal_full_name(sig), 1, msb, 0, LT_SYM_F_BITS);
            if (!sym) {
                lt_close(lt);
                set_err(err_buf, err_buf_len, "lt_symbol_add failed");
                return -1;
            }
            lxt1_symbols[sig] = sym;
        }

        uint64_t cur_t = UINT64_MAX;
        for (const ConvertEvent& ev : events) {
            if (ev.t != cur_t) {
                if (!lt_set_time64(lt, static_cast<lxttime_t>(ev.t))) {
                    lt_close(lt);
                    set_err(err_buf, err_buf_len, "lt_set_time64 failed");
                    return -1;
                }
                cur_t = ev.t;
            }
            const auto it = lxt1_symbols.find(ev.sig);
            if (it != lxt1_symbols.end()) {
                if (!lt_emit_value_bit_string(lt, it->second, 0, const_cast<char*>(ev.value))) {
                    lt_close(lt);
                    set_err(err_buf, err_buf_len, "lt_emit_value_bit_string failed");
                    return -1;
                }
            }
        }
        lt_close(lt);
        return 0;
    }

    struct lxt2_wr_trace* lt = lxt2_wr_init(dst_path);
    if (!lt) {
        set_err(err_buf, err_buf_len, "lxt2_wr_init failed");
        return -1;
    }
    lxt2_wr_set_timescale(lt, lxt_timescale_exp(vcd));
    lxt2_wr_symbol_bracket_stripping(lt, 1);

    std::map<signal_t*, struct lxt2_wr_symbol*> lxt2_symbols;
    for (signal_t* sig : collect_export_signals(vcd)) {
        const int msb = static_cast<int>(std::max<size_t>(1, sig->size)) - 1;
        struct lxt2_wr_symbol* sym = lxt2_wr_symbol_add(
            lt, signal_full_name(sig), 1, msb, 0, LXT2_WR_SYM_F_WIRE);
        if (!sym) {
            lxt2_wr_close(lt);
            set_err(err_buf, err_buf_len, "lxt2_wr_symbol_add failed");
            return -1;
        }
        lxt2_symbols[sig] = sym;
    }

    uint64_t cur_t = UINT64_MAX;
    for (const ConvertEvent& ev : events) {
        if (ev.t != cur_t) {
            if (!lxt2_wr_set_time64(lt, static_cast<lxttime_t>(ev.t))) {
                lxt2_wr_close(lt);
                set_err(err_buf, err_buf_len, "lxt2_wr_set_time64 failed");
                return -1;
            }
            cur_t = ev.t;
        }
        const auto it = lxt2_symbols.find(ev.sig);
        if (it != lxt2_symbols.end())
            lxt2_wr_emit_value_bit_string(lt, it->second, 0, const_cast<char*>(ev.value));
    }
    lxt2_wr_flush(lt);
    lxt2_wr_close(lt);
    return 0;
}
#endif

#ifdef BEAR2WAVE_WITH_VZT
static int write_vzt(const char* dst_path, vcd_t* vcd, const std::vector<ConvertEvent>& events, char* err_buf, size_t err_buf_len)
{
    struct vzt_wr_trace* lt = vzt_wr_init(dst_path);
    if (!lt) {
        set_err(err_buf, err_buf_len, "vzt_wr_init failed");
        return -1;
    }
    vzt_wr_set_compression_type(lt, VZT_WR_IS_GZ);
    vzt_wr_set_timescale(lt, lxt_timescale_exp(vcd));
    vzt_wr_symbol_bracket_stripping(lt, 1);

    std::map<signal_t*, struct vzt_wr_symbol*> symbols;
    for (signal_t* sig : collect_export_signals(vcd)) {
        const int msb = static_cast<int>(std::max<size_t>(1, sig->size)) - 1;
        struct vzt_wr_symbol* sym = vzt_wr_symbol_add(
            lt, signal_full_name(sig), 1, msb, 0, VZT_WR_SYM_F_WIRE);
        if (!sym) {
            vzt_wr_close(lt);
            set_err(err_buf, err_buf_len, "vzt_wr_symbol_add failed");
            return -1;
        }
        symbols[sig] = sym;
    }

    uint64_t cur_t = UINT64_MAX;
    for (const ConvertEvent& ev : events) {
        if (ev.t != cur_t) {
            if (!vzt_wr_set_time64(lt, static_cast<vzttime_t>(ev.t))) {
                vzt_wr_close(lt);
                set_err(err_buf, err_buf_len, "vzt_wr_set_time64 failed");
                return -1;
            }
            cur_t = ev.t;
        }
        const auto it = symbols.find(ev.sig);
        if (it != symbols.end())
            vzt_wr_emit_value_bit_string(lt, it->second, 0, const_cast<char*>(ev.value));
    }
    vzt_wr_flush(lt);
    vzt_wr_close(lt);
    return 0;
}
#endif

static std::string make_vcd_code(size_t idx)
{
    return "b2w" + std::to_string(idx);
}

static int write_vcd(const char* dst_path, vcd_t* vcd, const std::vector<ConvertEvent>& events, char* err_buf, size_t err_buf_len)
{
    FILE* f = fopen(dst_path, "wb");
    if (!f) {
        set_err(err_buf, err_buf_len, "failed to open output VCD");
        return -1;
    }

    fprintf(f, "$date\n    Bear2Wave convert\n$end\n");
    if (vcd->version[0])
        fprintf(f, "$version\n    %s\n$end\n", vcd->version);
    else
        fprintf(f, "$version\n    Bear2Wave trace_format_convert\n$end\n");

    const char* unit = vcd->timescale.unit[0] ? vcd->timescale.unit : "ns";
    fprintf(f, "$timescale 1 %s $end\n", unit);

    std::vector<signal_t*> sigs = collect_export_signals(vcd);
    std::sort(sigs.begin(), sigs.end(), [](signal_t* a, signal_t* b) {
        const int cmp = strcmp(a->module_path, b->module_path);
        if (cmp != 0)
            return cmp < 0;
        return strcmp(a->name, b->name) < 0;
    });

    std::map<signal_t*, std::string> codes;
    std::vector<std::string> cur_parts;
    for (size_t i = 0; i < sigs.size(); ++i) {
        signal_t* sig = sigs[i];
        std::vector<std::string> target_parts;
        split_module_path(sig->module_path[0] ? sig->module_path : "TOP", target_parts);

        size_t common = 0;
        while (common < cur_parts.size() && common < target_parts.size()
               && cur_parts[common] == target_parts[common])
            ++common;
        while (cur_parts.size() > common) {
            fprintf(f, "$upscope $end\n");
            cur_parts.pop_back();
        }
        for (size_t j = common; j < target_parts.size(); ++j) {
            fprintf(f, "$scope module %s $end\n", target_parts[j].c_str());
            cur_parts.push_back(target_parts[j]);
        }

        const std::string code = make_vcd_code(i);
        codes[sig] = code;
        const unsigned width = static_cast<unsigned>(std::max<size_t>(1, sig->size));
        fprintf(f, "$var wire %u %s %s $end\n", width, code.c_str(), sig->name);
    }
    while (!cur_parts.empty()) {
        fprintf(f, "$upscope $end\n");
        cur_parts.pop_back();
    }
    fprintf(f, "$enddefinitions $end\n");

    uint64_t cur_t = UINT64_MAX;
    for (const ConvertEvent& ev : events) {
        if (ev.t != cur_t) {
            fprintf(f, "#%llu\n", static_cast<unsigned long long>(ev.t));
            cur_t = ev.t;
        }
        const auto it = codes.find(ev.sig);
        if (it == codes.end())
            continue;
        const unsigned width = static_cast<unsigned>(std::max<size_t>(1, ev.sig->size));
        if (width <= 1) {
            const char ch = ev.value[0] ? ev.value[0] : '0';
            fprintf(f, "%c%s\n", ch, it->second.c_str());
        } else {
            fprintf(f, "b%s %s\n", ev.value, it->second.c_str());
        }
    }

    fclose(f);
    return 0;
}

} // namespace

TraceConvertTarget trace_convert_target_from_extension(const char* ext)
{
    if (!ext || !ext[0])
        return TraceConvertTarget::Unknown;
    if (!strcmp(ext, "vcd") || !strcmp(ext, "evcd"))
        return TraceConvertTarget::Vcd;
    if (!strcmp(ext, "fst") || !strcmp(ext, "fzt"))
        return TraceConvertTarget::Fst;
    if (!strcmp(ext, "lxt"))
        return TraceConvertTarget::Lxt;
    if (!strcmp(ext, "lxt2"))
        return TraceConvertTarget::Lxt2;
    if (!strcmp(ext, "vzt"))
        return TraceConvertTarget::Vzt;
    if (trace_external_kind_for_extension(ext) != TraceExternalKind::None)
        return TraceConvertTarget::Unknown;
    return TraceConvertTarget::Unknown;
}

int trace_convert_target_available(TraceConvertTarget target)
{
    switch (target) {
    case TraceConvertTarget::Vcd:
    case TraceConvertTarget::Fst:
        return 1;
    case TraceConvertTarget::Lxt:
    case TraceConvertTarget::Lxt2:
#ifdef BEAR2WAVE_WITH_LXT2
        return 1;
#else
        return 0;
#endif
    case TraceConvertTarget::Vzt:
#ifdef BEAR2WAVE_WITH_VZT
        return 1;
#else
        return 0;
#endif
    default:
        return 0;
    }
}

const char* trace_convert_target_label(TraceConvertTarget target)
{
    switch (target) {
    case TraceConvertTarget::Vcd:
        return "VCD";
    case TraceConvertTarget::Fst:
        return "FST";
    case TraceConvertTarget::Lxt:
        return "LXT";
    case TraceConvertTarget::Lxt2:
        return "LXT2";
    case TraceConvertTarget::Vzt:
        return "VZT";
    default:
        return "unknown";
    }
}

void trace_convert_print_capabilities(FILE* out)
{
    if (!out)
        return;
    fprintf(out, "  convert targets : vcd fst");
#ifdef BEAR2WAVE_WITH_LXT2
    fprintf(out, " lxt lxt2");
#endif
#ifdef BEAR2WAVE_WITH_VZT
    fprintf(out, " vzt");
#endif
    fprintf(out, "\n");
    fprintf(out, "  convert sources : all trace_loader formats (vpd/wlf/fsdb/shm/aet2 via E4/E5)\n");
}

int trace_convert_path(
    const char* src_path,
    const char* dst_path,
    char* err_buf,
    size_t err_buf_len)
{
    if (!src_path || !src_path[0] || !dst_path || !dst_path[0]) {
        set_err(err_buf, err_buf_len, "invalid convert paths");
        return -1;
    }

    const std::string dst_ext = ext_lower(fs::path(dst_path));
    const TraceConvertTarget target = trace_convert_target_from_extension(dst_ext.c_str());
    if (target == TraceConvertTarget::Unknown) {
        if (trace_external_kind_for_extension(dst_ext.c_str()) != TraceExternalKind::None) {
            set_err(err_buf, err_buf_len,
                "cannot write VPD/WLF/FSDB (import only via external tools); pick vcd/fst/lxt/lxt2/vzt");
        } else {
            set_err(err_buf, err_buf_len, "unsupported convert target extension");
        }
        return -1;
    }
    if (!trace_convert_target_available(target)) {
        set_err(err_buf, err_buf_len,
            "convert target not available in this build (enable BEAR2WAVE_WITH_LXT2 / BEAR2WAVE_WITH_VZT)");
        return -2;
    }

    const std::string src_ext = ext_lower(fs::path(src_path));
    if (target == TraceConvertTarget::Fst && (src_ext == "vcd" || src_ext == "evcd"))
        return vcd_stream_write_fst(src_path, dst_path, err_buf, err_buf_len);

    vcd_t* vcd = trace_load_from_path(src_path, err_buf, err_buf_len);
    if (!vcd) {
        if (err_buf && err_buf_len > 0 && err_buf[0] == '\0')
            set_err(err_buf, err_buf_len, "failed to load source trace");
        return -1;
    }

    if (ensure_all_loaded(vcd, err_buf, err_buf_len) != 0) {
        vcd_free(vcd);
        return -1;
    }

    std::vector<ConvertEvent> events;
    collect_events(vcd, events);

    int rc = -1;
    switch (target) {
    case TraceConvertTarget::Vcd:
        rc = write_vcd(dst_path, vcd, events, err_buf, err_buf_len);
        break;
    case TraceConvertTarget::Fst:
        rc = write_fst(dst_path, vcd, events, err_buf, err_buf_len);
        break;
    case TraceConvertTarget::Lxt:
#ifdef BEAR2WAVE_WITH_LXT2
        rc = write_lxt2_family(dst_path, vcd, events, true, err_buf, err_buf_len);
#else
        rc = -2;
#endif
        break;
    case TraceConvertTarget::Lxt2:
#ifdef BEAR2WAVE_WITH_LXT2
        rc = write_lxt2_family(dst_path, vcd, events, false, err_buf, err_buf_len);
#else
        rc = -2;
#endif
        break;
    case TraceConvertTarget::Vzt:
#ifdef BEAR2WAVE_WITH_VZT
        rc = write_vzt(dst_path, vcd, events, err_buf, err_buf_len);
#else
        rc = -2;
#endif
        break;
    default:
        set_err(err_buf, err_buf_len, "unsupported convert target");
        rc = -1;
        break;
    }

    vcd_free(vcd);
    return rc;
}
