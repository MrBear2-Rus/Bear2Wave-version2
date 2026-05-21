#include "vzt_loader.h"

#include "core/trace_load_gaps.h"
#include "core/trace_var_types.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef BEAR2WAVE_WITH_VZT
#include "vzt_read.h"
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

static void apply_vzt_timescale(struct vzt_rd_trace* lt, vcd_t* vcd)
{
    if (!lt || !vcd)
        return;
    const char ts = vzt_rd_get_timescale(lt);
    vcd->timescale.scale = 1;
    const char* u = "ns";
    if (ts >= 15)
        u = "fs";
    else if (ts >= 12)
        u = "ps";
    else if (ts >= 9)
        u = "ns";
    else if (ts >= 6)
        u = "us";
    else if (ts >= 3)
        u = "ms";
    else if (ts >= 0)
        u = "s";
    strncpy(vcd->timescale.unit, u, VCD_TIME_UNIT_SIZE - 1);
    vcd->timescale.unit[VCD_TIME_UNIT_SIZE - 1] = '\0';
}

#ifdef BEAR2WAVE_WITH_VZT

struct VztLoadCtx {
    std::vector<signal_t*> fac_map;
    uint64_t max_time = 0;
    std::atomic<bool>* cancel = nullptr;
    uint64_t cb_ticks = 0;
};

struct VztTraceSession {
    struct vzt_rd_trace* lt = nullptr;
    std::vector<signal_t*> fac_map;
    std::string path;
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    std::atomic<bool> cancel{false};
};

static void split_vzt_full_name(const char* full, char* mod_path, char* sig_name)
{
    if (!full || !full[0]) {
        strncpy(mod_path, "$root", VCD_SIGNAL_SIZE - 1);
        mod_path[VCD_SIGNAL_SIZE - 1] = '\0';
        strncpy(sig_name, "sig", VCD_NAME_SIZE - 1);
        sig_name[VCD_NAME_SIZE - 1] = '\0';
        return;
    }
    std::string s(full);
    const size_t dot = s.rfind('.');
    if (dot == std::string::npos) {
        strncpy(mod_path, "$root", VCD_SIGNAL_SIZE - 1);
        mod_path[VCD_SIGNAL_SIZE - 1] = '\0';
        strncpy(sig_name, s.c_str(), VCD_NAME_SIZE - 1);
        sig_name[VCD_NAME_SIZE - 1] = '\0';
        return;
    }
    std::string mod = s.substr(0, dot);
    std::string nm = s.substr(dot + 1);
    if (mod.empty())
        mod = "$root";
    strncpy(mod_path, mod.c_str(), VCD_SIGNAL_SIZE - 1);
    mod_path[VCD_SIGNAL_SIZE - 1] = '\0';
    strncpy(sig_name, nm.c_str(), VCD_NAME_SIZE - 1);
    sig_name[VCD_NAME_SIZE - 1] = '\0';
}

static void vzt_value_change_cb(struct vzt_rd_trace** lt, vztint64_t* time, vztint32_t* facidx, char** value)
{
    if (!lt || !*lt || !time || !facidx || !value || !*value)
        return;
    VztLoadCtx* ctx = static_cast<VztLoadCtx*>(vzt_rd_get_user_callback_data_pointer(*lt));
    if (ctx && ctx->cancel && (++ctx->cb_ticks & 0xFFFFu) == 0 && ctx->cancel->load(std::memory_order_relaxed))
        return;
    if (!ctx)
        return;
    if (*facidx < 0 || static_cast<size_t>(*facidx) >= ctx->fac_map.size())
        return;
    signal_t* sig = ctx->fac_map[static_cast<size_t>(*facidx)];
    if (!sig)
        return;
    const uint64_t t = static_cast<uint64_t>(*time);
    if (t > ctx->max_time)
        ctx->max_time = t;
    vcd_signal_append_change_lazy(sig, static_cast<timestamp_t>(t), *value);
}

#endif /* BEAR2WAVE_WITH_VZT */

static int vzt_build_hierarchy(struct vzt_rd_trace* lt, vcd_t* vcd, std::vector<signal_t*>* fac_map)
{
    const vztint32_t numfacs = vzt_rd_get_num_facs(lt);
    fac_map->assign(static_cast<size_t>(numfacs > 0 ? numfacs : 0), nullptr);

    for (vztint32_t i = 0; i < numfacs; ++i) {
        if (vzt_rd_get_alias_root(lt, i) != i)
            continue;

        char* facname = vzt_rd_get_facname(lt, i);
        if (!facname)
            continue;

        signal_node_t* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node)
            return -1;
        signal_t* sig = &node->signal;

        char mod[VCD_SIGNAL_SIZE];
        char nm[VCD_NAME_SIZE];
        split_vzt_full_name(facname, mod, nm);
        strncpy(sig->name, nm, VCD_NAME_SIZE - 1);
        sig->name[VCD_NAME_SIZE - 1] = '\0';
        strncpy(sig->module_path, mod, VCD_SIGNAL_SIZE - 1);
        sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s", facname);
        sig->full_name[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->signal_id, VCD_NAME_SIZE, "vzt%d", (int)i);
        sig->signal_id[VCD_NAME_SIZE - 1] = '\0';

        const vztint32_t len = vzt_rd_get_fac_len(lt, i);
        sig->size = len > 0 ? static_cast<size_t>(len) : 1u;
        sig->fst_var_type = Bear2waveVarTypeFromVztFlags(vzt_rd_get_fac_flags(lt, i));

        (*fac_map)[static_cast<size_t>(i)] = sig;
        append_signal_node(vcd, node);
    }
    return 0;
}

static VztTraceSession* vzt_session_from_vcd(vcd_t* vcd)
{
    if (!vcd || vcd->trace_backend != VCD_TRACE_BACKEND_VZT_LAZY || !vcd->trace_session)
        return nullptr;
    return static_cast<VztTraceSession*>(vcd->trace_session);
}

static bool vzt_signal_range_covers(const signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return false;
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return sig->trace_loaded_t1 == TRACE_LOAD_T1_FULL;
    return sig->trace_loaded_t0 <= t0 && sig->trace_loaded_t1 >= t1;
}

static int vzt_iter_changes(VztTraceSession* sess, uint64_t t0, uint64_t t1)
{
    if (!sess || !sess->lt)
        return -1;
    sess->cancel.store(false, std::memory_order_relaxed);

    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        vzt_rd_unlimit_time_range(sess->lt);
    else
        vzt_rd_limit_time_range(sess->lt, static_cast<vztint64_t>(t0), static_cast<vztint64_t>(t1));

    VztLoadCtx ctx;
    ctx.fac_map = sess->fac_map;
    ctx.cancel = &sess->cancel;

    vzt_rd_set_max_block_mem_usage(sess->lt, (vztint64_t)64 * 1024 * 1024);
    const int rc = vzt_rd_iter_blocks(sess->lt, vzt_value_change_cb, &ctx);
    vzt_rd_unlimit_time_range(sess->lt);

    if (sess->cancel.load(std::memory_order_relaxed))
        return 1;
    return rc < 0 ? -1 : 0;
}

extern "C" void vzt_trace_session_destroy(void* session)
{
    auto* sess = static_cast<VztTraceSession*>(session);
    if (!sess)
        return;
    if (sess->lt)
        vzt_rd_close(sess->lt);
    delete sess;
}

extern "C" void vzt_loader_request_cancel(vcd_t* vcd)
{
    if (VztTraceSession* sess = vzt_session_from_vcd(vcd))
        sess->cancel.store(true, std::memory_order_relaxed);
}

extern "C" vcd_t* vzt_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

#ifndef BEAR2WAVE_WITH_VZT
    fprintf(stderr,
        "[VZT] Bear2Wave built without BEAR2WAVE_WITH_VZT. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    struct vzt_rd_trace* lt = vzt_rd_init(utf8_path);
    if (!lt) {
        fprintf(stderr, "[VZT] vzt_rd_init failed: %s\n", utf8_path);
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        vzt_rd_close(lt);
        return nullptr;
    }

    strncpy(vcd->version, "Bear2Wave VZT loader", VCD_VERSION_SIZE - 1);
    vcd->version[VCD_VERSION_SIZE - 1] = '\0';
    apply_vzt_timescale(lt, vcd);

    auto* sess = new VztTraceSession();
    sess->lt = lt;
    sess->path = utf8_path;
    sess->start_time = static_cast<uint64_t>(vzt_rd_get_start_time(lt));
    sess->end_time = static_cast<uint64_t>(vzt_rd_get_end_time(lt));

    if (vzt_build_hierarchy(lt, vcd, &sess->fac_map) < 0) {
        delete sess;
        vcd_free(vcd);
        return nullptr;
    }

    vcd->trace_session = sess;
    vcd->trace_backend = VCD_TRACE_BACKEND_VZT_LAZY;
    vcd->trace_max_timestamp = sess->end_time;
    return vcd;
#endif
}

extern "C" int vzt_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
#ifndef BEAR2WAVE_WITH_VZT
    (void)vcd;
    (void)sigs;
    (void)count;
    (void)t0;
    (void)t1;
    return -1;
#else
    VztTraceSession* sess = vzt_session_from_vcd(vcd);
    if (!sess || !sigs || count == 0)
        return -1;

    uint64_t batch_t0 = UINT64_MAX;
    uint64_t batch_t1 = 0;
    std::vector<signal_t*> pending;

    for (size_t i = 0; i < count; ++i) {
        signal_t* sig = sigs[i];
        if (!sig || vzt_signal_range_covers(sig, t0, t1))
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

    vzt_rd_clr_fac_process_mask_all(sess->lt);
    for (signal_t* sig : pending) {
        const char* idp = sig->signal_id;
        if (idp[0] == 'v' && idp[1] == 'z' && idp[2] == 't')
            idp += 3;
        const long fac = std::strtol(idp, nullptr, 10);
        if (fac < 0 || fac >= static_cast<long>(sess->fac_map.size()))
            continue;
        vzt_rd_set_fac_process_mask(sess->lt, static_cast<vztint32_t>(fac));
    }

    const int rc = vzt_iter_changes(sess, batch_t0, batch_t1);
    if (rc != 0)
        return rc;

    for (signal_t* sig : pending) {
        vcd_ensure_signal_sorted(sig);
        trace_extend_loaded_span(sig, t0, t1);
        vcd_signal_shrink_to_fit(sig);
    }
    return 0;
#endif
}

extern "C" vcd_t* vzt_read_to_vcd(const char* utf8_path)
{
#ifndef BEAR2WAVE_WITH_VZT
    (void)utf8_path;
    return nullptr;
#else
    vcd_t* vcd = vzt_open_lazy(utf8_path);
    if (!vcd)
        return nullptr;

    std::vector<signal_t*> all;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        all.push_back(&n->signal);

    if (!all.empty() && vzt_load_signals(vcd, all.data(), all.size(), TRACE_LOAD_T0_FULL, TRACE_LOAD_T1_FULL) != 0) {
        vcd_free(vcd);
        return nullptr;
    }
    return vcd;
#endif
}
