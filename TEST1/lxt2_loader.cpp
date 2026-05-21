#include "lxt2_loader.h"

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

#ifdef BEAR2WAVE_WITH_LXT2
#include "lxt2_read.h"
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

static void apply_lxt2_timescale(struct lxt2_rd_trace* lt, vcd_t* vcd)
{
    if (!lt || !vcd)
        return;
    const char ts = lxt2_rd_get_timescale(lt);
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

#ifdef BEAR2WAVE_WITH_LXT2

struct Lxt2LoadCtx {
    std::vector<signal_t*> fac_map;
    uint64_t max_time = 0;
    std::atomic<bool>* cancel = nullptr;
    uint64_t cb_ticks = 0;
};

struct Lxt2TraceSession {
    struct lxt2_rd_trace* lt = nullptr;
    std::vector<signal_t*> fac_map;
    std::string path;
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    std::atomic<bool> cancel{false};
};

static void split_lxt2_full_name(const char* full, char* mod_path, char* sig_name)
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

static void lxt2_value_change_cb(struct lxt2_rd_trace** lt, lxtint64_t* time, lxtint32_t* facidx, char** value)
{
    if (!lt || !*lt || !time || !facidx || !value || !*value)
        return;
    Lxt2LoadCtx* ctx = static_cast<Lxt2LoadCtx*>(lxt2_rd_get_user_callback_data_pointer(*lt));
    if (!ctx)
        return;
    if (ctx->cancel && (++ctx->cb_ticks & 0xFFFFu) == 0 && ctx->cancel->load(std::memory_order_relaxed))
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

#endif /* BEAR2WAVE_WITH_LXT2 */

static int lxt2_build_hierarchy(struct lxt2_rd_trace* lt, vcd_t* vcd, std::vector<signal_t*>* fac_map)
{
    const lxtint32_t numfacs = lxt2_rd_get_num_facs(lt);
    fac_map->assign(static_cast<size_t>(numfacs > 0 ? numfacs : 0), nullptr);

    for (lxtint32_t i = 0; i < numfacs; ++i) {
        if (lxt2_rd_get_alias_root(lt, i) != i)
            continue;

        char* facname = lxt2_rd_get_facname(lt, i);
        if (!facname)
            continue;

        signal_node_t* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node)
            return -1;
        signal_t* sig = &node->signal;

        char mod[VCD_SIGNAL_SIZE];
        char nm[VCD_NAME_SIZE];
        split_lxt2_full_name(facname, mod, nm);
        strncpy(sig->name, nm, VCD_NAME_SIZE - 1);
        sig->name[VCD_NAME_SIZE - 1] = '\0';
        strncpy(sig->module_path, mod, VCD_SIGNAL_SIZE - 1);
        sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s", facname);
        sig->full_name[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->signal_id, VCD_NAME_SIZE, "lxt2%d", (int)i);
        sig->signal_id[VCD_NAME_SIZE - 1] = '\0';

        const lxtint32_t len = lxt2_rd_get_fac_len(lt, i);
        sig->size = len > 0 ? static_cast<size_t>(len) : 1u;
        sig->fst_var_type = Bear2waveVarTypeFromVztFlags(lxt2_rd_get_fac_flags(lt, i));

        (*fac_map)[static_cast<size_t>(i)] = sig;
        append_signal_node(vcd, node);
    }
    return 0;
}

static Lxt2TraceSession* lxt2_session_from_vcd(vcd_t* vcd)
{
    if (!vcd || vcd->trace_backend != VCD_TRACE_BACKEND_LXT2_LAZY || !vcd->trace_session)
        return nullptr;
    return static_cast<Lxt2TraceSession*>(vcd->trace_session);
}

static bool lxt2_signal_range_covers(const signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return false;
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return sig->trace_loaded_t1 == TRACE_LOAD_T1_FULL;
    return sig->trace_loaded_t0 <= t0 && sig->trace_loaded_t1 >= t1;
}

static int lxt2_iter_changes(Lxt2TraceSession* sess, uint64_t t0, uint64_t t1)
{
    if (!sess || !sess->lt)
        return -1;
    sess->cancel.store(false, std::memory_order_relaxed);

    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        lxt2_rd_unlimit_time_range(sess->lt);
    else
        lxt2_rd_limit_time_range(sess->lt, static_cast<lxtint64_t>(t0), static_cast<lxtint64_t>(t1));

    Lxt2LoadCtx ctx;
    ctx.fac_map = sess->fac_map;
    ctx.cancel = &sess->cancel;

    lxt2_rd_set_max_block_mem_usage(sess->lt, (lxtint64_t)64 * 1024 * 1024);
    const int rc = lxt2_rd_iter_blocks(sess->lt, lxt2_value_change_cb, &ctx);
    lxt2_rd_unlimit_time_range(sess->lt);

    if (sess->cancel.load(std::memory_order_relaxed))
        return 1;
    return rc < 0 ? -1 : 0;
}

extern "C" void lxt2_trace_session_destroy(void* session)
{
    auto* sess = static_cast<Lxt2TraceSession*>(session);
    if (!sess)
        return;
    if (sess->lt)
        lxt2_rd_close(sess->lt);
    delete sess;
}

extern "C" void lxt2_loader_request_cancel(vcd_t* vcd)
{
    if (Lxt2TraceSession* sess = lxt2_session_from_vcd(vcd))
        sess->cancel.store(true, std::memory_order_relaxed);
}

extern "C" vcd_t* lxt2_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

#ifndef BEAR2WAVE_WITH_LXT2
    fprintf(stderr,
        "[LXT2] Bear2Wave built without BEAR2WAVE_WITH_LXT2. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    struct lxt2_rd_trace* lt = lxt2_rd_init(utf8_path);
    if (!lt) {
        fprintf(stderr, "[LXT2] lxt2_rd_init failed: %s\n", utf8_path);
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        lxt2_rd_close(lt);
        return nullptr;
    }

    strncpy(vcd->version, "Bear2Wave LXT2 loader", VCD_VERSION_SIZE - 1);
    vcd->version[VCD_VERSION_SIZE - 1] = '\0';
    apply_lxt2_timescale(lt, vcd);

    auto* sess = new Lxt2TraceSession();
    sess->lt = lt;
    sess->path = utf8_path;
    sess->start_time = static_cast<uint64_t>(lxt2_rd_get_start_time(lt));
    sess->end_time = static_cast<uint64_t>(lxt2_rd_get_end_time(lt));

    if (lxt2_build_hierarchy(lt, vcd, &sess->fac_map) < 0) {
        delete sess;
        vcd_free(vcd);
        return nullptr;
    }

    vcd->trace_session = sess;
    vcd->trace_backend = VCD_TRACE_BACKEND_LXT2_LAZY;
    vcd->trace_max_timestamp = sess->end_time;
    return vcd;
#endif
}

extern "C" int lxt2_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
#ifndef BEAR2WAVE_WITH_LXT2
    (void)vcd;
    (void)sigs;
    (void)count;
    (void)t0;
    (void)t1;
    return -1;
#else
    Lxt2TraceSession* sess = lxt2_session_from_vcd(vcd);
    if (!sess || !sigs || count == 0)
        return -1;

    uint64_t batch_t0 = UINT64_MAX;
    uint64_t batch_t1 = 0;
    std::vector<signal_t*> pending;

    for (size_t i = 0; i < count; ++i) {
        signal_t* sig = sigs[i];
        if (!sig || lxt2_signal_range_covers(sig, t0, t1))
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

    lxt2_rd_clr_fac_process_mask_all(sess->lt);
    for (signal_t* sig : pending) {
        const char* idp = sig->signal_id;
        if (strncmp(idp, "lxt2", 4) == 0)
            idp += 4;
        const long fac = std::strtol(idp, nullptr, 10);
        if (fac < 0 || fac >= static_cast<long>(sess->fac_map.size()))
            continue;
        lxt2_rd_set_fac_process_mask(sess->lt, static_cast<lxtint32_t>(fac));
    }

    const int rc = lxt2_iter_changes(sess, batch_t0, batch_t1);
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

extern "C" vcd_t* lxt2_read_to_vcd(const char* utf8_path)
{
#ifndef BEAR2WAVE_WITH_LXT2
    (void)utf8_path;
    fprintf(stderr,
        "[LXT2] Bear2Wave built without BEAR2WAVE_WITH_LXT2. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    vcd_t* vcd = lxt2_open_lazy(utf8_path);
    if (!vcd)
        return nullptr;

    std::vector<signal_t*> all;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        all.push_back(&n->signal);

    if (!all.empty() && lxt2_load_signals(vcd, all.data(), all.size(), TRACE_LOAD_T0_FULL, TRACE_LOAD_T1_FULL) != 0) {
        vcd_free(vcd);
        return nullptr;
    }
    return vcd;
#endif
}
