#include "ghw_loader.h"

#include "core/trace_load_gaps.h"
#include "core/trace_var_types.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef BEAR2WAVE_WITH_GHW
extern "C" {
#include "libghw.h"
}
#endif

static void append_signal_node(vcd_t* vcd, signal_node_t* new_node)
{
    if (!vcd->signals_head)
        vcd->signals_head = new_node;
    else {
        signal_node_t* p = vcd->signals_head;
        while (p->next)
            p = p->next;
        p->next = new_node;
    }
    vcd->signals_count++;
}

#ifdef BEAR2WAVE_WITH_GHW

static std::string join_path(const std::string& prefix, const char* name)
{
    if (!name || !name[0])
        return prefix;
    if (prefix.empty())
        return name;
    return prefix + "." + name;
}

static std::string join_path(const std::string& prefix, const std::string& name)
{
    return join_path(prefix, name.c_str());
}

static void ghw_value_to_string(struct ghw_handler* h, struct ghw_sig* sig, char* buf, size_t len)
{
    if (!buf || len == 0)
        return;
    buf[0] = '\0';
    if (!sig || !sig->type || !sig->val) {
        strncpy(buf, "?", len - 1);
        buf[len - 1] = '\0';
        return;
    }
    ghw_get_value(buf, (int)len, sig->val, sig->type);
}

struct GhwTraceSession {
    std::string path;
    std::vector<signal_t*> by_idx;
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    struct ghw_handler handle {};
    bool handle_open = false;
};

static bool ghw_signal_range_covers(const signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return false;
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return sig->trace_loaded_t1 == TRACE_LOAD_T1_FULL;
    return sig->trace_loaded_t0 <= t0 && sig->trace_loaded_t1 >= t1;
}

static bool ghw_time_in_range(uint64_t ts, uint64_t t0, uint64_t t1)
{
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return true;
    return ts >= t0 && ts <= t1;
}

static unsigned ghw_parse_signal_index(const char* signal_id)
{
    if (!signal_id)
        return 0;
    const char* p = signal_id;
    if (strncmp(p, "ghw", 3) == 0)
        p += 3;
    return static_cast<unsigned>(std::strtoul(p, nullptr, 10));
}

static int ghw_scan_value_changes(
    GhwTraceSession* sess,
    const std::unordered_set<unsigned>& wanted,
    uint64_t t0,
    uint64_t t1,
    uint64_t* out_max_time)
{
    if (!sess || !sess->handle_open)
        return -1;

    struct ghw_handler* handle = &sess->handle;
    if (ghw_read_dump(handle) < 0)
        return -1;

    const std::vector<signal_t*>& by_idx = sess->by_idx;
    std::vector<std::string> last_val(handle->nbr_sigs);
    char buf[512];
    uint64_t file_max = 0;

    int* list = static_cast<int*>(calloc(handle->nbr_sigs + 2, sizeof(int)));
    if (!list)
        return -1;

    auto emit_for_index = [&](unsigned i, timestamp_t ts) {
        if (!wanted.count(i))
            return;
        signal_t* s = (i < by_idx.size()) ? by_idx[i] : nullptr;
        if (!s)
            return;
        if (!ghw_time_in_range(static_cast<uint64_t>(ts), t0, t1))
            return;
        ghw_value_to_string(handle, &handle->sigs[i], buf, sizeof(buf));
        const std::string v(buf);
        if (i < last_val.size() && last_val[i] == v)
            return;
        if (i >= last_val.size())
            last_val.resize(i + 1);
        last_val[i] = v;
        vcd_signal_append_change_lazy(s, ts, v.c_str());
        if (static_cast<uint64_t>(ts) > file_max)
            file_max = static_cast<uint64_t>(ts);
    };

    while (true) {
        const enum ghw_res res = ghw_read_sm_hdr(handle, list);
        if (res == ghw_res_error || res == ghw_res_eof)
            break;

        if (res == ghw_res_snapshot) {
            const timestamp_t ts = static_cast<timestamp_t>(handle->snap_time);
            for (unsigned i = 1; i < handle->nbr_sigs; ++i)
                emit_for_index(i, ts);
        } else if (res == ghw_res_cycle) {
            auto emit_cycle = [&]() {
                const timestamp_t ts = static_cast<timestamp_t>(handle->snap_time);
                for (int li = 0; list[li] != 0; ++li)
                    emit_for_index(static_cast<unsigned>(list[li]), ts);
            };
            emit_cycle();
            while (ghw_read_cycle_next(handle) == 1) {
                if (ghw_read_cycle_cont(handle, list) < 0)
                    break;
                emit_cycle();
            }
            ghw_read_cycle_end(handle);
        }
    }

    free(list);
    if (out_max_time && file_max > *out_max_time)
        *out_max_time = file_max;
    return 0;
}

static GhwTraceSession* ghw_session_from_vcd(vcd_t* vcd)
{
    if (!vcd || vcd->trace_backend != VCD_TRACE_BACKEND_GHW_LAZY || !vcd->trace_session)
        return nullptr;
    return static_cast<GhwTraceSession*>(vcd->trace_session);
}

#endif /* BEAR2WAVE_WITH_GHW */

extern "C" void ghw_trace_session_destroy(void* session)
{
#ifdef BEAR2WAVE_WITH_GHW
    auto* sess = static_cast<GhwTraceSession*>(session);
    if (sess) {
        if (sess->handle_open)
            ghw_close(&sess->handle);
        delete sess;
    }
#else
    (void)session;
#endif
}

extern "C" vcd_t* ghw_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

#ifndef BEAR2WAVE_WITH_GHW
    fprintf(stderr, "[GHW] Not built with BEAR2WAVE_WITH_GHW. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    struct ghw_handler handle;
    memset(&handle, 0, sizeof(handle));
    handle.flag_verbose = 0;

    if (ghw_open(&handle, utf8_path) < 0) {
        fprintf(stderr, "[GHW] ghw_open failed: %s\n", utf8_path);
        return nullptr;
    }
    if (ghw_read_base(&handle) < 0 || !handle.hie) {
        fprintf(stderr, "[GHW] ghw_read_base failed: %s\n", utf8_path);
        ghw_close(&handle);
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        ghw_close(&handle);
        return nullptr;
    }
    strncpy(vcd->version, "Bear2Wave GHW loader", VCD_VERSION_SIZE - 1);
    strncpy(vcd->timescale.unit, "fs", VCD_TIME_UNIT_SIZE - 1);
    vcd->timescale.scale = 1;

    auto* sess = new GhwTraceSession();
    sess->path = utf8_path;
    sess->by_idx.assign(handle.nbr_sigs + 1, nullptr);

    auto add_sig = [&](unsigned idx, const std::string& full_name) -> signal_t* {
        if (idx == 0 || idx >= handle.nbr_sigs || !handle.sigs[idx].type)
            return nullptr;
        if (sess->by_idx[idx])
            return sess->by_idx[idx];

        signal_node_t* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node)
            return nullptr;
        signal_t* s = &node->signal;

        std::string short_name = full_name;
        const size_t dot = full_name.rfind('.');
        if (dot != std::string::npos)
            short_name = full_name.substr(dot + 1);

        strncpy(s->name, short_name.c_str(), VCD_NAME_SIZE - 1);
        s->name[VCD_NAME_SIZE - 1] = '\0';
        strncpy(s->full_name, full_name.c_str(), VCD_SIGNAL_SIZE - 1);
        s->full_name[VCD_SIGNAL_SIZE - 1] = '\0';
        strncpy(s->module_path, "$root", VCD_SIGNAL_SIZE - 1);
        s->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(s->signal_id, VCD_NAME_SIZE, "ghw%u", idx);
        s->signal_id[VCD_NAME_SIZE - 1] = '\0';

        union ghw_type* bt = ghw_get_base_type(handle.sigs[idx].type);
        s->size = 1;
        s->fst_var_type = BEAR2WAVE_VT_VCD_ONLY;
        if (bt && bt->kind == ghdl_rtik_type_f64) {
            s->fst_var_type = BEAR2WAVE_VT_REAL;
        } else if (bt && (bt->kind == ghdl_rtik_type_i32 || bt->kind == ghdl_rtik_type_p32)) {
            s->size = 32;
        } else if (bt && (bt->kind == ghdl_rtik_type_i64 || bt->kind == ghdl_rtik_type_p64)) {
            s->size = 64;
        }

        append_signal_node(vcd, node);
        sess->by_idx[idx] = s;
        return s;
    };

    std::function<void(struct ghw_hie*, std::string)> walk;
    walk = [&](struct ghw_hie* hie, std::string prefix) {
        if (!hie)
            return;
        switch (hie->kind) {
        case ghw_hie_design:
        case ghw_hie_block:
        case ghw_hie_instance:
        case ghw_hie_generate_for:
        case ghw_hie_generate_if:
        case ghw_hie_package: {
            std::string np = join_path(prefix, hie->name);
            for (struct ghw_hie* ch = hie->u.blk.child; ch; ch = ch->brother)
                walk(ch, np);
            break;
        }
        case ghw_hie_signal:
        case ghw_hie_port_in:
        case ghw_hie_port_out:
        case ghw_hie_port_inout:
        case ghw_hie_port_buffer:
        case ghw_hie_port_linkage: {
            std::string np = join_path(prefix, hie->name);
            unsigned int* ptr = hie->u.sig.sigs;
            if (!ptr || !hie->u.sig.type)
                break;

            std::function<void(const std::string&, union ghw_type*, unsigned int**)> walk_type;
            walk_type = [&](const std::string& pfx, union ghw_type* t, unsigned int** sp) {
                if (!t || !sp)
                    return;
                switch (t->kind) {
                case ghdl_rtik_type_b2:
                case ghdl_rtik_type_e8:
                case ghdl_rtik_type_i32:
                case ghdl_rtik_type_i64:
                case ghdl_rtik_type_f64:
                case ghdl_rtik_type_p32:
                case ghdl_rtik_type_p64:
                    if (**sp != GHW_NO_SIG)
                        add_sig(**sp, pfx);
                    ++(*sp);
                    break;
                case ghdl_rtik_subtype_array:
                case ghdl_rtik_subtype_array_ptr: {
                    int len = 1;
                    if (t->sa.rngs && t->sa.rngs[0])
                        len = ghw_get_range_length(t->sa.rngs[0]);
                    for (int i = 0; i < len; ++i) {
                        char ib[24];
                        snprintf(ib, sizeof(ib), "%d", i);
                        walk_type(join_path(pfx, ib), t->sa.base, sp);
                    }
                    break;
                }
                case ghdl_rtik_type_record:
                    for (unsigned f = 0; f < t->rec.nbr_fields; ++f)
                        walk_type(join_path(pfx, t->rec.els[f].name), t->rec.els[f].type, sp);
                    break;
                case ghdl_rtik_subtype_record:
                    for (int f = 0; f < t->sr.nbr_scalars; ++f)
                        walk_type(join_path(pfx, t->sr.els[f].name), t->sr.els[f].type, sp);
                    break;
                default:
                    break;
                }
            };
            walk_type(np, hie->u.sig.type, &ptr);
            break;
        }
        default:
            break;
        }
    };
    walk(handle.hie, "");

    if (vcd->signals_count == 0) {
        for (unsigned i = 1; i < handle.nbr_sigs; ++i) {
            char nm[VCD_SIGNAL_SIZE];
            snprintf(nm, sizeof(nm), "sig%u", i);
            add_sig(i, nm);
        }
    }

    sess->handle = handle;
    sess->handle_open = true;

    vcd->trace_session = sess;
    vcd->trace_backend = VCD_TRACE_BACKEND_GHW_LAZY;
    vcd->trace_max_timestamp = 1;
    return vcd;
#endif
}

extern "C" int ghw_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
#ifndef BEAR2WAVE_WITH_GHW
    (void)vcd;
    (void)sigs;
    (void)count;
    (void)t0;
    (void)t1;
    return -1;
#else
    GhwTraceSession* sess = ghw_session_from_vcd(vcd);
    if (!sess || !sigs || count == 0)
        return -1;

    uint64_t batch_t0 = UINT64_MAX;
    uint64_t batch_t1 = 0;
    std::vector<signal_t*> pending;

    for (size_t i = 0; i < count; ++i) {
        signal_t* sig = sigs[i];
        if (!sig || ghw_signal_range_covers(sig, t0, t1))
            continue;
        trace_compute_sig_gaps(sig, t0, t1, batch_t0, batch_t1);
        pending.push_back(sig);
    }

    if (pending.empty())
        return 0;

    if (batch_t0 == UINT64_MAX) {
        batch_t0 = t0;
        batch_t1 = t1;
    }

    std::unordered_set<unsigned> wanted;
    for (signal_t* sig : pending) {
        const unsigned idx = ghw_parse_signal_index(sig->signal_id);
        if (idx > 0 && idx < sess->by_idx.size())
            wanted.insert(idx);
    }

    if (wanted.empty())
        return 0;

    uint64_t file_max = vcd->trace_max_timestamp;
    if (ghw_scan_value_changes(sess, wanted, batch_t0, batch_t1, &file_max) < 0)
        return -1;

    if (file_max > vcd->trace_max_timestamp)
        vcd->trace_max_timestamp = file_max;

    for (signal_t* sig : pending) {
        vcd_ensure_signal_sorted(sig);
        trace_extend_loaded_span(sig, t0, t1);
        vcd_signal_shrink_to_fit(sig);
    }
    return 0;
#endif
}

extern "C" vcd_t* ghw_read_to_vcd(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

#ifndef BEAR2WAVE_WITH_GHW
    (void)utf8_path;
    fprintf(stderr, "[GHW] Not built with BEAR2WAVE_WITH_GHW. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    vcd_t* vcd = ghw_open_lazy(utf8_path);
    if (!vcd)
        return nullptr;

    std::vector<signal_t*> all;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        all.push_back(&n->signal);

    if (!all.empty() && ghw_load_signals(vcd, all.data(), all.size(), TRACE_LOAD_T0_FULL, TRACE_LOAD_T1_FULL) != 0) {
        vcd_free(vcd);
        return nullptr;
    }
    fprintf(stderr, "[GHW] loaded %s signals=%zu\n", utf8_path, vcd->signals_count);
    return vcd;
#endif
}
