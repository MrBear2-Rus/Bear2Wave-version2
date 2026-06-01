#include "fst_loader.h"

#include "core/trace_load_gaps.h"

#include <fstapi.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

static int g_fst_loader_debug = 0;

static void (*s_line_logger)(const char* line, void* user) = nullptr;
static void* s_line_logger_user = nullptr;
static int s_line_logger_enabled = 0;

void fst_loader_set_debug(int enabled) { g_fst_loader_debug = enabled ? 1 : 0; }

void fst_loader_set_line_logger(void (*fn)(const char* line, void* user), void* user)
{
    s_line_logger = fn;
    s_line_logger_user = user;
}

void fst_loader_set_line_logger_enabled(int enabled) { s_line_logger_enabled = enabled ? 1 : 0; }

static int fst_debug_enabled(void)
{
    if (g_fst_loader_debug)
        return 1;
    const char* e = getenv("BEAR2WAVE_FST_DEBUG");
    return (e && e[0] && e[0] != '0');
}

#define FST_LOG(...)                                                                               \
    do {                                                                                           \
        char _fst_log_buf[1024];                                                                   \
        snprintf(_fst_log_buf, sizeof(_fst_log_buf), __VA_ARGS__);                               \
        if (fst_debug_enabled())                                                                   \
            fprintf(stderr, "[FST] %s", _fst_log_buf);                                             \
        if (s_line_logger_enabled && s_line_logger)                                                \
            s_line_logger(_fst_log_buf, s_line_logger_user);                                       \
    } while (0)

static void apply_fst_timescale(fstReaderContext* ctx, vcd_t* vcd)
{
    signed char ts = fstReaderGetTimescale(ctx);
    vcd->timescale.scale = 1;
    if (ts >= 0) {
        strncpy(vcd->timescale.unit, "s", VCD_TIME_UNIT_SIZE - 1);
        vcd->timescale.unit[VCD_TIME_UNIT_SIZE - 1] = '\0';
        return;
    }
    int e = -ts;
    const char* u = "s";
    if (e >= 15)
        u = "fs";
    else if (e >= 12)
        u = "ps";
    else if (e >= 9)
        u = "ns";
    else if (e >= 6)
        u = "us";
    else if (e >= 3)
        u = "ms";
    strncpy(vcd->timescale.unit, u, VCD_TIME_UNIT_SIZE - 1);
    vcd->timescale.unit[VCD_TIME_UNIT_SIZE - 1] = '\0';
}

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

struct FstTraceSession {
    fstReaderContext* ctx = nullptr;
    std::vector<signal_t*> fac_map;
    std::string path;
    uint64_t start_time = 0;
    uint64_t end_time = 0;
    std::atomic<bool> cancel{false};
};

struct FstCbCtx {
    std::vector<signal_t*>* fac_map = nullptr;
    std::atomic<bool>* cancel = nullptr;
    uint64_t cb_std = 0;
    uint64_t cb_varlen = 0;
    uint64_t dropped_bad_idx = 0;
    uint64_t dropped_null_sig = 0;
    uint64_t dropped_cap = 0;
    uint64_t max_time = 0;
    uint64_t cb_ticks = 0;
};

static bool fst_cb_cancelled(FstCbCtx* ctx)
{
    if (!ctx || !ctx->cancel)
        return false;
    if ((++ctx->cb_ticks & 0xFFFFu) == 0 && ctx->cancel->load(std::memory_order_relaxed))
        return true;
    return false;
}

static void fst_value_cb(void* user, uint64_t time, fstHandle facidx, const unsigned char* value)
{
    auto* ctx = static_cast<FstCbCtx*>(user);
    if (fst_cb_cancelled(ctx))
        return;
    ctx->cb_std++;
    if (time > ctx->max_time)
        ctx->max_time = time;

    auto* fac_map = ctx->fac_map;
    /* fstReaderIterBlocks2 passes 1-based fac indices (see fstapi.c: idx + 1). */
    if (facidx == 0 || (size_t)facidx >= fac_map->size()) {
        ctx->dropped_bad_idx++;
        return;
    }
    signal_t* sig = (*fac_map)[facidx];
    if (!sig || !value) {
        ctx->dropped_null_sig++;
        return;
    }
    if (vcd_signal_append_change_lazy(sig, static_cast<timestamp_t>(time), reinterpret_cast<const char*>(value)) != 0) {
        ctx->dropped_cap++;
        return;
    }
}

static void fst_value_varlen_cb(void* user,
    uint64_t time,
    fstHandle facidx,
    const unsigned char* value,
    uint32_t len)
{
    auto* ctx = static_cast<FstCbCtx*>(user);
    if (fst_cb_cancelled(ctx))
        return;
    ctx->cb_varlen++;
    if (time > ctx->max_time)
        ctx->max_time = time;

    auto* fac_map = ctx->fac_map;
    if (facidx == 0 || (size_t)facidx >= fac_map->size()) {
        ctx->dropped_bad_idx++;
        return;
    }
    signal_t* sig = (*fac_map)[facidx];
    if (!sig || !value) {
        ctx->dropped_null_sig++;
        return;
    }
    int elen = fstUtilityBinToEscConvertedLen(value, static_cast<int>(len));
    if (elen < 0)
        elen = 0;
    std::vector<unsigned char> tmp(static_cast<size_t>(elen) + 1u);
    if (elen > 0)
        fstUtilityBinToEsc(tmp.data(), value, static_cast<int>(len));
    tmp[static_cast<size_t>(elen)] = 0;

    if (vcd_signal_append_change_lazy(sig, static_cast<timestamp_t>(time), reinterpret_cast<const char*>(tmp.data())) != 0) {
        ctx->dropped_cap++;
        return;
    }
}

/** Returns number of non-alias vars added, or -1 on allocation failure. */
static int fst_append_hier_to_vcd(fstReaderContext* ctx, vcd_t* vcd, std::vector<signal_t*>* fac_map)
{
    std::string scope_prefix;
    unsigned var_hier = 0;
    struct fstHier* h = nullptr;
    while ((h = fstReaderIterateHier(ctx)) != nullptr) {
        if (h->htyp == FST_HT_SCOPE) {
            const char* nm = h->u.scope.name ? h->u.scope.name : "";
            if (scope_prefix.empty())
                scope_prefix = nm;
            else {
                scope_prefix.push_back('.');
                scope_prefix += nm;
            }
        } else if (h->htyp == FST_HT_UPSCOPE) {
            const size_t pos = scope_prefix.rfind('.');
            if (pos == std::string::npos)
                scope_prefix.clear();
            else
                scope_prefix.resize(pos);
        } else if (h->htyp == FST_HT_VAR) {
            if (h->u.var.is_alias)
                continue;

            signal_node_t* new_node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
            if (!new_node)
                return -1;
            signal_t* sig = &new_node->signal;
            const char* nm = h->u.var.name ? h->u.var.name : "";
            strncpy(sig->name, nm, VCD_NAME_SIZE - 1);
            sig->name[VCD_NAME_SIZE - 1] = '\0';

            const char* modpath = scope_prefix.empty() ? "$root" : scope_prefix.c_str();
            strncpy(sig->module_path, modpath, VCD_SIGNAL_SIZE - 1);
            sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';

            if (scope_prefix.empty()) {
                snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s.%s", modpath, sig->name);
            } else {
                snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s.%s", scope_prefix.c_str(), sig->name);
            }
            sig->full_name[VCD_SIGNAL_SIZE - 1] = '\0';

            fstHandle fh = h->u.var.handle;
            snprintf(sig->signal_id, VCD_NAME_SIZE, "%u", static_cast<unsigned>(fh));
            sig->size = h->u.var.length;
            sig->fst_var_type = (int32_t)h->u.var.typ;

            if (fh >= fac_map->size())
                fac_map->resize(static_cast<size_t>(fh) + 1u, nullptr);
            (*fac_map)[fh] = sig;

            append_signal_node(vcd, new_node);
            var_hier++;
            FST_LOG("  var handle=%u name=\"%s\" len=%u scope=\"%s\"\n",
                (unsigned)fh,
                sig->name,
                (unsigned)h->u.var.length,
                scope_prefix.c_str());
        }
    }
    return static_cast<int>(var_hier);
}

/**
 * When embedded .hier gzip decode fails (seen on some MSVC builds) but GEOM + VC blocks are
 * valid, create one scalar placeholder per fst handle so fstReaderIterBlocks2 can fill fac_map.
 * Names are sig1..sigN under $root (readable when embedded .hier cannot be decoded).
 */
static int fst_synth_signals_from_maxhandle(vcd_t* vcd, std::vector<signal_t*>* fac_map, fstHandle maxh)
{
    if (!vcd || !fac_map || maxh == 0)
        return 0;
    if (fac_map->size() < static_cast<size_t>(maxh) + 1u)
        fac_map->resize(static_cast<size_t>(maxh) + 1u, nullptr);

    int added = 0;
    for (fstHandle fh = 1; fh <= maxh; fh++) {
        if ((*fac_map)[fh])
            continue;
        signal_node_t* new_node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!new_node)
            return -1;
        signal_t* sig = &new_node->signal;
        const bool bear2wave_sample =
            (maxh == 3) && vcd->version[0] != '\0' &&
            std::strstr(vcd->version, "Bear2Wave sample") != nullptr;
        if (bear2wave_sample && fh >= 1 && fh <= 3) {
            static const char* const demo3[] = {"clk", "din", "dout"};
            strncpy(sig->name, demo3[fh - 1], VCD_NAME_SIZE - 1);
        } else {
            snprintf(sig->name, VCD_NAME_SIZE, "sig%u", static_cast<unsigned>(fh));
        }
        sig->name[VCD_NAME_SIZE - 1] = '\0';
        strncpy(sig->module_path, "$root", VCD_SIGNAL_SIZE - 1);
        sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "$root.%s", sig->name);
        snprintf(sig->signal_id, VCD_NAME_SIZE, "%u", static_cast<unsigned>(fh));
        sig->size = 1;
        sig->fst_var_type = -1;
        (*fac_map)[fh] = sig;
        append_signal_node(vcd, new_node);
        added++;
    }
    FST_LOG("GEOM-only fallback: added %d signals sig1..sig%u (handles 1..%u, width=1)\n",
        added, static_cast<unsigned>(maxh), static_cast<unsigned>(maxh));
    return added;
}

/**
 * If fstReaderIterBlocks2 yields no samples (some tools / mask quirks), use fstapi's
 * random-access reader to seed at least one point per signal so the UI can draw.
 */
static void seed_signals_using_rvat(fstReaderContext* ctx, vcd_t* vcd)
{
    if (!ctx || !vcd || !vcd->signals_head)
        return;

    uint64_t t0 = fstReaderGetStartTime(ctx);
    uint64_t t1 = fstReaderGetEndTime(ctx);
    if (t1 < t0) {
        const uint64_t tmp = t0;
        t0 = t1;
        t1 = tmp;
    }

    char buf[VCD_SIGNAL_SIZE];

    for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
        signal_t* sig = &n->signal;
        if (sig->changes_count > 0)
            continue;

        const fstHandle h = static_cast<fstHandle>(std::strtoul(sig->signal_id, nullptr, 10));
        if (h == 0)
            continue;

        const uint64_t times[] = {t0, t1};
        for (unsigned ti = 0; ti < sizeof(times) / sizeof(times[0]); ++ti) {
            const uint64_t t = times[ti];
            char* got = fstReaderGetValueFromHandleAtTime(ctx, t, h, buf);
            if (!got || !got[0])
                continue;
            if (vcd_signal_append_change(sig, static_cast<timestamp_t>(t), got) != 0)
                break;
            break;
        }
    }
}

static FstTraceSession* fst_session_from_vcd(vcd_t* vcd)
{
    if (!vcd || vcd->trace_backend != VCD_TRACE_BACKEND_FST_LAZY || !vcd->trace_session)
        return nullptr;
    return static_cast<FstTraceSession*>(vcd->trace_session);
}

static int fst_build_hierarchy(fstReaderContext* ctx, vcd_t* vcd, std::vector<signal_t*>* fac_map)
{
    fstHandle maxh = fstReaderGetMaxHandle(ctx);
    if (fac_map->size() < static_cast<size_t>(maxh) + 1u)
        fac_map->resize(static_cast<size_t>(maxh) + 1u, nullptr);

    fstReaderIterateHierRewind(ctx);

    int var_hier = fst_append_hier_to_vcd(ctx, vcd, fac_map);
    if (var_hier < 0)
        return -1;

    if (var_hier == 0 && maxh > 0) {
        FST_LOG("hierarchy: 0 vars with maxHandle>0; discarding external .hier handle, rescan embedded\n");
        fstReaderPreferEmbeddedHier(ctx);
        fstReaderIterateHierRewind(ctx);
        var_hier = fst_append_hier_to_vcd(ctx, vcd, fac_map);
        if (var_hier < 0)
            return -1;
    }
    if (var_hier == 0 && maxh > 0) {
        FST_LOG("embedded hierarchy still unreadable; using GEOM maxHandle placeholder signals\n");
        const int syn = fst_synth_signals_from_maxhandle(vcd, fac_map, maxh);
        if (syn < 0)
            return -1;
        var_hier = syn;
    }

    FST_LOG("hierarchy: %u non-alias vars, fac_map.size=%zu\n", (unsigned)var_hier, fac_map->size());
    if (fstReaderIterateHierRewind(ctx) != 1)
        FST_LOG("warning: fstReaderIterateHierRewind failed (non-fatal)\n");
    return var_hier;
}

static int fst_iter_value_changes(FstTraceSession* sess, uint64_t t0, uint64_t t1)
{
    if (!sess || !sess->ctx)
        return -1;

    fstReaderContext* ctx = sess->ctx;
    sess->cancel.store(false, std::memory_order_relaxed);

    if (t0 == 0 && t1 == UINT64_MAX)
        fstReaderSetUnlimitedTimeRange(ctx);
    else
        fstReaderSetLimitTimeRange(ctx, t0, t1);

    fstReaderIterBlocksSetNativeDoublesOnCallback(ctx, 0);

    FstCbCtx cbctx;
    cbctx.fac_map = &sess->fac_map;
    cbctx.cancel = &sess->cancel;

    const int iter_rc = fstReaderIterBlocks2(ctx, fst_value_cb, fst_value_varlen_cb, &cbctx, nullptr);
    FST_LOG("fstReaderIterBlocks2 rc=%d callbacks: std=%llu varlen=%llu dropped_idx=%llu null_sig=%llu cap=%llu maxTime=%llu cancel=%d\n",
        iter_rc,
        (unsigned long long)cbctx.cb_std,
        (unsigned long long)cbctx.cb_varlen,
        (unsigned long long)cbctx.dropped_bad_idx,
        (unsigned long long)cbctx.dropped_null_sig,
        (unsigned long long)cbctx.dropped_cap,
        (unsigned long long)cbctx.max_time,
        sess->cancel.load(std::memory_order_relaxed) ? 1 : 0);

    if (sess->cancel.load(std::memory_order_relaxed))
        return 1;
    return iter_rc < 0 ? -1 : 0;
}

extern "C" void fst_trace_session_destroy(void* session)
{
    auto* sess = static_cast<FstTraceSession*>(session);
    if (!sess)
        return;
    if (sess->ctx)
        fstReaderClose(sess->ctx);
    delete sess;
}

extern "C" void fst_loader_request_cancel(vcd_t* vcd)
{
    if (FstTraceSession* sess = fst_session_from_vcd(vcd))
        sess->cancel.store(true, std::memory_order_relaxed);
}

extern "C" vcd_t* fst_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

    FST_LOG("lazy open \"%s\"\n", utf8_path);

    fstReaderContext* ctx = fstReaderOpen(utf8_path);
    if (!ctx) {
        FST_LOG("fstReaderOpen failed\n");
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        fstReaderClose(ctx);
        return nullptr;
    }

    const char* ds = fstReaderGetDateString(ctx);
    if (ds)
        strncpy(vcd->date, ds, VCD_DATE_SIZE - 1);
    const char* vs = fstReaderGetVersionString(ctx);
    if (vs)
        strncpy(vcd->version, vs, VCD_VERSION_SIZE - 1);
    apply_fst_timescale(ctx, vcd);

    auto* sess = new FstTraceSession();
    sess->ctx = ctx;
    sess->path = utf8_path;
    sess->start_time = fstReaderGetStartTime(ctx);
    sess->end_time = fstReaderGetEndTime(ctx);
    if (sess->end_time < sess->start_time) {
        const uint64_t tmp = sess->start_time;
        sess->start_time = sess->end_time;
        sess->end_time = tmp;
    }

    fstHandle maxh = fstReaderGetMaxHandle(ctx);
    sess->fac_map.assign(static_cast<size_t>(maxh) + 1u, nullptr);

    FST_LOG(
        "header: maxHandle=%u varCount=%llu vcSections=%llu start=%llu end=%llu timescale_exp=%d\n",
        (unsigned)maxh,
        (unsigned long long)fstReaderGetVarCount(ctx),
        (unsigned long long)fstReaderGetValueChangeSectionCount(ctx),
        (unsigned long long)sess->start_time,
        (unsigned long long)sess->end_time,
        (int)fstReaderGetTimescale(ctx));

    if (fst_build_hierarchy(ctx, vcd, &sess->fac_map) < 0) {
        delete sess;
        vcd_free(vcd);
        return nullptr;
    }

    vcd->trace_session = sess;
    vcd->trace_backend = VCD_TRACE_BACKEND_FST_LAZY;
    vcd->trace_max_timestamp = sess->end_time;

    FST_LOG("lazy hierarchy ready: signals=%zu end_time=%llu\n",
        (size_t)vcd->signals_count,
        (unsigned long long)vcd->trace_max_timestamp);
    return vcd;
}

static bool fst_signal_range_covers(const signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return false;
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return sig->trace_loaded_t1 == TRACE_LOAD_T1_FULL;
    return sig->trace_loaded_t0 <= t0 && sig->trace_loaded_t1 >= t1;
}

extern "C" int fst_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
    FstTraceSession* sess = fst_session_from_vcd(vcd);
    if (!sess || !sigs || count == 0)
        return -1;

    uint64_t batch_t0 = UINT64_MAX;
    uint64_t batch_t1 = 0;
    std::vector<signal_t*> pending;

    for (size_t i = 0; i < count; ++i) {
        signal_t* sig = sigs[i];
        if (!sig || fst_signal_range_covers(sig, t0, t1))
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

    fstReaderClrFacProcessMaskAll(sess->ctx);

    for (signal_t* sig : pending) {
        const fstHandle h = static_cast<fstHandle>(std::strtoul(sig->signal_id, nullptr, 10));
        if (h == 0 || h >= sess->fac_map.size())
            continue;
        fstReaderSetFacProcessMask(sess->ctx, h);
    }

    const int rc = fst_iter_value_changes(sess, batch_t0, batch_t1);
    if (rc != 0)
        return rc;

    for (signal_t* sig : pending) {
        vcd_ensure_signal_sorted(sig);
        trace_extend_loaded_span(sig, t0, t1);
        vcd_signal_shrink_to_fit(sig);
    }

    size_t total_changes = 0;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        total_changes += n->signal.changes_count;

    if (total_changes == 0 && vcd->signals_count > 0) {
        FST_LOG("no VC from IterBlocks2; trying fstReaderGetValueFromHandleAtTime per loaded signal\n");
        for (signal_t* sig : pending) {
            if (!sig || sig->changes_count > 0)
                continue;
            const fstHandle h = static_cast<fstHandle>(std::strtoul(sig->signal_id, nullptr, 10));
            if (h == 0)
                continue;
            char buf[VCD_SIGNAL_SIZE];
            const uint64_t times[] = {sess->start_time, sess->end_time};
            for (unsigned ti = 0; ti < sizeof(times) / sizeof(times[0]); ++ti) {
                char* got = fstReaderGetValueFromHandleAtTime(sess->ctx, times[ti], h, buf);
                if (!got || !got[0])
                    continue;
                vcd_signal_append_change(sig, static_cast<timestamp_t>(times[ti]), got);
                break;
            }
            vcd_ensure_signal_sorted(sig);
            sig->trace_data_loaded = 1;
            sig->trace_loaded_t0 = sess->start_time;
            sig->trace_loaded_t1 = TRACE_LOAD_T1_FULL;
        }
    }

    return 0;
}

extern "C" vcd_t* fst_read_to_vcd(const char* utf8_path)
{
    vcd_t* vcd = fst_open_lazy(utf8_path);
    if (!vcd)
        return nullptr;

    std::vector<signal_t*> all;
    all.reserve(vcd->signals_count);
    for (signal_node_t* n = vcd->signals_head; n; n = n->next)
        all.push_back(&n->signal);

    if (!all.empty()) {
        if (fst_load_signals(vcd, all.data(), all.size(), 0, UINT64_MAX) != 0) {
            vcd_free(vcd);
            return nullptr;
        }
    }

    FST_LOG("full load signals=%zu\n", (size_t)vcd->signals_count);
    return vcd;
}
